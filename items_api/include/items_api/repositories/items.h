#ifndef ITEMS_API_REPOSITORIES_ITEMS_H
#define ITEMS_API_REPOSITORIES_ITEMS_H
#include <items_api/db/database.h>
#include <items_api/models/item.h>

db_status items_list(PGconn *conn, int limit, int offset, char *json, size_t capacity);
db_status items_get(PGconn *conn, const char *id, char *json, size_t capacity);
db_status items_create(PGconn *conn, const item_input *input, char *json, size_t capacity);
db_status items_patch(PGconn *conn, const char *id, const item_input *input, char *json,
                      size_t capacity);
db_status items_delete(PGconn *conn, const char *id);
#endif
