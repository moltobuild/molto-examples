#include <items_api/repositories/items.h>
#include <stdio.h>

/* PostgreSQL encodes strings and exact numeric values as JSON. Request values
   are always parameters; no user text is concatenated into SQL. */
#define ITEM_JSON                                                                                  \
    "json_build_object('id',id,'name',name,'price',price,"                                         \
    "'stock',stock,'updated_at',updated_at,'created_at',created_at)::text"

db_status items_list(PGconn *conn, int limit, int offset, char *json, size_t capacity) {
    char count[16], skip[16];
    snprintf(count, sizeof count, "%d", limit);
    snprintf(skip, sizeof skip, "%d", offset);
    const char *values[] = {count, skip};
    return db_query(conn,
                    "SELECT COALESCE(json_agg(row_to_json(page) ORDER BY id), '[]'::json)::text "
                    "FROM (SELECT id,name,price,stock,updated_at,created_at FROM items "
                    "ORDER BY id LIMIT $1::int OFFSET $2::int) page",
                    2, values, json, capacity);
}
db_status items_get(PGconn *conn, const char *id, char *json, size_t capacity) {
    const char *values[] = {id};
    return db_query(conn, "SELECT " ITEM_JSON " FROM items WHERE id=$1::bigint", 1, values, json,
                    capacity);
}
db_status items_create(PGconn *conn, const item_input *input, char *json, size_t capacity) {
    const char *values[] = {input->name, input->price, input->stock};
    return db_query(conn,
                    "INSERT INTO items(name,price,stock) VALUES($1,$2::numeric,$3::int) "
                    "RETURNING " ITEM_JSON,
                    3, values, json, capacity);
}
db_status items_patch(PGconn *conn, const char *id, const item_input *input, char *json,
                      size_t capacity) {
    const char *values[] = {id, input->has_name ? input->name : NULL,
                            input->has_price ? input->price : NULL,
                            input->has_stock ? input->stock : NULL};
    return db_query(conn,
                    "UPDATE items SET name=COALESCE($2,name), "
                    "price=COALESCE($3::numeric,price),stock=COALESCE($4::int,stock) "
                    "WHERE id=$1::bigint RETURNING " ITEM_JSON,
                    4, values, json, capacity);
}
db_status items_delete(PGconn *conn, const char *id) {
    const char *values[] = {id};
    return db_query(conn, "DELETE FROM items WHERE id=$1::bigint RETURNING id", 1, values, NULL, 0);
}
