#include <moltest.h>
#include <items_api/repositories/items.h>

DESCRIBE(a_missing_connection_returns_unavailable_without_stale_data) {
    char json[32] = "old";
    EXPECT_EQ(DB_UNAVAILABLE, items_get(NULL, "1", json, sizeof json));
    EXPECT_STREQ("", json);
    db_close(NULL);
}
