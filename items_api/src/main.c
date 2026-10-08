#include <items_api/config/env.h>
#include <items_api/routes/items.h>
#include <libwebsockets.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t stopped;
typedef struct {
    api_response response;
    char body[ITEM_REQUEST_MAX + 1], path[256], query[256];
    size_t received, sent;
    int method;
    bool waiting, responded, headers_sent;
} session;
static const char *const methods[] = {"GET",   "POST",   "OPTIONS", "PUT",
                                      "PATCH", "DELETE", "CONNECT", "HEAD"};
static void respond(struct lws *wsi, session *s) {
    if(s->responded)
        return;
    PGconn *conn = lws_context_user(lws_get_context(wsi));
    const char *method = s->method >= 0 && s->method < 8 ? methods[s->method] : "OTHER";
    items_route(conn, method, s->path, s->query, s->body, &s->response);
    s->responded = true;
    s->waiting = false;
    lws_callback_on_writable(wsi);
}
static void reject(struct lws *wsi, session *s, int status, const char *message) {
    api_error(&s->response, status, message);
    s->responded = true;
    s->waiting = false;
    lws_callback_on_writable(wsi);
}
static int on_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
                   size_t len) {
    session *s = user;
    switch(reason) {
    case LWS_CALLBACK_HTTP: {
        memset(s, 0, sizeof *s);
        char *uri = NULL;
        int uri_length = 0;
        s->method = lws_http_get_uri_and_method(wsi, &uri, &uri_length);
        if(s->method < 0 || uri_length < 0 || (size_t)uri_length >= sizeof s->path) {
            reject(wsi, s, 414, "Request URI too long");
            return 0;
        }
        memcpy(s->path, uri, (size_t)uri_length);
        s->path[uri_length] = '\0';
        int query_length = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_URI_ARGS);
        if(query_length >= (int)sizeof s->query ||
           (query_length &&
            lws_hdr_copy(wsi, s->query, sizeof s->query, WSI_TOKEN_HTTP_URI_ARGS) < 0)) {
            reject(wsi, s, 400, "Query too long");
            return 0;
        }
        s->waiting = s->method == LWSHUMETH_POST || s->method == LWSHUMETH_PATCH;
        if(!s->waiting) {
            respond(wsi, s);
            return 0;
        }
        char type[128] = "", length[32] = "";
        if(lws_hdr_copy(wsi, type, sizeof type, WSI_TOKEN_HTTP_CONTENT_TYPE) <= 0 ||
           strncmp(type, "application/json", 16) != 0 || (type[16] && type[16] != ';')) {
            reject(wsi, s, 415, "Use Content-Type application/json");
            return 0;
        }
        if(lws_hdr_copy(wsi, length, sizeof length, WSI_TOKEN_HTTP_CONTENT_LENGTH) <= 0) {
            reject(wsi, s, 411, "Content-Length is required");
            return 0;
        }
        char *end;
        unsigned long bytes = strtoul(length, &end, 10);
        if(*end || !*length || bytes > ITEM_REQUEST_MAX) {
            reject(wsi, s, 413, "Request body too large");
            return 0;
        }
        if(!bytes)
            respond(wsi, s);
        return 0;
    }
    case LWS_CALLBACK_HTTP_BODY:
        if(!s->waiting)
            return 0;
        if(len > ITEM_REQUEST_MAX - s->received || memchr(in, 0, len)) {
            reject(wsi, s, 413, "Request body too large or contains a NUL byte");
            return 0;
        }
        memcpy(s->body + s->received, in, len);
        s->received += len;
        s->body[s->received] = '\0';
        return 0;
    case LWS_CALLBACK_HTTP_BODY_COMPLETION:
        if(s->waiting)
            respond(wsi, s);
        return 0;
    case LWS_CALLBACK_HTTP_WRITEABLE: {
        if(!s || !s->responded)
            return 0;
        if(!s->headers_sent) {
            unsigned char buffer[LWS_PRE + 512], *start = buffer + LWS_PRE, *p = start;
            unsigned char *end = buffer + sizeof buffer;
            if(lws_add_http_common_headers(wsi, (unsigned)s->response.status, "application/json",
                                           (lws_filepos_t)s->response.length, &p, end) ||
               (*s->response.allow &&
                lws_add_http_header_by_name(
                    wsi, (const unsigned char *)"allow:", (const unsigned char *)s->response.allow,
                    (int)strlen(s->response.allow), &p, end)) ||
               lws_finalize_write_http_header(wsi, start, &p, end))
                return -1;
            s->headers_sent = true;
            if(!s->response.length || s->method == LWSHUMETH_HEAD)
                return lws_http_transaction_completed(wsi) ? -1 : 0;
            lws_callback_on_writable(wsi);
            return 0;
        }
        unsigned char buffer[LWS_PRE + 4096];
        size_t count = s->response.length - s->sent;
        if(count > 4096)
            count = 4096;
        memcpy(buffer + LWS_PRE, s->response.body + s->sent, count);
        bool last = s->sent + count == s->response.length;
        if(lws_write(wsi, buffer + LWS_PRE, count, last ? LWS_WRITE_HTTP_FINAL : LWS_WRITE_HTTP) !=
           (int)count)
            return -1;
        s->sent += count;
        if(last)
            return lws_http_transaction_completed(wsi) ? -1 : 0;
        lws_callback_on_writable(wsi);
        return 0;
    }
    default:
        return lws_callback_http_dummy(wsi, reason, user, in, len);
    }
}
static const struct lws_protocols protocols[] = {
    {"http", on_http, sizeof(session), 4096, 0, NULL, 0}, LWS_PROTOCOL_LIST_TERM};
static void stop(int sig) {
    (void)sig;
    stopped = 1;
}
int main(void) {
    app_config config;
    char error[256];
    if(!config_read(".env", &config, error, sizeof error)) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    long number = config.port;
    PGconn *conn = db_open(config.database_url);
    if(!conn)
        return 1;
    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    lws_set_log_level(LLL_ERR | LLL_WARN, NULL);
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof info);
    info.port = (int)number;
    info.iface = "127.0.0.1";
    info.protocols = protocols;
    info.user = conn;
    struct lws_context *context = lws_create_context(&info);
    if(!context) {
        db_close(conn);
        fprintf(stderr, "Could not create HTTP server\n");
        return 1;
    }
    printf("items_api listening at http://127.0.0.1:%ld\n", number);
    fflush(stdout);
    int status = 0;
    while(!stopped && status >= 0)
        status = lws_service(context, 0);
    lws_context_destroy(context);
    db_close(conn);
    return status < 0 ? 1 : 0;
}
