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

#ifndef ZITI_TUNNELER_SDK_ZITI_TUNNEL_CBS_H
#define ZITI_TUNNELER_SDK_ZITI_TUNNEL_CBS_H

#include "ziti/ziti_tunnel.h"
#include "ziti/ziti.h"


#if _WIN32
#ifndef strcasecmp
#define strcasecmp(a,b) stricmp(a,b)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TUNNELER_CONN_TYPE_ENUM(XX,...) \
XX(data, __VA_ARGS__)                    \
XX(resolver, __VA_ARGS__)

#define TUNNELER_APP_DATA_MODEL(XX, ...) \
XX(conn_type, TunnelConnectionType, none, connType, __VA_ARGS__) \
XX(dst_protocol, model_string, none, dst_protocol, __VA_ARGS__)\
XX(dst_hostname, model_string, none, dst_hostname, __VA_ARGS__)\
XX(dst_ip, model_string, none, dst_ip, __VA_ARGS__)\
XX(dst_port, model_string, none, dst_port, __VA_ARGS__)\
XX(src_protocol, model_string, none, src_protocol, __VA_ARGS__)\
XX(src_ip, model_string, none, src_ip, __VA_ARGS__)\
XX(src_port, model_string, none, src_port, __VA_ARGS__)\
XX(source_addr, model_string, none, source_addr, __VA_ARGS__)

DECLARE_ENUM(TunnelConnectionType, TUNNELER_CONN_TYPE_ENUM)

DECLARE_MODEL(tunneler_app_data, TUNNELER_APP_DATA_MODEL)

#define TUNNEL_COMMANDS(XX, ...) \
XX(ZitiDump, __VA_ARGS__)    \
XX(IpDump, __VA_ARGS__) \
XX(LoadIdentity, __VA_ARGS__)   \
XX(ListIdentities, __VA_ARGS__) \
XX(IdentityOnOff, __VA_ARGS__) \
XX(EnableMFA, __VA_ARGS__)  \
XX(SubmitMFA, __VA_ARGS__)  \
XX(VerifyMFA, __VA_ARGS__)  \
XX(RemoveMFA, __VA_ARGS__)  \
XX(GenerateMFACodes, __VA_ARGS__) \
XX(GetMFACodes, __VA_ARGS__) \
XX(GetMetrics, __VA_ARGS__) \
XX(SetLogLevel, __VA_ARGS__) \
XX(UpdateTunIpv4, __VA_ARGS__) \
XX(ServiceControl, __VA_ARGS__) \
XX(Status, __VA_ARGS__) \
XX(RefreshIdentity, __VA_ARGS__) \
XX(RemoveIdentity, __VA_ARGS__) \
XX(StatusChange, __VA_ARGS__)   \
XX(AddIdentity, __VA_ARGS__)    \
XX(Enroll, __VA_ARGS__)         \
XX(ExternalAuth, __VA_ARGS__)   \
XX(SetUpstreamDNS, __VA_ARGS__) \
XX(AccessTokenAuth, __VA_ARGS__)

DECLARE_ENUM(TunnelCommand, TUNNEL_COMMANDS)

#define TUNNEL_CMD(XX, ...) \
XX(command, TunnelCommand, none, Command, __VA_ARGS__) \
XX(data, json, none, Data, __VA_ARGS__)                \
XX(show_result, model_bool, none, , __VA_ARGS__)

#define TUNNEL_CMD_RES(XX, ...) \
XX(success, model_bool, none, Success, __VA_ARGS__) \
XX(error, model_string, none, Error, __VA_ARGS__)\
XX(data, json, none, Data, __VA_ARGS__) \
XX(code, model_number, none, Code, __VA_ARGS__)

#define TNL_LOAD_IDENTITY(XX, ...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__)\
XX(path, model_string, none, Path, __VA_ARGS__)            \
XX(config, ziti_config, ptr, Config, __VA_ARGS__)          \
XX(disabled, model_bool, none, Disabled, __VA_ARGS__)      \
XX(apiPageSize, model_number, none, ApiPageSize, __VA_ARGS__)

