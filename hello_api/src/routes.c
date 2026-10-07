#include <hello_api/routes.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int respond(api_response *out, int status, const char *json) {
    out->status = status;
    int n = snprintf(out->body, sizeof out->body, "%s", json);
    out->length = (size_t)n;
    return status;
}

static bool name_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '_';
}

/* The path without its query string, as a length into path. */
static size_t path_length(const char *path) {
    const char *query = strchr(path, '?');
    return query != NULL ? (size_t)(query - path) : strlen(path);
}

static bool path_is(const char *path, size_t length, const char *route) {
    return strlen(route) == length && strncmp(path, route, length) == 0;
}

static int greet(api_response *out, const char *name, size_t length) {
    if(length == 0 || length > API_NAME_MAX)
        return respond(out, 400, "{\"error\":\"a name is 1 to 32 characters\"}");
    for(size_t i = 0; i < length; i++)
        if(!name_char(name[i]))
            return respond(out, 400,
                           "{\"error\":\"a name is letters, digits, '-' or '_'\"}");
    out->status = 200;
    int n = snprintf(out->body, sizeof out->body, "{\"message\":\"Hello, %.*s!\"}",
                     (int)length, name);
    out->length = (size_t)n;
    return out->status;
}

int api_route(const char *method, const char *path, api_response *out) {
    if(method == NULL || path == NULL)
        return respond(out, 400, "{\"error\":\"bad request\"}");
    size_t length = path_length(path);
    bool readable = strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0;

    static const char hello[] = "/hello/";
    bool known = path_is(path, length, "/") || path_is(path, length, "/hello") ||
                 path_is(path, length, "/health") ||
                 (length >= sizeof hello - 1 && strncmp(path, hello, sizeof hello - 1) == 0);
    if(!known)
        return respond(out, 404, "{\"error\":\"not found\"}");
    if(!readable)
        return respond(out, 405, "{\"error\":\"method not allowed\"}");

    if(path_is(path, length, "/health"))
        return respond(out, 200, "{\"status\":\"ok\"}");
    if(path_is(path, length, "/") || path_is(path, length, "/hello"))
        return respond(out, 200, "{\"message\":\"Hello, world!\"}");
    return greet(out, path + sizeof hello - 1, length - (sizeof hello - 1));
}
