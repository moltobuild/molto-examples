#include <items_api/repositories/items.h>
#include <items_api/routes/items.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void api_error(api_response *r, int status, const char *message) {
    r->status = status;
    /* Callers supply constant messages, without quotes or backslashes. */
    int n = snprintf(r->body, sizeof r->body, "{\"error\":\"%s\"}", message);
    r->length = n > 0 && (size_t)n < sizeof r->body ? (size_t)n : 0;
}
static bool pagination(const char *query, int *limit, int *offset) {
    *limit = 20;
    *offset = 0;
    bool saw_limit = false, saw_offset = false;
    while(*query) {
        int *target;
        if(!strncmp(query, "limit=", 6) && !saw_limit) {
            target = limit;
            saw_limit = true;
            query += 6;
        } else if(!strncmp(query, "offset=", 7) && !saw_offset) {
            target = offset;
            saw_offset = true;
            query += 7;
        } else
            return false;
        if(*query < '0' || *query > '9')
            return false;
        unsigned value = 0;
        while(*query >= '0' && *query <= '9') {
            unsigned digit = (unsigned)(*query++ - '0');
            if(value > ((unsigned)INT_MAX - digit) / 10)
                return false;
            value = value * 10 + digit;
        }
        *target = (int)value;
        if(!*query)
            break;
        if(*query != '&')
            return false;
        query++;
        if(!*query)
            return false;
    }
    return *limit >= 1 && *limit <= 100;
}
static void finish(api_response *r, db_status status, int success) {
    switch(status) {
    case DB_OK:
        r->status = success;
        r->length = strlen(r->body);
        break;
    case DB_NOT_FOUND:
        api_error(r, 404, "Item not found");
        break;
    case DB_UNAVAILABLE:
        api_error(r, 503, "Database unavailable");
        break;
    case DB_ERROR:
        api_error(r, 500, "Database operation failed");
        break;
    }
}
void items_route(PGconn *conn, const char *method, const char *path, const char *query,
                 const char *body, api_response *r) {
    memset(r, 0, sizeof *r);
    if(!strcmp(path, "/health") && !strcmp(method, "GET")) {
        r->status = 200;
        strcpy(r->body, "{\"status\":\"ok\"}");
        r->length = strlen(r->body);
        return;
    }
    bool collection = !strcmp(path, "/items");
    const char *id = !strncmp(path, "/items/", 7) ? path + 7 : NULL;
    if(!collection && !id) {
        api_error(r, 404, "Route not found");
        return;
    }
    if(id && !item_id_valid(id)) {
        api_error(r, 400, "Invalid item id");
        return;
    }
    if(!strcmp(method, "GET")) {
        if(collection) {
            int limit, offset;
            if(!pagination(query, &limit, &offset)) {
                api_error(r, 400, "Invalid pagination");
                return;
            }
            finish(r, items_list(conn, limit, offset, r->body, sizeof r->body), 200);
        } else {
            if(*query) {
                api_error(r, 400, "Query parameters are not supported here");
                return;
            }
            finish(r, items_get(conn, id, r->body, sizeof r->body), 200);
        }
        return;
    }
    if((collection && !strcmp(method, "POST")) || (id && !strcmp(method, "PATCH"))) {
        if(*query) {
            api_error(r, 400, "Query parameters are not supported here");
            return;
        }
        item_input input;
        char error[256];
        int status = item_parse(body, id != NULL, &input, error, sizeof error);
        if(status) {
            api_error(r, status, error);
            return;
        }
        finish(r,
               id ? items_patch(conn, id, &input, r->body, sizeof r->body)
                  : items_create(conn, &input, r->body, sizeof r->body),
               id ? 200 : 201);
        return;
    }
    if(id && !strcmp(method, "DELETE")) {
        if(*query) {
            api_error(r, 400, "Query parameters are not supported here");
            return;
        }
        finish(r, items_delete(conn, id), 204);
        return;
    }
    api_error(r, 405, "Method not allowed");
    snprintf(r->allow, sizeof r->allow, "%s", collection ? "GET, POST" : "GET, PATCH, DELETE");
}
