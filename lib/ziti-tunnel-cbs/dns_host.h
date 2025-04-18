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

//
// Created by eugene on 11/12/2021.
//

#ifndef ZITI_TUNNELER_SDK_DNS_HOST_H
#define ZITI_TUNNELER_SDK_DNS_HOST_H

#if _WIN32
#include <windows.h>
#include <windns.h>

#define ns_rr DNS_RECORDA
#define ns_msg void

#define ns_t_srv DNS_TYPE_SRV
#define ns_t_mx  DNS_TYPE_MX
#define ns_t_txt DNS_TYPE_TEXT

#define ns_r_refused DNS_RCODE_REFUSED

typedef struct {
    int no_use;
} resolver_t;
#define res_nclose(rs) do{}while(0)
#define res_ninit(rs) 0
#else
#include <resolv.h>
typedef struct __res_state resolver_t;
#  if __RES < 19991006
#     define res_ninit(c) res_init()
#     define res_nclose(c) do{}while(0)
#     define res_nquery(res, name, c, t, resp, sz) res_query(name, c, t, resp, sz)
#endif

#endif

#include <stdint.h>
#include <ziti/model_support.h>

#define DNS_Q_MODEL(XX, ...) \
XX(name, model_string, none, name, __VA_ARGS__) \
XX(type, model_number, none, type, __VA_ARGS__)

#define DNS_A_MODEL(XX, ...) \
DNS_Q_MODEL(XX, __VA_ARGS__) \
XX(ttl, model_number, none, ttl, __VA_ARGS__) \
XX(priority, model_number, none, priority, __VA_ARGS__) \
XX(weight, model_number, none, weight, __VA_ARGS__)     \
XX(port, model_number , none, port, __VA_ARGS__)    \
XX(data, model_string, none, data, __VA_ARGS__)

#define DNS_MSG_MODEL(XX,...) \
XX(status, model_number, none, status, __VA_ARGS__) \
XX(id, model_number, none, id, __VA_ARGS__)         \
XX(recursive, model_number, none, recursive, __VA_ARGS__) \
XX(question, dns_question, array, question, __VA_ARGS__) \
XX(answer, dns_answer, array, answer, __VA_ARGS__)       \
XX(comment, model_string, none, comment, __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

#define DNS_FLAG_QR(f) (((f) & 0x8000U) != 0)
#define DNS_FLAG_RD(f) (((f) & 0x0100U) != 0)

DECLARE_MODEL(dns_question, DNS_Q_MODEL)

DECLARE_MODEL(dns_answer, DNS_A_MODEL)

DECLARE_MODEL(dns_message, DNS_MSG_MODEL)

void dns_host_init();

void do_query(const dns_question *q, dns_message *resp, resolver_t *resolver);

int parse_dns_req(dns_message *msg, const unsigned char* buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif //ZITI_TUNNELER_SDK_DNS_HOST_H
