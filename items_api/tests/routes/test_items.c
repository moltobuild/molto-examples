#include <moltest.h>
#include <moltest_mock.h>
#include <items_api/routes/items.h>
#include <items_api/repositories/items.h>
#include <string.h>

MOCK_VALUE_FUNC(db_status, items_list, PGconn *, int, int, char *, size_t);
MOCK_VALUE_FUNC(db_status, items_get, PGconn *, const char *, char *, size_t);
MOCK_VALUE_FUNC(db_status, items_create, PGconn *, const item_input *, char *, size_t);
MOCK_VALUE_FUNC(db_status, items_patch, PGconn *, const char *, const item_input *, char *, size_t);
MOCK_VALUE_FUNC(db_status, items_delete, PGconn *, const char *);

static db_status found(PGconn *conn, const char *id, char *json, size_t size) {
    (void)conn; (void)id;
    snprintf(json, size, "{\"id\":1,\"name\":\"Keyboard\",\"price\":49.90,\"stock\":8}"); return DB_OK;
}
static db_status page(PGconn *conn, int limit, int offset, char *json, size_t size) {
    (void)conn; (void)limit; (void)offset; snprintf(json, size, "[]"); return DB_OK;
}
static item_input received;
static db_status created(PGconn *conn, const item_input *input, char *json, size_t size) {
    received = *input; return found(conn, "1", json, size);
}
static db_status patched(PGconn *conn, const char *id, const item_input *input, char *json, size_t size) {
    received = *input; return found(conn, id, json, size);
}
DESCRIBE(list_and_get_use_the_repository_and_return_json) {
    api_response r; items_list_mock.custom_fake = page; items_get_mock.custom_fake = found;
    items_route(NULL, "GET", "/items", "", "", &r);
    EXPECT_EQ(200, r.status); EXPECT_STREQ("[]", r.body);
    EXPECT_EQ(20, items_list_mock.arg1_val); EXPECT_EQ(0, items_list_mock.arg2_val);
    items_route(NULL, "GET", "/items", "limit=5&offset=10", "", &r);
    EXPECT_EQ(200, r.status); EXPECT_EQ(5, items_list_mock.arg1_val); EXPECT_EQ(10, items_list_mock.arg2_val);
    items_route(NULL, "GET", "/items/1", "", "", &r);
    EXPECT_EQ(200, r.status); EXPECT_EQ(strlen(r.body), r.length);
    EXPECT_EQ(1u, items_get_mock.call_count);
}
DESCRIBE(invalid_pagination_and_ids_do_not_touch_the_database) {
    const char *queries[] = {"limit=0", "limit=101", "offset=-1", "offset=2147483648", "limit=1&", "limit=2&limit=3", "x=1", "limit=", "limit=2x"};
    api_response r;
    for(size_t i = 0; i < sizeof queries / sizeof *queries; i++) {
        items_route(NULL, "GET", "/items", queries[i], "", &r); EXPECT_EQ(400, r.status);
    }
    items_route(NULL, "GET", "/items/0", "", "", &r); EXPECT_EQ(400, r.status);
    items_route(NULL, "GET", "/items/1", "limit=1", "", &r); EXPECT_EQ(400, r.status);
    EXPECT_EQ(0u, items_list_mock.call_count); EXPECT_EQ(0u, items_get_mock.call_count);
}
DESCRIBE(post_creates_an_item_and_patch_preserves_omitted_fields) {
    api_response r; items_create_mock.custom_fake = created; items_patch_mock.custom_fake = patched;
    items_route(NULL, "POST", "/items", "", "{\"name\":\"Keyboard\",\"price\":49.90,\"stock\":8}", &r);
    EXPECT_EQ(201, r.status); EXPECT_STREQ("49.90", received.price);
    items_route(NULL, "PATCH", "/items/1", "", "{\"stock\":0}", &r);
    EXPECT_EQ(200, r.status); EXPECT_TRUE(received.has_stock);
    EXPECT_FALSE(received.has_price); EXPECT_FALSE(received.has_name);
    EXPECT_STREQ("0", received.stock);
}
DESCRIBE(bad_input_never_reaches_create_or_patch) {
    api_response r;
    items_route(NULL, "POST", "/items", "", "{}", &r); EXPECT_EQ(422, r.status);
    items_route(NULL, "PATCH", "/items/1", "", "[]", &r); EXPECT_EQ(400, r.status);
    items_route(NULL, "PATCH", "/items/1", "x=1", "{}", &r); EXPECT_EQ(400, r.status);
    EXPECT_EQ(0u, items_create_mock.call_count); EXPECT_EQ(0u, items_patch_mock.call_count);
}
DESCRIBE(database_errors_are_safe_and_missing_items_are_404) {
    api_response r;
    const db_status statuses[] = {DB_NOT_FOUND, DB_UNAVAILABLE, DB_ERROR};
    const int codes[] = {404, 503, 500};
    for(size_t i = 0; i < 3; i++) {
        items_get_mock.return_val = statuses[i];
        items_route(NULL, "GET", "/items/1", "", "", &r); EXPECT_EQ(codes[i], r.status);
        EXPECT_NOT_NULL(strstr(r.body, "error"));
    }
}
DESCRIBE(delete_returns_no_body_and_reports_a_missing_item) {
    api_response r;
    items_route(NULL, "DELETE", "/items/1", "", "", &r);
    EXPECT_EQ(204, r.status); EXPECT_EQ(0u, r.length);
    items_delete_mock.return_val = DB_NOT_FOUND;
    items_route(NULL, "DELETE", "/items/1", "", "", &r); EXPECT_EQ(404, r.status);
    items_route(NULL, "DELETE", "/items/1", "x=1", "", &r); EXPECT_EQ(400, r.status);
}
DESCRIBE(unsupported_methods_name_the_allowed_methods) {
    api_response r;
    items_route(NULL, "PUT", "/items/1", "", "", &r);
    EXPECT_EQ(405, r.status); EXPECT_STREQ("GET, PATCH, DELETE", r.allow);
    items_route(NULL, "DELETE", "/items", "", "", &r);
    EXPECT_EQ(405, r.status); EXPECT_STREQ("GET, POST", r.allow);
    items_route(NULL, "GET", "/unknown", "", "", &r); EXPECT_EQ(404, r.status);
    items_route(NULL, "GET", "/health", "", "", &r); EXPECT_EQ(200, r.status);
}
