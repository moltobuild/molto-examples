#ifndef HELLO_API_ROUTES_H
#define HELLO_API_ROUTES_H

#include <stddef.h>

/*
 * The API, as a function from a request line to a response.
 *
 * Nothing here knows about sockets or libwebsockets: the server in main.c asks
 * this for every request and writes back what it gets, and the tests ask it
 * directly.
 *
 *   GET /             {"message":"Hello, world!"}
 *   GET /hello        {"message":"Hello, world!"}
 *   GET /hello/{name} {"message":"Hello, {name}!"}
 *   GET /health       {"status":"ok"}
 *
 * A name is 1 to API_NAME_MAX letters, digits, '-' or '_', so it never needs
 * escaping in JSON. Anything else answers 400, an unknown path 404, and a
 * method other than GET or HEAD on a known one 405.
 */

#define API_NAME_MAX 32
#define API_BODY_MAX 128

typedef struct {
    int status;                /* HTTP status code */
    char body[API_BODY_MAX];   /* JSON, NUL-terminated */
    size_t length;             /* bytes of body, without the NUL */
} api_response;

/* Fills out for method ("GET", "POST", ...) and path ("/hello/ana"); a query
   string, if any, is ignored. Returns out->status. */
int api_route(const char *method, const char *path, api_response *out);

#endif
