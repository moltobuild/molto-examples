/* An HTTP server on libwebsockets that answers with routes.c: the request line
   goes in, the status and JSON body it returns go out. */

#include <hello_api/routes.h>

#include <libwebsockets.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t interrupted;

/* What one connection answers: worked out when the headers arrive, written
   when the socket can take the body. */
typedef struct {
    api_response response;
    int sent;
} session;

/* In the order of libwebsockets' LWSHUMETH_ constants. */
static const char *const methods[] = {"GET", "POST", "OPTIONS", "PUT", "PATCH",
                                      "DELETE", "CONNECT", "HEAD"};

static int answer(struct lws *wsi, session *s, const char *uri, int method) {
    const char *name = method >= 0 && method < (int)(sizeof methods / sizeof *methods)
                           ? methods[method]
                           : "OTHER";
    api_route(name, uri, &s->response);
    s->sent = 0;
    lwsl_user("%s %s -> %d\n", name, uri, s->response.status);

    uint8_t buffer[LWS_PRE + 512];
    uint8_t *start = buffer + LWS_PRE, *p = start, *end = buffer + sizeof buffer - 1;
    if(lws_add_http_common_headers(wsi, (unsigned int)s->response.status,
                                   "application/json", s->response.length, &p, end) ||
       lws_finalize_write_http_header(wsi, start, &p, end))
        return 1;
    if(method == LWSHUMETH_HEAD)
        return lws_http_transaction_completed(wsi) ? -1 : 0;
    lws_callback_on_writable(wsi);
    return 0;
}

static int on_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
                   size_t len) {
    session *s = user;
    switch(reason) {
    case LWS_CALLBACK_HTTP: {
        char *uri = NULL;
        int uri_length = 0;
        int method = lws_http_get_uri_and_method(wsi, &uri, &uri_length);
        return answer(wsi, s, method >= 0 ? uri : (const char *)in, method);
    }
    case LWS_CALLBACK_HTTP_WRITEABLE: {
        if(s == NULL || s->sent)
            return 0;
        uint8_t buffer[LWS_PRE + API_BODY_MAX];
        memcpy(buffer + LWS_PRE, s->response.body, s->response.length);
        s->sent = 1;
        if(lws_write(wsi, buffer + LWS_PRE, s->response.length, LWS_WRITE_HTTP_FINAL) !=
           (int)s->response.length)
            return 1;
        return lws_http_transaction_completed(wsi) ? -1 : 0;
    }
    default:
        return lws_callback_http_dummy(wsi, reason, user, in, len);
    }
}

static const struct lws_protocols protocols[] = {
    {"http", on_http, sizeof(session), 0, 0, NULL, 0},
    LWS_PROTOCOL_LIST_TERM,
};

static void on_signal(int sig) {
    (void)sig;
    interrupted = 1;
}

int main(void) {
    const char *port = getenv("PORT");
    int listen_port = port != NULL ? atoi(port) : 8080;
    if(listen_port <= 0 || listen_port > 65535) {
        fprintf(stderr, "PORT must be 1 to 65535, not %s\n", port);
        return 1;
    }

    signal(SIGINT, on_signal);
    lws_set_log_level(LLL_ERR | LLL_WARN | LLL_USER, NULL);

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof info);
    info.port = listen_port;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
    struct lws_context *context = lws_create_context(&info);
    if(context == NULL) {
        fprintf(stderr, "could not listen on port %d\n", listen_port);
        return 1;
    }

    printf("hello_api %s on http://localhost:%d (libwebsockets %s), Ctrl+C to stop\n",
           MOLTO_PKG_VERSION, listen_port, lws_get_library_version());
    fflush(stdout);
    int n = 0;
    while(n >= 0 && !interrupted)
        n = lws_service(context, 0);

    lws_context_destroy(context);
    return 0;
}