#define TNL_ON_OFF_IDENTITY(XX, ...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__) \
XX(onOff, model_bool, none, OnOff, __VA_ARGS__)

#define TNL_IDENTITY_INFO(XX, ...) \
XX(name, model_string, none, Name, __VA_ARGS__) \
XX(config, model_string, none, Config, __VA_ARGS__) \
XX(network, model_string, none, Network, __VA_ARGS__) \
XX(id, model_string, none, Id, __VA_ARGS__)

#define TNL_IDENTITY_LIST(XX, ...) \
XX(identities, tunnel_identity_info, array, Identities, __VA_ARGS__)

#define TNL_ZITI_DUMP(XX, ...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__) \
XX(dump_path, model_string, none, DumpPath, __VA_ARGS__)

#define TNL_IP_DUMP(XX, ...) \
XX(dump_path, model_string, none, DumpPath, __VA_ARGS__)

#define TNL_IDENTITY_ID(XX, ...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__)

#define TNL_ID_EXT_AUTH(XX, ...) \
TNL_IDENTITY_ID(XX, __VA_ARGS__) \
XX(provider, model_string, none, Provider, __VA_ARGS__)

#define TNL_ID_ACCESSTOKEN_AUTH(XX, ...) \
TNL_IDENTITY_ID(XX, __VA_ARGS__) \
XX(accesstoken, model_string, none, AccessToken, __VA_ARGS__)

#define TNL_MFA_ENROL_RES(XX,...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__) \
XX(is_verified, model_bool, none, IsVerified, __VA_ARGS__) \
XX(provisioning_url, model_string, none, ProvisioningUrl, __VA_ARGS__) \
XX(recovery_codes, model_string, array, RecoveryCodes, __VA_ARGS__)

// MFA auth command
#define TNL_SUBMIT_MFA(XX, ...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__) \
XX(code, model_string, none, Code, __VA_ARGS__)

// MFA auth command
#define TNL_VERIFY_MFA(XX, ...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__) \
XX(code, model_string, none, Code, __VA_ARGS__)

#define TNL_REMOVE_MFA(XX, ...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__) \
XX(code, model_string, none, Code, __VA_ARGS__)

#define TNL_GENERATE_MFA_CODES(XX, ...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__) \
XX(code, model_string, none, Code, __VA_ARGS__)

#define TNL_MFA_RECOVERY_CODES(XX, ...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__) \
XX(recovery_codes, model_string, array, RecoveryCodes, __VA_ARGS__)

#define TNL_GET_MFA_CODES(XX, ...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__) \
XX(code, model_string, none, Code, __VA_ARGS__)

#define TNL_IDENTITY_METRICS(XX, ...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__) \
XX(up, model_string, none, Up, __VA_ARGS__) \
XX(down, model_string, none, Down, __VA_ARGS__)

#define TUNNEL_CMD_INLINE(XX, ...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__) \
XX(command, TunnelCommand, none, Command, __VA_ARGS__)

#define TUNNEL_SET_LOG_LEVEL(XX, ...) \
XX(loglevel, model_string, none, Level, __VA_ARGS__)

#define TUNNEL_TUN_IP_V4(XX, ...) \
XX(tunIP, model_string, none, TunIPv4, __VA_ARGS__) \
XX(prefixLength, model_number, none, TunPrefixLength, __VA_ARGS__) \
XX(addDns, model_bool, none, AddDns, __VA_ARGS__)

#define TUNNEL_SERVICE_CONTROL(XX, ...) \
XX(operation, model_string, none, Operation, __VA_ARGS__)

#define TUNNEL_STATUS_CHANGE(XX, ...) \
XX(woken, model_bool, none, Woke, __VA_ARGS__) \
XX(unlocked, model_bool, none, Unlocked, __VA_ARGS__)

