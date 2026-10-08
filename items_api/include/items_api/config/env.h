#ifndef ITEMS_API_CONFIG_ENV_H
#define ITEMS_API_CONFIG_ENV_H
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char database_url[1024];
    int port;
} app_config;
/* Plain KEY=value only; no shell expansion. Environment overrides the file. */
bool config_read(const char *path, app_config *config, char *error, size_t size);
#endif
