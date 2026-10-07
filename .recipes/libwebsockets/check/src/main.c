/* Brings libwebsockets up with TLS and takes it down again: the version it
   reports, a context whose OpenSSL is initialised, and a clean destroy. */

#include <libwebsockets.h>
#include <stdio.h>
#include <string.h>

static const struct lws_protocols protocols[] = {
    {"check", lws_callback_http_dummy, 0, 0, 0, NULL, 0},
    LWS_PROTOCOL_LIST_TERM,
};

int main(void) {
    const char *version = lws_get_library_version();
    if(strncmp(version, "5.0.0", 5) != 0) {
        fprintf(stderr, "lws_get_library_version is %s, not 5.0.0\n", version);
        return 1;
    }
    lws_set_log_level(LLL_ERR | LLL_WARN, NULL);

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof info);
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    struct lws_context *context = lws_create_context(&info);
    if(context == NULL) {
        fprintf(stderr, "lws_create_context failed\n");
        return 1;
    }
    lws_context_destroy(context);
    printf("libwebsockets %s with OpenSSL: ok\n", version);
    return 0;
}