#define TUNNEL_ADD_IDENTITY(XX, ...) \
XX(useKeychain, model_bool, none, UseKeychain, __VA_ARGS__) \
XX(identityFilename, model_string, none, IdentityFilename, __VA_ARGS__) \
XX(jwtContent, model_string, none, JwtContent, __VA_ARGS__) \
XX(key, model_string, none, Key, __VA_ARGS__) \
XX(cert, model_string, none, Certificate, __VA_ARGS__) \
XX(controllerURL, model_string, none, ControllerURL, __VA_ARGS__)

#define TUNNEL_EXT_AUTH(XX, ...) \
XX(identifier, model_string, none, identifier, __VA_ARGS__) \
XX(ext_auth_url, model_string, none, url, __VA_ARGS__)

#define TUNNEL_ACCESSTOKEN_AUTH(XX, ...) \
XX(identifier, model_string, none, Identifier, __VA_ARGS__)

#define TUNNEL_UPSTREAM_DNS(XX, ...) \
XX(host, model_string, none, host, __VA_ARGS__) \
XX(port, model_number, none, port, __VA_ARGS__)

#define TNL_ENROLL(XX, ...) \
XX(url, model_string, none, url, __VA_ARGS__)   \
XX(name, model_string, none, name, __VA_ARGS__) \
XX(jwt, model_string, none, jwt, __VA_ARGS__)   \
XX(key, model_string, none, key, __VA_ARGS__)   \
XX(cert, model_string, none, cert, __VA_ARGS__) \
XX(use_keychain, model_bool, none, useKeychain, __VA_ARGS__)

DECLARE_MODEL(tunnel_command, TUNNEL_CMD)
DECLARE_MODEL(tunnel_result, TUNNEL_CMD_RES)
DECLARE_MODEL(tunnel_load_identity, TNL_LOAD_IDENTITY)
DECLARE_MODEL(tunnel_identity_info, TNL_IDENTITY_INFO)
DECLARE_MODEL(tunnel_identity_lst, TNL_IDENTITY_LIST)
DECLARE_MODEL(tunnel_ziti_dump, TNL_ZITI_DUMP)
DECLARE_MODEL(tunnel_ip_dump, TNL_IP_DUMP)
DECLARE_MODEL(tunnel_on_off_identity, TNL_ON_OFF_IDENTITY)
DECLARE_MODEL(tunnel_identity_id, TNL_IDENTITY_ID)
DECLARE_MODEL(tunnel_id_ext_auth, TNL_ID_EXT_AUTH)
DECLARE_MODEL(tunnel_id_accesstoken_auth, TNL_ID_ACCESSTOKEN_AUTH)
DECLARE_MODEL(tunnel_mfa_enrol_res, TNL_MFA_ENROL_RES)
DECLARE_MODEL(tunnel_submit_mfa, TNL_SUBMIT_MFA)
DECLARE_MODEL(tunnel_verify_mfa, TNL_VERIFY_MFA)
DECLARE_MODEL(tunnel_remove_mfa, TNL_REMOVE_MFA)
DECLARE_MODEL(tunnel_generate_mfa_codes, TNL_GENERATE_MFA_CODES)
DECLARE_MODEL(tunnel_mfa_recovery_codes, TNL_MFA_RECOVERY_CODES)
DECLARE_MODEL(tunnel_get_mfa_codes, TNL_GET_MFA_CODES)
DECLARE_MODEL(tunnel_identity_metrics, TNL_IDENTITY_METRICS)
DECLARE_MODEL(tunnel_command_inline, TUNNEL_CMD_INLINE)
DECLARE_MODEL(tunnel_set_log_level, TUNNEL_SET_LOG_LEVEL)
DECLARE_MODEL(tunnel_tun_ip_v4, TUNNEL_TUN_IP_V4)
DECLARE_MODEL(tunnel_service_control, TUNNEL_SERVICE_CONTROL)
DECLARE_MODEL(tunnel_status_change, TUNNEL_STATUS_CHANGE)
DECLARE_MODEL(tunnel_add_identity, TUNNEL_ADD_IDENTITY)
DECLARE_MODEL(tunnel_upstream_dns, TUNNEL_UPSTREAM_DNS)
DECLARE_MODEL(tunnel_enroll, TNL_ENROLL)
DECLARE_MODEL(tunnel_ext_auth, TUNNEL_EXT_AUTH)
DECLARE_MODEL(tunnel_accesstoken_auth, TUNNEL_ACCESSTOKEN_AUTH)

