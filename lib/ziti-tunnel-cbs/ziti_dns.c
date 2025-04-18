/*
 Copyright NetFoundry Inc.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 https://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

#include <ziti/ziti_tunnel.h>
#include <ziti/ziti_log.h>
#include <ziti/ziti_dns.h>
#include "ziti_instance.h"
#include "dns_host.h"

#define MAX_UPSTREAMS 5
#define MAX_DNS_NAME 256
#define MAX_IP_LENGTH 16

#ifndef IN6ADDR_V4MAPPED
#define IN6ADDR_V4MAPPED(v4) \
	{{{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	    0x00, 0x00, 0xff, 0xff, v4[0], v4[1], v4[2], v4[3] }}}
#endif

enum ns_q_type {
    NS_T_A = 1,
    NS_T_AAAA = 28,
    NS_T_MX = 15,
    NS_T_TXT = 16,
    NS_T_SRV = 33,
};

typedef struct ziti_dns_client_s {
    io_ctx_t *io_ctx;
    bool is_tcp;
    model_map active_reqs; // dns_reqs keyed by address of ID.
} ziti_dns_client_t;

struct dns_req {
    uint16_t id;
    size_t req_len;
    uint8_t req[4096];
    size_t resp_len;
    uint8_t resp[4096];

    dns_message msg;

    struct in_addr addr;

    uint8_t *rp;

    ziti_dns_client_t *clt;
};

static void* on_dns_client(const void *app_intercept_ctx, io_ctx_t *io);
static int on_dns_close(void *dns_io_ctx);
static ssize_t on_dns_req(const void *ziti_io_ctx, void *write_ctx, const void *q_packet, size_t len);
static int query_upstream(struct dns_req *req);
static void dns_upstream_alloc(uv_handle_t *h, size_t reqlen, uv_buf_t *b);
static void on_upstream_packet(uv_udp_t *h, ssize_t rc, const uv_buf_t *buf, const struct sockaddr* addr, unsigned int flags);
static void complete_dns_req(struct dns_req *req);
static void free_dns_req(struct dns_req *req);

typedef struct dns_domain_s {
    char name[MAX_DNS_NAME];

    model_map intercepts; // set[intercept]

    ziti_connection resolv_proxy;

} dns_domain_t;

// hostname or domain
typedef struct dns_entry_s {
    char name[MAX_DNS_NAME];
    char ip[MAX_IP_LENGTH];
    ip_addr_t addr;
    dns_domain_t *domain;

    model_map intercepts;

} dns_entry_t;

struct ziti_dns_s {

    struct {
        uint32_t base;
        uint32_t counter;
        uint32_t counter_mask;
        uint32_t capacity;
    } ip_pool;

    // map[hostname -> dns_entry_t]
    model_map hostnames;

    // map[ip4_addr_t -> dns_entry_t]
    model_map ip_addresses;

    // map[domain -> dns_domain_t]
    model_map domains;

    uv_loop_t *loop;
    tunneler_context tnlr;

    model_map requests;
    uv_udp_t upstream;
    bool is_ipv4;
    int num_dns_up;
    struct sockaddr_in6 upstream_addr[MAX_UPSTREAMS];
} ziti_dns;

static uint32_t next_ipv4() {
    uint32_t candidate;
    uint32_t i = 0; // track how many candidates have been considered. should never exceed pool capacity.

    if (model_map_size(&ziti_dns.ip_addresses) == ziti_dns.ip_pool.capacity) {
        ZITI_LOG(ERROR, "DNS ip pool exhausted (%u IPs). Try rerunning with larger DNS range.",
                 ziti_dns.ip_pool.capacity);
        return INADDR_NONE;
    }

    do {
        candidate = htonl(ziti_dns.ip_pool.base | (ziti_dns.ip_pool.counter++ & ziti_dns.ip_pool.counter_mask));
        i += 1;
        if (ziti_dns.ip_pool.counter == ziti_dns.ip_pool.counter_mask) {
            ziti_dns.ip_pool.counter = 1;
        }
    } while ((model_map_getl(&ziti_dns.ip_addresses, candidate) != NULL) && i < ziti_dns.ip_pool.capacity);

    if (i == ziti_dns.ip_pool.capacity) {
        ZITI_LOG(ERROR, "no IPs available after scanning entire pool");
        return INADDR_NONE;
    }

    return candidate;
}

static int seed_dns(const char *dns_cidr) {
    uint32_t ip[4];
    uint32_t bits;
    int rc = sscanf(dns_cidr, "%d.%d.%d.%d/%d", &ip[0], &ip[1], &ip[2], &ip[3], &bits);
    if (rc != 5 || ip[0] > 255 || ip[1] > 255 || ip[2] > 255 || ip[3] > 255 || bits > 32) {
        ZITI_LOG(ERROR, "Invalid IP range specification: n.n.n.n/m format is expected");
        return -1;
    }
    uint32_t mask = 0;
    for (int i = 0; i < 4; i++) {
        mask <<= 8U;
        mask |= (ip[i] & 0xFFU);
    }

    ziti_dns.ip_pool.counter_mask = ~( (uint32_t)-1 << (32 - (uint32_t)bits));
    ziti_dns.ip_pool.base = mask & ~ziti_dns.ip_pool.counter_mask;

    ziti_dns.ip_pool.counter = 1;
    ziti_dns.ip_pool.capacity = (1 << (32 - bits)) - 2; // subtract 2 for network and broadcast IPs

    union ip_bits {
        uint8_t b[4];
        uint32_t ip;
    } min_ip, max_ip;

    min_ip.ip = htonl(ziti_dns.ip_pool.base);
    max_ip.ip = htonl(ziti_dns.ip_pool.base | ziti_dns.ip_pool.counter_mask);
    ZITI_LOG(INFO, "DNS configured with range %d.%d.%d.%d - %d.%d.%d.%d (%u ips)",
             min_ip.b[0],min_ip.b[1],min_ip.b[2],min_ip.b[3],
             max_ip.b[0],max_ip.b[1],max_ip.b[2],max_ip.b[3], ziti_dns.ip_pool.capacity
             );

    return 0;
}

int ziti_dns_setup(tunneler_context tnlr, const char *dns_addr, const char *dns_cidr) {
    ziti_dns.tnlr = tnlr;
    seed_dns(dns_cidr);

    intercept_ctx_t *dns_intercept = intercept_ctx_new(tnlr, "ziti:dns-resolver", &ziti_dns);
    ziti_address dns_zaddr, tun_zaddr;
    ziti_address_from_string(&dns_zaddr, dns_addr);
    intercept_ctx_add_address(dns_intercept, &dns_zaddr);
    intercept_ctx_add_port_range(dns_intercept, 53, 53);
    intercept_ctx_add_protocol(dns_intercept, "udp");
    intercept_ctx_override_cbs(dns_intercept, on_dns_client, on_dns_req, on_dns_close, on_dns_close);
    ziti_tunneler_intercept(tnlr, dns_intercept);

    // reserve tun and dns ips by adding to ip_addresses with empty dns entries
    ziti_address_from_string(&tun_zaddr, dns_cidr); // assume tun ip is first in dns_cidr
    ziti_address *reserved[] = { &tun_zaddr, &dns_zaddr };
    size_t n = sizeof(reserved) / sizeof(ziti_address *);
    for (int i = 0; i < n; i++) {
        struct in_addr *in4_p = (struct in_addr *) &reserved[i]->addr.cidr.ip;
        model_map_setl(&ziti_dns.ip_addresses, in4_p->s_addr, calloc(1, sizeof(dns_entry_t)));
    }
    return 0;
}

#define CHECK_UV(op) do{ int rc = (op); if (rc < 0) {\
ZITI_LOG(ERROR, "failed [" #op "]: %d(%s)", rc, uv_strerror(rc)); \
return rc;} \
}while(0)

int ziti_dns_set_upstream(uv_loop_t *l, tunnel_upstream_dns_array upstreams) {
    if (!uv_is_active((const uv_handle_t *) &ziti_dns.upstream)) {
        CHECK_UV(uv_udp_init(l, &ziti_dns.upstream));
        int r = uv_udp_bind(&ziti_dns.upstream,
                            (const struct sockaddr *) &(struct sockaddr_in6){
                                    .sin6_family = AF_INET6,
                                    .sin6_addr = in6addr_any,
                            }, 0);
        if (r != 0) {
            ZITI_LOG(WARN, "failed to bind upstream socket to IPv6 address: %s", uv_strerror(r));
            r = uv_udp_bind(&ziti_dns.upstream,
                            (const struct sockaddr *) &(struct sockaddr_in){
                                    .sin_family = AF_INET,
                                    .sin_addr = INADDR_ANY,
                            }, 0);
            if (r != 0) {
                ZITI_LOG(WARN, "failed to bind upstream socket to IPv4 address: %s", uv_strerror(r));
                return r;
            }
            ziti_dns.is_ipv4 = true;
        }
        CHECK_UV(uv_udp_recv_start(&ziti_dns.upstream, dns_upstream_alloc, on_upstream_packet));
        uv_unref((uv_handle_t *) &ziti_dns.upstream);
    }

    union {
        struct in_addr addr;
        uint8_t a[4];
    } ipv4;

    int idx = 0;
    for (int i = 0; upstreams[i] != NULL && idx < MAX_UPSTREAMS; i++) {
        const tunnel_upstream_dns *dns = upstreams[i];
        int port = dns->port != 0 ? (int)dns->port : 53;

        if (ziti_dns.is_ipv4) {
            if (uv_inet_pton(AF_INET, dns->host, &ipv4) == 0) {
                ((struct sockaddr_in *) &ziti_dns.upstream_addr[idx])->sin_family = AF_INET;
                ((struct sockaddr_in *) &ziti_dns.upstream_addr[idx])->sin_addr = ipv4.addr;
                ((struct sockaddr_in *) &ziti_dns.upstream_addr[idx])->sin_port = htons(port);
                idx++;
            } else {
                ZITI_LOG(WARN, "cannot set non-IPv4 upstream on IPv4 only socket");
            }
        } else {
            // set IPv6 upstream address, mapping IPv4 target to IPv6 space (if needed)
            ziti_dns.upstream_addr[idx].sin6_family = AF_INET6;
            ziti_dns.upstream_addr[idx].sin6_port = htons(port);
            if (uv_inet_pton(AF_INET6, dns->host, &ziti_dns.upstream_addr[idx].sin6_addr) != 0) {
                if (uv_inet_pton(AF_INET, dns->host, &ipv4) == 0) {
                    ziti_dns.upstream_addr[idx].sin6_addr = (struct in6_addr) IN6ADDR_V4MAPPED(ipv4.a);
                } else {
                    ZITI_LOG(WARN, "upstream address[%s] is not IP format", dns->host);
                    char port_str[6];
                    snprintf(port_str, sizeof(port_str), "%hu", port);
                    uv_getaddrinfo_t req = {0};
                    if(uv_getaddrinfo(l, &req, NULL, dns->host, port_str, NULL) == 0) {
                        memcpy(&ziti_dns.upstream_addr[idx], req.addrinfo->ai_addr, req.addrinfo->ai_addrlen);
                    }
                }
            }
            idx++;
        }
        ZITI_LOG(INFO, "DNS upstream[%d] is set to %s:%hu", idx, dns->host, port);
    }
    ziti_dns.num_dns_up = idx;
    return 0;
}


void* on_dns_client(const void *app_intercept_ctx, io_ctx_t *io) {
    ZITI_LOG(TRACE, "new DNS client");
    ziti_dns_client_t *clt = calloc(1, sizeof(ziti_dns_client_t));
    io->ziti_io = clt;
    clt->io_ctx = io;
    ziti_tunneler_set_idle_timeout(io, 5000); // 5 seconds
    ziti_tunneler_dial_completed(io, true);
    return clt;
}

static void remove_dns_req(void *p) {
    struct dns_req *req = p;
    if (req) {
        model_map_remove_key(&ziti_dns.requests, &req->id, sizeof(req->id));
        free_dns_req(req);
    }
}

int on_dns_close(void *dns_io_ctx) {
    ZITI_LOG(TRACE, "DNS client close");
    ziti_dns_client_t *clt = dns_io_ctx;
    // we may be here due to udp timeout, and reqs may have been sent to upstream.
    // remove reqs from ziti_dns to prevent completion (with invalid io_ctx) if upstream should respond after udp timeout.
    model_map_clear(&clt->active_reqs, remove_dns_req);
    ziti_tunneler_close(clt->io_ctx->tnlr_io);
    free(clt->io_ctx);
    free(dns_io_ctx);
    return 0;
}

static bool check_name(const char *name, char clean_name[MAX_DNS_NAME], bool *is_domain) {
    const char *hp = name;
    char *p = clean_name;

    if (*hp == '*' && *(hp + 1) == '.') {
        if (is_domain) *is_domain = true;
        *p++ = '*';
        *p++ = '.';
        hp += 2;
    } else {
        if (is_domain) *is_domain = false;
    }

    bool success = true;
    while (*hp != '\0') {
        *p++ = (char) tolower(*hp++);
        if (p - clean_name >= MAX_DNS_NAME) {
            p = clean_name;
            success = false;
            break;
        }
    }
    *p = '\0';
    return success;
}

static dns_entry_t* new_ipv4_entry(const char *host) {
    dns_entry_t *entry = calloc(1, sizeof(dns_entry_t));
    strncpy(entry->name, host, sizeof(entry->name));
    uint32_t next = next_ipv4();
    if (next == INADDR_NONE) {
        return NULL;
    }

    ip_addr_set_ip4_u32(&entry->addr, next);
    ipaddr_ntoa_r(&entry->addr, entry->ip, sizeof(entry->ip));

    model_map_set(&ziti_dns.hostnames, host, entry);
    model_map_setl(&ziti_dns.ip_addresses, ip_2_ip4(&entry->addr)->addr, entry);
    ZITI_LOG(INFO, "registered DNS entry %s -> %s", host, entry->ip);

    return entry;
}

const char *ziti_dns_reverse_lookup_domain(const ip_addr_t *addr) {
     dns_entry_t *entry = model_map_getl(&ziti_dns.ip_addresses, ip_2_ip4(addr)->addr);
     if (entry && entry->domain) {
         return entry->domain->name;
     }
     return NULL;
}

const char *ziti_dns_reverse_lookup(const char *ip_addr) {
    ip_addr_t addr = {0};
    ipaddr_aton(ip_addr, &addr);
    dns_entry_t *entry = model_map_getl(&ziti_dns.ip_addresses, ip_2_ip4(&addr)->addr);

    return entry ? entry->name : NULL;
}

static dns_domain_t* find_domain(const char *hostname) {
    char *dot = strchr(hostname, '.');
    dns_domain_t *domain = model_map_get(&ziti_dns.domains, hostname);
    while (dot != NULL && domain == NULL) {
        domain = model_map_get(&ziti_dns.domains, dot + 1);
        dot = strchr(dot + 1, '.');
    }
    return domain;
}

static dns_entry_t *ziti_dns_lookup(const char *hostname) {
    char clean[MAX_DNS_NAME];
    bool is_wildcard;
    if (!check_name(hostname, clean, &is_wildcard) || is_wildcard) {
        ZITI_LOG(WARN, "invalid host lookup[%s]", hostname);
        return NULL;
    }

    dns_entry_t *entry = model_map_get(&ziti_dns.hostnames, clean);

    if (!entry) {         // try domains
        dns_domain_t *domain = find_domain(clean);

        if (domain && model_map_size(&domain->intercepts) > 0) {
            ZITI_LOG(DEBUG, "matching domain[%s] found for %s", domain->name, hostname);
            entry = new_ipv4_entry(clean);
            if (entry) {
                entry->domain = domain;
            }
        }
    }

    if (entry) {
        if (model_map_size(&entry->intercepts) > 0 ||
            (entry->domain && model_map_size(&entry->domain->intercepts) > 0)) {
            return entry;
        } else {
            return NULL; // inactive entry
        }
    }
    return entry;
}


void ziti_dns_deregister_intercept(void *intercept) {
    model_map_iter it = model_map_iterator(&ziti_dns.domains);
    while (it != NULL) {
        dns_domain_t *domain = model_map_it_value(it);
        model_map_remove_key(&domain->intercepts, &intercept, sizeof(intercept));
        it = model_map_it_next(it);
    }

    it = model_map_iterator(&ziti_dns.hostnames);
    while (it != NULL) {
        dns_entry_t *e = model_map_it_value(it);
        model_map_remove_key(&e->intercepts, &intercept, sizeof(intercept));
        if (model_map_size(&e->intercepts) == 0 && (e->domain == NULL || model_map_size(&e->domain->intercepts) == 0)) {
            it = model_map_it_remove(it);
            model_map_removel(&ziti_dns.ip_addresses, ip_2_ip4(&e->addr)->addr);
            ZITI_LOG(DEBUG, "%zu active hostnames mapped to %zu IPs", model_map_size(&ziti_dns.hostnames), model_map_size(&ziti_dns.ip_addresses));
            ZITI_LOG(INFO, "DNS mapping %s -> %s is now inactive", e->name, e->ip);
        } else {
            it = model_map_it_next(it);
        }
    }

    it = model_map_iterator(&ziti_dns.domains);
    while (it != NULL) {
        dns_domain_t *domain = model_map_it_value(it);
        if (model_map_size(&domain->intercepts) == 0) {
            it = model_map_it_remove(it);
            ZITI_LOG(INFO, "wildcard domain[*%s] is now inactive", domain->name);
        } else {
            it = model_map_it_next(it);
        }
    }
}

const ip_addr_t *ziti_dns_register_hostname(const ziti_address *addr, void *intercept) {
    // IP or CIDR block
    if (addr->type == ziti_address_cidr) {
        return NULL;
    }

    const char *hostname = addr->addr.hostname;
    char clean[MAX_DNS_NAME];
    bool is_domain = false;

    if (!check_name(hostname, clean, &is_domain)) {
        ZITI_LOG(ERROR, "invalid hostname[%s]", hostname);
    }

    if (is_domain) {
        dns_domain_t *domain = model_map_get(&ziti_dns.domains, clean + 2);
        if (domain == NULL) {
            ZITI_LOG(INFO, "registered wildcard domain[%s]", clean);
            domain = calloc(1, sizeof(dns_domain_t));
            strncpy(domain->name, clean, sizeof(domain->name));
            model_map_set(&ziti_dns.domains, clean + 2, domain);
        }
        model_map_set_key(&domain->intercepts, &intercept, sizeof(intercept), intercept);
        return NULL;
    } else {
        dns_entry_t *entry = model_map_get(&ziti_dns.hostnames, clean);
        if (!entry) {
            entry = new_ipv4_entry(clean);
        }
        if (entry) {
            model_map_set_key(&entry->intercepts, &intercept, sizeof(intercept), intercept);
            return &entry->addr;
        } else {
            return NULL;
        }
    }
}

static const char DNS_OPT[] = { 0x0, 0x0, 0x29, 0x10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

#define DNS_HEADER_LEN 12
#define DNS_ID(p) ((uint8_t)(p)[0] << 8 | (uint8_t)(p)[1])
#define DNS_FLAGS(p) ((p)[2] << 8 | (p)[3])
#define DNS_QRS(p) ((p)[4] << 8 | (p)[5])
#define DNS_QR(p) ((p) + 12)
#define DNS_RD(p) ((p)[2] & 0x1)

#define DNS_SET_RA(p) ((p)[3] = (p)[3] | 0x80)
#define DNS_SET_TC(p) ((p)[2] = (p)[2] | 0x2)
#define DNS_SET_CODE(p,c) ((p)[3] = (p)[3] | ((c) & 0xf))
#define DNS_SET_ANS(p) ((p)[2] = (p)[2] | 0x80)
#define DNS_SET_ARS(p,n) do{ (p)[6] = (n) >> 8; (p)[7] = (n) & 0xff; } while(0)
#define DNS_SET_AARS(p,n) do{ (p)[10] = (n) >> 8; (p)[11] = (n) & 0xff; } while(0)

#define SET_U8(p,v) *(p)++ = (v) & 0xff
#define SET_U16(p,v) (*(p)++ = ((v) >> 8) & 0xff),*(p)++ = (v) & 0xff
#define SET_U32(p,v) (*(p)++ = ((v) >> 24) & 0xff), \
(*(p)++ = ((v)>>16) & 0xff),                          \
(*(p)++ = ((v) >> 8) & 0xff),                       \
*(p)++ = (v) & 0xff

#define IS_QUERY(flags) (((flags) & (1 << 15)) == 0)

static uint8_t* format_name(uint8_t* p, const char* name) {
    const char *np = name;
    do {
        const char *dot = strchr(np, '.');
        uint8_t len = dot ? dot - np : strlen(np);

        *p++ = len;
        if (len == 0) break;

        memcpy(p, np, len);
        p += len;

        if (dot == NULL) {
            *p++ = 0;
            break;
        } else {
            np = dot + 1;
        }
    } while(1);
    return p;
}

static void format_resp(struct dns_req *req) {

    // copy header from request
    memcpy(req->resp, req->req, DNS_HEADER_LEN); // DNS header
    DNS_SET_ANS(req->resp);
    DNS_SET_CODE(req->resp, req->msg.status);
    bool recursion_avail = uv_is_active((const uv_handle_t *) &ziti_dns.upstream);
    if (recursion_avail) {
        DNS_SET_RA(req->resp);
    }

    size_t query_section_len = strlen(req->msg.question[0]->name) + 2 + 4;
    memcpy(req->resp + DNS_HEADER_LEN, req->req + DNS_HEADER_LEN, query_section_len);

    uint8_t *rp = req->resp + DNS_HEADER_LEN + query_section_len;
    uint8_t *resp_end = req->resp + sizeof(req->resp);
    bool truncated = false;

    if (req->msg.status == DNS_NO_ERROR && req->msg.answer != NULL) {
        int ans_count = 0;
        for (int i = 0; req->msg.answer[i] != NULL; i++) {
            ans_count++;
            dns_answer *a = req->msg.answer[i];

            if (resp_end - rp < 10) { // 2 bytes for name ref, 2 for type, 2 for class, and 4 for ttl
                truncated = true;
                goto done;
            }

            // name ref
            *rp++ = 0xc0;
            *rp++ = 0x0c;

            ZITI_LOG(INFO, "found record[%s] for query[%d:%s]", a->data,
                     (int)req->msg.question[0]->type, req->msg.question[0]->name);

            SET_U16(rp, a->type);
            SET_U16(rp, 1); // class IN
            SET_U32(rp, a->ttl);

            switch (a->type) {
                case NS_T_A: {
                    if (resp_end - rp < (2 + sizeof(req->addr.s_addr))) {
                        truncated = true;
                        goto done;
                    }
                    SET_U16(rp, sizeof(req->addr.s_addr));
                    memcpy(rp, &req->addr.s_addr, sizeof(req->addr.s_addr));
                    rp += sizeof(req->addr.s_addr);
                    break;
                }

                case NS_T_TXT: {
                    uint16_t txtlen = strlen(a->data);
                    uint16_t datalen = 1 + txtlen;
                    if (resp_end - rp < (3 + txtlen)) {
                        truncated = true;
                        goto done;
                    }
                    SET_U16(rp, datalen);
                    SET_U8(rp, txtlen);
                    memcpy(rp, a->data, txtlen);
                    rp += txtlen;
                    break;
                }
                case NS_T_MX: {
                    uint8_t *hold = rp;
                    rp += 2;
                    uint16_t datalen_est = strlen(a->data) + 1;
                    if (resp_end - hold < (4 + datalen_est)) {
                        truncated = true;
                        goto done;
                    }
                    SET_U16(rp, a->priority);
                    rp = format_name(rp, a->data);
                    uint16_t datalen = rp - hold - 2;
                    SET_U16(hold, datalen);
                    break;
                }
                case NS_T_SRV: {
                    uint8_t *hold = rp;
                    rp += 2;
                    uint16_t datalen_est = strlen(a->data) + 1;
                    if (resp_end - hold < (8 + datalen_est)) {
                        truncated = true;
                        goto done;
                    }
                    SET_U16(rp, a->priority);
                    SET_U16(rp, a->weight);
                    SET_U16(rp, a->port);
                    rp = format_name(rp, a->data);
                    uint16_t datalen = rp - hold - 2;
                    SET_U16(hold, datalen);
                    break;
                }
                default:
                    ZITI_LOG(WARN, "unhandled response type[%d]", (int)a->type);
            }
        }
        done:
        if (truncated) {
            ZITI_LOG(DEBUG, "dns response truncated");
            DNS_SET_TC(req->resp);
        }
        DNS_SET_ARS(req->resp, ans_count);
    }

    DNS_SET_AARS(req->resp, 1);
    if (resp_end - rp > 11) {
        memcpy(rp, DNS_OPT, sizeof(DNS_OPT));
        rp += sizeof(DNS_OPT);
    }
    req->resp_len = rp - req->resp;
}

static void process_host_req(struct dns_req *req) {
    dns_entry_t *entry = ziti_dns_lookup(req->msg.question[0]->name);
    if (entry) {
        req->msg.status = DNS_NO_ERROR;

        if (req->msg.question[0]->type == NS_T_A) {
            req->addr.s_addr = entry->addr.u_addr.ip4.addr;

            dns_answer *a = calloc(1, sizeof(dns_answer));
            a->ttl = 60;
            a->type = NS_T_A;
            a->data = strdup(entry->ip);
            req->msg.answer = calloc(2, sizeof(dns_answer *));
            req->msg.answer[0] = a;
        }

        format_resp(req);
        complete_dns_req(req);
    } else {
        int rc = query_upstream(req);
        if (rc != DNS_NO_ERROR) {
            req->msg.status = rc;
            format_resp(req);
            complete_dns_req(req);
        }
    }
}

static void proxy_domain_close_cb(ziti_connection c) {
    dns_domain_t *domain = ziti_conn_data(c);
    if (domain) {
        domain->resolv_proxy = NULL;
    }
}

static void on_proxy_connect(ziti_connection conn, int status) {
    dns_domain_t *domain = ziti_conn_data(conn);
    if (status == ZITI_OK) {
        ZITI_LOG(INFO, "proxy resolve connection established for domain[%s]", domain->name);
        domain->resolv_proxy = conn;
    } else {
        ZITI_LOG(ERROR, "failed to establish proxy resolve connection for domain[%s]", domain->name);
        ziti_close(conn, proxy_domain_close_cb);
    }
}

static ssize_t on_proxy_data(ziti_connection conn, const uint8_t* data, ssize_t status) {
    if (status >= 0) {
        ZITI_LOG(DEBUG, "proxy resolve: %.*s", (int)status, data);
        dns_message msg = {0};
        int rc = parse_dns_message(&msg, (const char *) data, status);
        if (rc < 0) {
            // the original DNS client's request won't be completed because we can't get the msg ID.
            return rc;
        }
        uint16_t id = msg.id;
        struct dns_req *req = model_map_get_key(&ziti_dns.requests, &id, sizeof(id));
        if (req) {
            req->msg.answer = msg.answer;
            msg.answer = NULL;
            format_resp(req);
            complete_dns_req(req);
        }
        free_dns_message(&msg);
    } else {
        ZITI_LOG(ERROR, "proxy resolve connection failed: %d(%s)", (int)status, ziti_errorstr(status));
        ziti_close(conn, proxy_domain_close_cb);
    }
    return status;
}

struct proxy_dns_req_wr_s {
    struct dns_req *req;
    char *json;
};

static void free_proxy_dns_wr(struct proxy_dns_req_wr_s *wr) {
    if (wr->json) {
        free(wr->json);
        wr->json = NULL;
    }
    free(wr);
}

static void on_proxy_write(ziti_connection conn, ssize_t len, void *ctx) {
    ZITI_LOG(DEBUG, "proxy resolve write: %d", (int)len);
    if (ctx) {
        struct proxy_dns_req_wr_s *wr = ctx;
        if (len < 0) {
            ZITI_LOG(WARN, "proxy resolve write failed: %s/%zd", ziti_errorstr(len), len);
            wr->req->msg.status = DNS_SERVFAIL;
            format_resp(wr->req);
            complete_dns_req(wr->req);
            ziti_close(conn, proxy_domain_close_cb);
        }
        free_proxy_dns_wr(wr);
    }
}

static void proxy_domain_req(struct dns_req *req, dns_domain_t *domain) {
    if (domain->resolv_proxy == NULL) {
        // initiate connection to hosting endpoint for this domain
        model_map_iter it = model_map_iterator(&domain->intercepts);
        void *intercept = model_map_it_value(it);
        domain->resolv_proxy = intercept_resolve_connect(intercept, domain, on_proxy_connect, on_proxy_data);
    }
    dns_question *q = req->msg.question[0];
    if (domain->resolv_proxy == NULL) {
        req->msg.status = DNS_SERVFAIL;
    } else if (q->type == NS_T_MX || q->type == NS_T_SRV || q->type == NS_T_TXT) {
        size_t jsonlen;
        struct proxy_dns_req_wr_s *wr = calloc(1, sizeof(struct proxy_dns_req_wr_s));
        wr->req = req;
        wr->json = dns_message_to_json(&req->msg, MODEL_JSON_COMPACT, &jsonlen);
        if (wr->json) {
            ZITI_LOG(DEBUG, "writing proxy resolve req[%04x]: %s", req->id, wr->json);

            // intercept_resolve_connect above can quick-fail if context does not have a valid API session
            // in that case resolve_proxy connection will be in Closed state and write will fail.
            // ziti_write will queue the message if the connection state is Connecting (as it will be the first time through)
            int rc = ziti_write(domain->resolv_proxy, (uint8_t *) wr->json, jsonlen, on_proxy_write, wr);
            if (rc == ZITI_OK) {
                // completion with client will happen in on_proxy_write if write fails, or on_proxy_data when response arrives
                return;
            }
            ZITI_LOG(WARN, "failed to write proxy resolve request[%04x]: %s", req->id, ziti_errorstr(rc));
            ziti_close(domain->resolv_proxy, proxy_domain_close_cb);
        } else {
            req->msg.status = DNS_FORMERR;
        }
        free_proxy_dns_wr(wr);
    } else {
        req->msg.status = DNS_NOT_IMPL;
    }

    format_resp(req);
    complete_dns_req(req);
}

ssize_t on_dns_req(const void *ziti_io_ctx, void *write_ctx, const void *q_packet, size_t q_len) {
    ziti_dns_client_t *clt = (ziti_dns_client_t *)ziti_io_ctx;
    const uint8_t *dns_packet = q_packet;
    size_t dns_packet_len = q_len;

    uint16_t req_id = DNS_ID(dns_packet);
    struct dns_req *req = model_map_get_key(&ziti_dns.requests, &req_id, sizeof(req_id));
    if (req != NULL) {
        ZITI_LOG(TRACE, "duplicate dns req[%04x] from %s client", req_id, req->clt == ziti_io_ctx ? "same" : "another");
        // just drop new request
        ziti_tunneler_ack(write_ctx);
        return (ssize_t)q_len;
    }

    req = calloc(1, sizeof(struct dns_req));
    req->clt = clt;

    req->req_len = q_len;
    memcpy(req->req, q_packet, q_len);

    if (parse_dns_req(&req->msg, dns_packet, dns_packet_len) != 0) {
        ZITI_LOG(ERROR, "failed to parse DNS message");
        on_dns_close(clt);
        free_dns_req(req);
        ziti_tunneler_ack(write_ctx);
        return (ssize_t)q_len;
    }
    req->id = req->msg.id;

    ZITI_LOG(TRACE, "received DNS query q_len=%zd id[%04x] recursive[%s] type[%d] name[%s]", q_len, req->id,
             req->msg.recursive ? "true" : "false",
             (int)req->msg.question[0]->type,
             req->msg.question[0]->name);

    model_map_set_key(&req->clt->active_reqs, &req->id, sizeof(req->id), req);
    model_map_set_key(&ziti_dns.requests, &req->id, sizeof(req->id), req);

    // route request
    dns_question *q = req->msg.question[0];

    if (q->type == NS_T_A || q->type == NS_T_AAAA) {
        process_host_req(req); // will send upstream if no local answer and req is recursive
    } else {
        // find domain requires normalized name
        char reqname[MAX_DNS_NAME];
        check_name(q->name, reqname, NULL);
        dns_domain_t *domain = find_domain(reqname);
        if (domain) {
            proxy_domain_req(req, domain);
        } else {
            int dns_status = query_upstream(req);
            if (dns_status != DNS_NO_ERROR) {
                req->msg.status = dns_status;
                format_resp(req);
                complete_dns_req(req);
            }
        }
    }

    ziti_tunneler_ack(write_ctx);
    return (ssize_t)q_len;
}

int query_upstream(struct dns_req *req) {
    bool avail = uv_is_active((const uv_handle_t *) &ziti_dns.upstream);
    bool success = false;
    if (avail && req->msg.recursive) {
        uv_buf_t buf = uv_buf_init((char *) req->req, req->req_len);

        for (int i = 0; i < ziti_dns.num_dns_up; i++) {
            int rc = uv_udp_try_send(&ziti_dns.upstream, &buf, 1,
                                     (struct sockaddr *) &ziti_dns.upstream_addr[i]);
            if (rc > 0) {
                success = true;
            } else {
                ZITI_LOG(WARN, "failed to query[%04x] upstream DNS server[%d]: %d(%s)",
                         req->id, i, rc, uv_strerror(rc));
            }
        }
    }
    return success ? DNS_NO_ERROR : DNS_REFUSE;
}

static void dns_upstream_alloc(uv_handle_t *h, size_t reqlen, uv_buf_t *b) {
    static char dns_buf[1024];
    b->base = dns_buf;
    b->len = sizeof(dns_buf);
}

static void on_upstream_packet(uv_udp_t *h, ssize_t rc, const uv_buf_t *buf, const struct sockaddr* addr, unsigned int flags) {
    if (rc > 0) {
        uint16_t id = DNS_ID(buf->base);
        struct dns_req *req = model_map_get_key(&ziti_dns.requests, &id, sizeof(id));
        if (req != NULL) {
            ZITI_LOG(TRACE, "upstream sent response to query[%04x] (rc=%zd)", id, rc);
            if (rc <= sizeof(req->resp)) {
                req->resp_len = rc;
                memcpy(req->resp, buf->base, rc);
            } else {
                ZITI_LOG(WARN, "unexpected DNS response: too large");
            }
            complete_dns_req(req);
        }
    }
}

static void free_dns_req(struct dns_req *req) {
    free_dns_message(&req->msg);
    free(req);
}

static void complete_dns_req(struct dns_req *req) {
    model_map_remove_key(&ziti_dns.requests, &req->id, sizeof(req->id));
    if (req->clt) {
        ziti_tunneler_write(req->clt->io_ctx->tnlr_io, req->resp, req->resp_len);
        model_map_remove_key(&req->clt->active_reqs, &req->id, sizeof(req->id));
        // close client if there are no other pending requests
        if (model_map_size(&req->clt->active_reqs) == 0) {
            on_dns_close(req->clt->io_ctx->ziti_io);
        }
    } else {
        ZITI_LOG(WARN, "query[%04x] is stale", req->id);
    }
    free_dns_req(req);
}