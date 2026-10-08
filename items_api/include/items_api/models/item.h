#ifndef ITEMS_API_MODELS_ITEM_H
#define ITEMS_API_MODELS_ITEM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ITEM_NAME_MAX 120
#define ITEM_REQUEST_MAX 4096
#define ITEM_RESPONSE_MAX 65536
typedef struct {
    bool has_name, has_price, has_stock;
    char name[ITEM_NAME_MAX + 1];
    char price[32]; /* Exact decimal text, never a binary floating-point price. */
    char stock[16];
} item_input;

/* POST needs all fields; PATCH needs at least one. Returns 0, 400 or 422. */
int item_parse(const char *json, bool patch, item_input *out, char *error, size_t size);
bool item_id_valid(const char *id);
#endif
