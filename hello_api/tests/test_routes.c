#include <moltest.h>

#include <hello_api/routes.h>

#include <string.h>

/* The API without a server: what each request line answers. */

DESCRIBE(the_root_and_hello_greet_the_world) {
    api_response r;
    EXPECT_EQ(200, api_route("GET", "/", &r));
    EXPECT_STREQ("{\"message\":\"Hello, world!\"}", r.body);
    EXPECT_EQ(strlen(r.body), r.length);
    EXPECT_EQ(200, api_route("GET", "/hello", &r));
    EXPECT_STREQ("{\"message\":\"Hello, world!\"}", r.body);
}

DESCRIBE(a_name_in_the_path_is_greeted) {
    api_response r;
    EXPECT_EQ(200, api_route("GET", "/hello/ana", &r));
    EXPECT_STREQ("{\"message\":\"Hello, ana!\"}", r.body);
    EXPECT_EQ(200, api_route("GET", "/hello/Jo_2-b", &r));
    EXPECT_STREQ("{\"message\":\"Hello, Jo_2-b!\"}", r.body);
}

DESCRIBE(the_query_string_is_ignored) {
    api_response r;
    EXPECT_EQ(200, api_route("GET", "/hello/ana?x=1", &r));
    EXPECT_STREQ("{\"message\":\"Hello, ana!\"}", r.body);
    EXPECT_EQ(200, api_route("GET", "/health?verbose", &r));
}

DESCRIBE(health_says_ok) {
    api_response r;
    EXPECT_EQ(200, api_route("GET", "/health", &r));
    EXPECT_STREQ("{\"status\":\"ok\"}", r.body);
    EXPECT_EQ(200, api_route("HEAD", "/health", &r));
}

DESCRIBE(a_name_that_would_need_escaping_is_refused) {
    api_response r;
    EXPECT_EQ(400, api_route("GET", "/hello/a\"b", &r));
    EXPECT_EQ(400, api_route("GET", "/hello/a%20b", &r));
    EXPECT_EQ(400, api_route("GET", "/hello/a/b", &r));
    EXPECT_EQ(400, api_route("GET", "/hello/", &r));
}

DESCRIBE(a_name_longer_than_the_limit_is_refused) {
    char path[64] = "/hello/";
    memset(path + 7, 'x', API_NAME_MAX);
    api_response r;
    EXPECT_EQ(200, api_route("GET", path, &r));
    path[7 + API_NAME_MAX] = 'x';
    path[8 + API_NAME_MAX] = '\0';
    EXPECT_EQ(400, api_route("GET", path, &r));
}

DESCRIBE(an_unknown_path_is_not_found) {
    api_response r;
    EXPECT_EQ(404, api_route("GET", "/nope", &r));
    EXPECT_STREQ("{\"error\":\"not found\"}", r.body);
    EXPECT_EQ(404, api_route("GET", "/helloworld", &r));
    EXPECT_EQ(404, api_route("POST", "/nope", &r));
}

DESCRIBE(only_get_and_head_are_allowed) {
    api_response r;
    EXPECT_EQ(405, api_route("POST", "/hello", &r));
    EXPECT_EQ(405, api_route("DELETE", "/hello/ana", &r));
    EXPECT_STREQ("{\"error\":\"method not allowed\"}", r.body);
}