#define TUNNEL_EVENTS(XX, ...) \
XX(ContextEvent, __VA_ARGS__) \
XX(ServiceEvent, __VA_ARGS__)  \
XX(MFAEvent, __VA_ARGS__)      \
XX(MFAStatusEvent, __VA_ARGS__) \
XX(ConfigEvent, __VA_ARGS__)      \
XX(ExtJWTEvent, __VA_ARGS__)

DECLARE_ENUM(TunnelEvent, TUNNEL_EVENTS)

#define MFA_STATUS(XX, ...) \
XX(mfa_auth_status, __VA_ARGS__) \
XX(auth_challenge, __VA_ARGS__)  \
XX(enrollment_verification, __VA_ARGS__) \
XX(enrollment_remove, __VA_ARGS__) \
XX(enrollment_challenge, __VA_ARGS__)    \
XX(key_pass_challenge, __VA_ARGS__)

DECLARE_ENUM(mfa_status, MFA_STATUS)

#define BASE_EVENT_MODEL(XX, ...) \
XX(identifier, model_string, none, identifier, __VA_ARGS__) \
XX(event_type, TunnelEvent, none, type, __VA_ARGS__)

#define ZTX_EVENT_MODEL(XX, ...)  \
BASE_EVENT_MODEL(XX, __VA_ARGS__) \
XX(status, model_string, none, status, __VA_ARGS__) \
XX(name, model_string, none, name, __VA_ARGS__) \
XX(version, model_string, none, version, __VA_ARGS__) \
XX(controller, model_string, none, controller, __VA_ARGS__) \
XX(code, model_number, none, code, __VA_ARGS__)

#define ZTX_SVC_EVENT_MODEL(XX, ...)  \
BASE_EVENT_MODEL(XX, __VA_ARGS__)            \
XX(status, model_string, none, status, __VA_ARGS__) \
XX(added_services, ziti_service, array, added_services, __VA_ARGS__) \
XX(removed_services, ziti_service, array, removed_services, __VA_ARGS__)

#define MFA_EVENT_MODEL(XX, ...)  \
BASE_EVENT_MODEL(XX, __VA_ARGS__)               \
XX(provider, model_string, none, provider, __VA_ARGS__) \
XX(status, model_string, none, status, __VA_ARGS__)   \
XX(operation, model_string, none, operation, __VA_ARGS__) \
XX(operation_type, mfa_status, none, operation_type, __VA_ARGS__ ) \
XX(provisioning_url, model_string, none, provisioning_url, __VA_ARGS__) \
XX(recovery_codes, model_string, array, recovery_codes, __VA_ARGS__) \
XX(code, model_number, none, code, __VA_ARGS__)

#define CONFIG_EVENT_MODEL(XX, ...)  \
BASE_EVENT_MODEL(XX, __VA_ARGS__)     \
XX(config_json, json, none, config, __VA_ARGS__) \
XX(identity_name, model_string, none, identity_name, __VA_ARGS__)

#define EXT_JWT_PROVIDER(XX, ...) \
XX(name, model_string, none, name, __VA_ARGS__) \
XX(issuer, model_string, none, issuer, __VA_ARGS__)

#define EXT_SIGNER_EVENT_MODEL(XX, ...)  \
BASE_EVENT_MODEL(XX, __VA_ARGS__)               \
XX(status, model_string, none, status, __VA_ARGS__)   \
XX(providers, jwt_provider, list, providers, __VA_ARGS__)

