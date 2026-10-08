#include <moltest.h>
#include <moltest_mock.h>
#include <items_api/repositories/items.h>
#include <string.h>

MOCK_VALUE_FUNC(db_status, db_query, PGconn *, const char *, int, const char *const *, char *, size_t);
MOCK_VALUE_FUNC(PGconn *, db_open, const char *);
MOCK_VOID_FUNC(db_close, PGconn *);
static char sql[1024], params[4][256];
static bool missing[4];
static db_status capture(PGconn *conn, const char *statement, int count, const char *const *values,
                         char *json, size_t capacity) {
    (void)conn;
    snprintf(sql, sizeof sql, "%s", statement);
    for(int i = 0; i < count; i++) {
        missing[i] = values[i] == NULL;
        snprintf(params[i], sizeof params[i], "%s", values[i] ? values[i] : "");
    }
    if(json) snprintf(json, capacity, "{}"); return DB_OK;
}
DESCRIBE(create_binds_quotes_and_sql_as_data_without_interpolation) {
    item_input input = {.has_name=true,.has_price=true,.has_stock=true};
    strcpy(input.name, "'); DROP TABLE items; --"); strcpy(input.price, "12.30"); strcpy(input.stock, "5");
    char json[64]; db_query_mock.custom_fake = capture;
    EXPECT_EQ(DB_OK, items_create(NULL, &input, json, sizeof json));
    EXPECT_EQ(3, db_query_mock.arg2_val);
    EXPECT_STREQ(input.name, params[0]); EXPECT_STREQ("12.30", params[1]);
    EXPECT_NULL(strstr(sql, "DROP TABLE")); EXPECT_NOT_NULL(strstr(sql, "$1"));
}
DESCRIBE(patch_binds_null_for_every_omitted_field_and_does_not_touch_dates) {
    item_input input = {.has_stock=true}; strcpy(input.stock, "0");
    char json[64]; db_query_mock.custom_fake = capture;
    EXPECT_EQ(DB_OK, items_patch(NULL, "42", &input, json, sizeof json));
    EXPECT_STREQ("42", params[0]); EXPECT_TRUE(missing[1]); EXPECT_TRUE(missing[2]);
    EXPECT_FALSE(missing[3]); EXPECT_STREQ("0", params[3]);
    EXPECT_NOT_NULL(strstr(sql, "COALESCE")); EXPECT_NULL(strstr(sql, "SET updated_at"));
}
DESCRIBE(list_get_and_delete_all_use_bound_parameters) {
    char json[64]; db_query_mock.custom_fake = capture;
    EXPECT_EQ(DB_OK, items_list(NULL, 20, 10, json, sizeof json));
    EXPECT_STREQ("20", params[0]); EXPECT_STREQ("10", params[1]); EXPECT_NOT_NULL(strstr(sql, "ORDER BY id"));
    EXPECT_EQ(DB_OK, items_get(NULL, "7", json, sizeof json)); EXPECT_STREQ("7", params[0]);
    EXPECT_EQ(DB_OK, items_delete(NULL, "7")); EXPECT_STREQ("7", params[0]);
    EXPECT_NULL(db_query_mock.arg4_val);
}
DESCRIBE(repository_preserves_database_failure_statuses) {
    char json[64]; db_query_mock.return_val = DB_UNAVAILABLE;
    EXPECT_EQ(DB_UNAVAILABLE, items_get(NULL, "1", json, sizeof json));
    db_query_mock.return_val = DB_NOT_FOUND;
    EXPECT_EQ(DB_NOT_FOUND, items_delete(NULL, "1"));
}
