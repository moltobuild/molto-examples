#include <errno.h>
#include <items_api/config/env.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool assign(char *out, size_t capacity, const char *value) {
    size_t length = strlen(value);
    if(length >= capacity)
        return false;
    memcpy(out, value, length + 1);
    return true;
}
bool config_read(const char *path, app_config *config, char *error, size_t size) {
    memset(config, 0, sizeof *config);
    char port[32] = "8080";
    FILE *file = fopen(path, "r");
    if(!file && errno != ENOENT)
        goto invalid;
    if(file) {
        char line[1200];
        bool valid = true;
        while(fgets(line, sizeof line, file)) {
            size_t length = strlen(line);
            if(length == sizeof line - 1 && line[length - 1] != '\n') {
                valid = false;
                break;
            }
            line[strcspn(line, "\r\n")] = '\0';
            char *key = line;
            while(*key == ' ' || *key == '\t')
                key++;
            if(!*key || *key == '#')
                continue;
            char *equal = strchr(key, '=');
            if(!equal) {
                valid = false;
                break;
            }
            *equal = '\0';
            char *value = equal + 1;
            char *end = equal;
            while(end > key && (end[-1] == ' ' || end[-1] == '\t'))
                *--end = '\0';
            while(*value == ' ' || *value == '\t')
                value++;
            end = value + strlen(value);
            while(end > value && (end[-1] == ' ' || end[-1] == '\t'))
                *--end = '\0';
            if(*value == '\'' || *value == '"') {
                if(end <= value + 1 || end[-1] != *value) {
                    valid = false;
                    break;
                }
                value++;
                end[-1] = '\0';
            }
            if(!strcmp(key, "DATABASE_URL"))
                valid = assign(config->database_url, sizeof config->database_url, value);
            else if(!strcmp(key, "PORT"))
                valid = assign(port, sizeof port, value);
            else
                valid = false;
            if(!valid)
                break;
        }
        valid = valid && !ferror(file);
        fclose(file);
        if(!valid)
            goto invalid;
    }
    const char *url_env = getenv("DATABASE_URL"), *port_env = getenv("PORT");
    if(url_env && !assign(config->database_url, sizeof config->database_url, url_env))
        goto invalid;
    if(port_env && !assign(port, sizeof port, port_env))
        goto invalid;
    char *end;
    errno = 0;
    long number = strtol(port, &end, 10);
    if(!*config->database_url || !*port || *end || errno || number < 1 || number > 65535)
        goto invalid;
    config->port = (int)number;
    return true;
invalid:
    snprintf(error, size, "Invalid configuration: set DATABASE_URL and PORT (1-65535) in .env");
    return false;
}
