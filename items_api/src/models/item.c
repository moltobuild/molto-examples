#include <ctype.h>
#include <items_api/models/item.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const unsigned char *p;
} reader;
static void space(reader *r) {
    while(*r->p == ' ' || *r->p == '\t' || *r->p == '\r' || *r->p == '\n')
        r->p++;
}
static int hex(unsigned char c) {
    if(c >= '0' && c <= '9')
        return c - '0';
    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
static bool unicode(reader *r, unsigned *value) {
    *value = 0;
    for(int i = 0; i < 4; i++) {
        int digit = hex(*r->p);
        if(digit < 0)
            return false;
        *value = (*value << 4) | (unsigned)digit;
        r->p++;
    }
    return true;
}
/* Decode JSON strings, including surrogate pairs, into bounded UTF-8. */
static bool string(reader *r, char *out, size_t size) {
    if(*r->p++ != '"')
        return false;
    size_t n = 0;
    while(*r->p && *r->p != '"') {
        unsigned c = *r->p++;
        if(c < 32)
            return false;
        if(c == '\\') {
            c = *r->p++;
            switch(c) {
            case '"':
            case '\\':
            case '/':
                break;
            case 'b':
                c = 8;
                break;
            case 'f':
                c = 12;
                break;
            case 'n':
                c = 10;
                break;
            case 'r':
                c = 13;
                break;
            case 't':
                c = 9;
                break;
            case 'u': {
                if(!unicode(r, &c) || !c)
                    return false;
                if(c >= 0xd800 && c <= 0xdbff) {
                    if(r->p[0] != '\\' || r->p[1] != 'u')
                        return false;
                    r->p += 2;
                    unsigned low;
                    if(!unicode(r, &low) || low < 0xdc00 || low > 0xdfff)
                        return false;
                    c = 0x10000 + ((c - 0xd800) << 10) + low - 0xdc00;
                } else if(c >= 0xdc00 && c <= 0xdfff)
                    return false;
                unsigned char bytes[4];
                size_t count;
                if(c < 0x80) {
                    bytes[0] = (unsigned char)c;
                    count = 1;
                } else if(c < 0x800) {
                    bytes[0] = (unsigned char)(0xc0 | (c >> 6));
                    bytes[1] = (unsigned char)(0x80 | (c & 63));
                    count = 2;
                } else if(c < 0x10000) {
                    bytes[0] = (unsigned char)(0xe0 | (c >> 12));
                    bytes[1] = (unsigned char)(0x80 | ((c >> 6) & 63));
                    bytes[2] = (unsigned char)(0x80 | (c & 63));
                    count = 3;
                } else {
                    bytes[0] = (unsigned char)(0xf0 | (c >> 18));
                    bytes[1] = (unsigned char)(0x80 | ((c >> 12) & 63));
                    bytes[2] = (unsigned char)(0x80 | ((c >> 6) & 63));
                    bytes[3] = (unsigned char)(0x80 | (c & 63));
                    count = 4;
                }
                if(n + count >= size)
                    return false;
                memcpy(out + n, bytes, count);
                n += count;
                continue;
            }
            default:
                return false;
            }
        }
        if(!c || n + 1 >= size)
            return false;
        out[n++] = (char)c;
    }
    if(*r->p != '"')
        return false;
    r->p++;
    out[n] = '\0';
    return true;
}
static bool number(reader *r, char *out, size_t size) {
    const unsigned char *start = r->p;
    if(*r->p == '-')
        r->p++;
    if(*r->p == '0')
        r->p++;
    else {
        if(*r->p < '1' || *r->p > '9')
            return false;
        while(isdigit(*r->p))
            r->p++;
    }
    if(*r->p == '.') {
        r->p++;
        if(!isdigit(*r->p))
            return false;
        while(isdigit(*r->p))
            r->p++;
    }
    if(*r->p == 'e' || *r->p == 'E') {
        r->p++;
        if(*r->p == '+' || *r->p == '-')
            r->p++;
        if(!isdigit(*r->p))
            return false;
        while(isdigit(*r->p))
            r->p++;
    }
    size_t n = (size_t)(r->p - start);
    if(n >= size)
        return false;
    memcpy(out, start, n);
    out[n] = '\0';
    return true;
}
bool item_id_valid(const char *id) {
    if(*id < '1' || *id > '9')
        return false;
    uint64_t value = 0;
    for(const unsigned char *p = (const unsigned char *)id; *p; p++) {
        if(!isdigit(*p) || value > ((uint64_t)INT64_MAX - (*p - '0')) / 10)
            return false;
        value = value * 10 + (*p - '0');
    }
    return true;
}
static bool valid_price(const char *price) {
    size_t digits = 0, cents = 0;
    const unsigned char *p = (const unsigned char *)price;
    while(isdigit(*p)) {
        digits++;
        p++;
    }
    if(!digits || digits > 10)
        return false;
    if(*p == '.') {
        p++;
        while(isdigit(*p)) {
            cents++;
            p++;
        }
        if(!cents || cents > 2)
            return false;
    }
    return !*p;
}
static bool valid_stock(const char *stock) {
    uint64_t value = 0;
    if(!*stock)
        return false;
    for(const unsigned char *p = (const unsigned char *)stock; *p; p++) {
        if(!isdigit(*p) || value > ((uint64_t)INT_MAX - (*p - '0')) / 10)
            return false;
        value = value * 10 + (*p - '0');
    }
    return true;
}
static bool valid_name(const char *name) {
    bool nonspace = false;
    for(const unsigned char *p = (const unsigned char *)name; *p; p++) {
        if(*p < 32 || *p == 127)
            return false;
        nonspace = nonspace || !isspace(*p);
        /* Reject malformed UTF-8 before sending it to a UTF-8 database. */
        if(*p >= 128) {
            unsigned c = *p;
            int n;
            unsigned minimum;
            if(c >= 0xc2 && c <= 0xdf) {
                n = 1;
                c &= 31;
                minimum = 0x80;
            } else if(c >= 0xe0 && c <= 0xef) {
                n = 2;
                c &= 15;
                minimum = 0x800;
            } else if(c >= 0xf0 && c <= 0xf4) {
                n = 3;
                c &= 7;
                minimum = 0x10000;
            } else
                return false;
            for(int i = 0; i < n; i++) {
                p++;
                if((*p & 0xc0) != 0x80)
                    return false;
                c = (c << 6) | (*p & 63);
            }
            if(c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
                return false;
        }
    }
    return nonspace;
}
int item_parse(const char *json, bool patch, item_input *out, char *error, size_t size) {
    memset(out, 0, sizeof *out);
    reader r = {(const unsigned char *)json};
    space(&r);
    if(*r.p++ != '{')
        goto malformed;
    space(&r);
    if(*r.p != '}')
        for(;;) {
            char key[32];
            if(!string(&r, key, sizeof key))
                goto malformed;
            space(&r);
            if(*r.p++ != ':')
                goto malformed;
            space(&r);
            if(!strcmp(key, "name")) {
                if(out->has_name)
                    goto invalid;
                out->has_name = true;
                if(!string(&r, out->name, sizeof out->name))
                    goto invalid;
            } else if(!strcmp(key, "price")) {
                if(out->has_price)
                    goto invalid;
                out->has_price = true;
                if(!number(&r, out->price, sizeof out->price))
                    goto invalid;
            } else if(!strcmp(key, "stock")) {
                if(out->has_stock)
                    goto invalid;
                out->has_stock = true;
                if(!number(&r, out->stock, sizeof out->stock))
                    goto invalid;
            } else
                goto invalid;
            space(&r);
            if(*r.p == '}')
                break;
            if(*r.p++ != ',')
                goto malformed;
            space(&r);
        }
    if(*r.p++ != '}')
        goto malformed;
    space(&r);
    if(*r.p)
        goto malformed;
    if((!patch && (!out->has_name || !out->has_price || !out->has_stock)) ||
       (patch && !out->has_name && !out->has_price && !out->has_stock) ||
       (out->has_name && !valid_name(out->name)) || (out->has_price && !valid_price(out->price)) ||
       (out->has_stock && !valid_stock(out->stock)))
        goto invalid;
    return 0;
malformed:
    snprintf(error, size, "Expected one JSON object");
    return 400;
invalid:
    snprintf(error, size,
             "Use name (1-120 UTF-8 bytes), price (0-9999999999.99, up to two decimals), and stock "
             "(0-2147483647); fields must be unique");
    return 422;
}