DECLARE_MODEL(base_event, BASE_EVENT_MODEL)
DECLARE_MODEL(ziti_ctx_event, ZTX_EVENT_MODEL)
DECLARE_MODEL(mfa_event, MFA_EVENT_MODEL)
DECLARE_MODEL(service_event, ZTX_SVC_EVENT_MODEL)
DECLARE_MODEL(config_event, CONFIG_EVENT_MODEL)

DECLARE_MODEL(jwt_provider, EXT_JWT_PROVIDER)
DECLARE_MODEL(ext_signer_event, EXT_SIGNER_EVENT_MODEL)

typedef struct tunneled_service_s tunneled_service_t;

#define MAX_PENDING_BYTES (128 * 1024)

/** context passed through the tunneler SDK for network i/o */
typedef struct ziti_io_ctx_s {
    ziti_connection      ziti_conn;
    bool ziti_eof;
    bool tnlr_eof;
    uint64_t pending_wbytes;
} ziti_io_context;


typedef void (*event_cb)(const base_event* event);
typedef void (*command_cb)(const tunnel_result *, void *ctx);
typedef struct {
    int (*process)(const tunnel_command *cmd, command_cb cb, void *ctx);
    int (*load_identity)(const char *identifier, const char *path, bool disabled, int api_page_size, command_cb cb, void *ctx);
    // do not use, temporary accessor
    ziti_context (*get_ziti)(const char *identifier);
} ziti_tunnel_ctrl;

/**
  * replaces first occurrence of _substring_ in _source_ with _with_.
  * returns pointer to last replaced char in _source_, or NULL if no replacement was made.
  */
char *string_replace(char *source, size_t sourceSize, const char *substring, const char *with);

/** called by tunneler SDK after a client connection is intercepted */
void *ziti_sdk_c_dial(const void *app_intercept_ctx, struct io_ctx_s *io);

/** called from tunneler SDK when intercepted client sends data */
ssize_t ziti_sdk_c_write(const void *ziti_io_ctx, void *write_ctx, const void *data, size_t len);

/** called by tunneler SDK after a client connection's RX is closed
 * return 0 if TX should still be open, 1 if both sides are closed */
int ziti_sdk_c_close(void *io_ctx);
int ziti_sdk_c_close_write(void *io_ctx);

host_ctx_t *ziti_sdk_c_host(void *ziti_ctx, uv_loop_t *loop, const char *service_name, cfg_type_e cfgtype, const void *cfg);

/** passed to ziti-sdk via ziti_options.service_cb */
tunneled_service_t *ziti_sdk_c_on_service(ziti_context ziti_ctx, ziti_service *service, int status, void *tnlr_ctx);

void remove_intercepts(ziti_context ziti_ctx, void *tnlr_ctx);

const ziti_tunnel_ctrl* ziti_tunnel_init_cmd(uv_loop_t *loop, tunneler_context, event_cb);

struct add_identity_request_s {
    char* identifier;
    char* identifier_file_name;
    char* jwt_content;
    char* key;
    char* certificate;
    char* url;

    bool use_keychain;
    void *add_id_ctx;
    command_cb cmd_cb;
    void *cmd_ctx;
};

#define IPC_SUCCESS 0
#define IPC_ERROR 500

struct ziti_instance_s {
    char *identifier;
    command_cb load_cb;
    void *load_ctx;

    ziti_context ztx;
    struct mfa_request_s *mfa_req;
    model_map intercepts;
    LIST_ENTRY(ziti_instance_s) _next;
};

void ziti_set_refresh_interval(unsigned long seconds);

struct ziti_instance_s *new_ziti_instance(const char *identifier);
int init_ziti_instance(struct ziti_instance_s *inst, const ziti_config *cfg, const ziti_options *opts);
/** set options for tsdk usage on a ziti_instance's ziti_context */
int set_tnlr_options(struct ziti_instance_s *inst);
void set_ziti_instance(const char *identifier, struct ziti_instance_s *inst);
void remove_ziti_instance(const char *identifier);

#ifdef __cplusplus
}
#endif

#endif //ZITI_TUNNELER_SDK_ZITI_TUNNEL_CBS_H