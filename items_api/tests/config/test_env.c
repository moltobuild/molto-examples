#include <moltest.h>
#include <items_api/config/env.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool from_text(const char *text, app_config *config) {
    char path[MOLTEST_PATH], error[256];
    if(!moltest_temp_file("items_env", path, sizeof path)) return false;
    FILE *file = fopen(path, "w");
    if(!file) return false;
    fputs(text, file); fclose(file);
    bool ok = config_read(path, config, error, sizeof error); remove(path); return ok;
}
DESCRIBE(dotenv_loads_a_connection_url_and_port_without_shell_expansion) {
    if(getenv("DATABASE_URL") || getenv("PORT")) SKIP("Environment overrides .env in this test");
    app_config config;
    EXPECT_TRUE(from_text("# local settings\n DATABASE_URL = 'postgresql://items:password@localhost/items'\nPORT=9090\n", &config));
    EXPECT_EQ(9090, config.port); EXPECT_STREQ("postgresql://items:password@localhost/items", config.database_url);
    EXPECT_TRUE(from_text("DATABASE_URL=postgresql://localhost/items\r\n", &config)); EXPECT_EQ(8080, config.port);
}
DESCRIBE(dotenv_refuses_typos_missing_values_and_invalid_ports) {
    if(getenv("DATABASE_URL") || getenv("PORT")) SKIP("Environment overrides .env in this test");
    const char *bad[] = {"", "DATABASE_URL=\n", "DATABASE_URL=x\nPORT=0\n", "DATABASE_URL=x\nPORT=65536\n",
        "DATABASE_URL=x\nPORT=abc\n", "DATABASE_URL=x\nPORT=\n", "DATABASE=x\n", "DATABASE_URL\n", "DATABASE_URL='x\n"};
    app_config config;
    for(size_t i=0;i<sizeof bad/sizeof *bad;i++) EXPECT_FALSE(from_text(bad[i], &config));
}
