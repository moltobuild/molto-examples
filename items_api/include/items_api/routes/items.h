#ifndef ITEMS_API_ROUTES_ITEMS_H
#define ITEMS_API_ROUTES_ITEMS_H
#include <items_api/db/database.h>
#include <items_api/models/item.h>

typedef struct {
    int status;
    char body[ITEM_RESPONSE_MAX];
    size_t length;
    char allow[32];
} api_response;
void items_route(PGconn *conn, const char *method, const char *path, const char *query,
                 const char *body, api_response *response);
void api_error(api_response *response, int status, const char *message);
#endif
