#include <moltest.h>
#include <items_api/models/item.h>
#include <string.h>

DESCRIBE(create_requires_all_three_fields_and_keeps_exact_decimals) {
    item_input item; char error[256];
    EXPECT_EQ(0, item_parse("{\"name\":\"Keyboard\",\"price\":49.90,\"stock\":8}", false, &item, error, sizeof error));
    EXPECT_STREQ("Keyboard", item.name); EXPECT_STREQ("49.90", item.price);
    EXPECT_STREQ("8", item.stock);
    EXPECT_TRUE(item.has_name && item.has_price && item.has_stock);
    EXPECT_EQ(422, item_parse("{\"name\":\"Keyboard\"}", false, &item, error, sizeof error));
}
DESCRIBE(patch_accepts_each_field_without_changing_omitted_fields) {
    item_input item; char error[256];
    EXPECT_EQ(0, item_parse("{\"stock\":0}", true, &item, error, sizeof error));
    EXPECT_TRUE(item.has_stock); EXPECT_FALSE(item.has_price); EXPECT_FALSE(item.has_name);
    EXPECT_EQ(0, item_parse("{\"price\":0}", true, &item, error, sizeof error));
    EXPECT_TRUE(item.has_price); EXPECT_FALSE(item.has_stock);
    EXPECT_EQ(0, item_parse("{\"name\":\"Mouse\"}", true, &item, error, sizeof error));
    EXPECT_TRUE(item.has_name); EXPECT_FALSE(item.has_price);
}
DESCRIBE(unknown_duplicate_null_and_readonly_fields_are_rejected) {
    const char *bad[] = {"{}", "{\"stock\":null}", "{\"stock\":true}",
        "{\"stock\":1,\"stock\":2}", "{\"id\":1}", "{\"created_at\":\"now\"}",
        "{\"updated_at\":\"now\"}", "{\"name\":[]}", "{\"name\":{}}", "{\"price\":\"12.00\"}"};
    for(size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        item_input item; char error[256];
        EXPECT_EQ(422, item_parse(bad[i], true, &item, error, sizeof error));
    }
}
DESCRIBE(prices_and_stock_have_exact_nonnegative_bounds) {
    const char *bad[] = {"{\"price\":-1}", "{\"price\":1.001}", "{\"price\":1e2}",
        "{\"price\":10000000000}", "{\"stock\":-1}", "{\"stock\":1.5}",
        "{\"stock\":2147483648}", "{\"stock\":1e3}"};
    item_input item; char error[256];
    for(size_t i = 0; i < sizeof bad / sizeof *bad; i++)
        EXPECT_EQ(422, item_parse(bad[i], true, &item, error, sizeof error));
    EXPECT_EQ(0, item_parse("{\"price\":9999999999.99,\"stock\":2147483647}", true, &item, error, sizeof error));
}
DESCRIBE(malformed_objects_cannot_smuggle_another_document_or_number) {
    const char *bad[] = {"", "[]", "null", "{", "{\"stock\":1}{}", "{\"stock\":01}",
        "{\"stock\":1,}", "{\"stock\":1", "{\"stock\" 1}", "{\"stock\":1 \"price\":2}", "{\"stock\":1}#comment"};
    for(size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        item_input item; char error[256];
        EXPECT_NE(0, item_parse(bad[i], true, &item, error, sizeof error));
    }
}
DESCRIBE(names_decode_json_escapes_unicode_and_surrogate_pairs) {
    item_input item; char error[256];
    EXPECT_EQ(0, item_parse("{\"name\":\"Caf\\u00e9 \\ud83d\\ude00 \\\"cup\\\"\\/\\\\\"}", true, &item, error, sizeof error));
    EXPECT_STREQ("Café 😀 \"cup\"/\\", item.name);
    EXPECT_EQ(0, item_parse(" \n{ \"name\" : \"Café\" } \t", true, &item, error, sizeof error));
}
DESCRIBE(names_reject_controls_invalid_utf8_and_unpaired_surrogates) {
    const char *bad[] = {"{\"name\":\"\"}", "{\"name\":\"   \"}",
        "{\"name\":\"a\\n\"}", "{\"name\":\"\\u0000\"}",
        "{\"name\":\"\\ud800\"}", "{\"name\":\"\\udc00\"}",
        "{\"name\":\"\\ud800\\u0001\"}", "{\"name\":\"\\uZZZZ\"}",
        "{\"name\":\"\\x\"}", "{\"name\":\"\xc0\xaf\"}", "{\"name\":\"\xed\xa0\x80\"}",
        "{\"name\":\"\xf4\x90\x80\x80\"}", "{\"name\":\"\xe2\"}"};
    for(size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        item_input item; char error[256];
        EXPECT_NE(0, item_parse(bad[i], true, &item, error, sizeof error));
    }
}
DESCRIBE(names_are_bounded_by_utf8_bytes) {
    char body[256]; item_input item; char error[256];
    strcpy(body, "{\"name\":\"");
    memset(body + 9, 'a', 120); strcpy(body + 129, "\"}");
    EXPECT_EQ(0, item_parse(body, true, &item, error, sizeof error));
    body[129] = 'b'; strcpy(body + 130, "\"}");
    EXPECT_EQ(422, item_parse(body, true, &item, error, sizeof error));
}
DESCRIBE(ids_are_positive_bigints_with_no_extra_path_components) {
    EXPECT_TRUE(item_id_valid("1")); EXPECT_TRUE(item_id_valid("9223372036854775807"));
    const char *bad[] = {"", "0", "01", "-1", "+1", "1/other", "1?x", "1abc", "9223372036854775808"};
    for(size_t i = 0; i < sizeof bad / sizeof *bad; i++) EXPECT_FALSE(item_id_valid(bad[i]));
}
