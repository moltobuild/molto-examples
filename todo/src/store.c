#include <todo/store.h>

#include <sqlite3.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct todo_store {
    sqlite3 *db;
};

/* The schema, versioned through PRAGMA user_version so a later release can
   tell an old database from a new one and migrate it. */
#define SCHEMA_VERSION 1

static const char *const SCHEMA =
    "CREATE TABLE IF NOT EXISTS todos ("
    "    id         INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    title      TEXT    NOT NULL CHECK (length(title) > 0),"
    "    done       INTEGER NOT NULL DEFAULT 0 CHECK (done IN (0, 1)),"
    "    created_at TEXT    NOT NULL DEFAULT (datetime('now'))"
    ");";

/* --- statements --- */

/* Prepare `sql`, bind the given values in order and step it once. Every
   statement here changes or reads at most one row of state, so one helper
   serves them all. `kinds` spells the bindings: 'i' a long long, 't' text. */
static bool run(todo_store *store, const char *sql, const char *kinds, ...) {
    sqlite3_stmt *stmt = NULL;
    if(sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return false;

    va_list args;
    va_start(args, kinds);
    bool ok = true;
    for(int i = 0; ok && kinds[i] != '\0'; i++) {
        if(kinds[i] == 'i')
            ok = sqlite3_bind_int64(stmt, i + 1, va_arg(args, long long)) == SQLITE_OK;
        else
            ok = sqlite3_bind_text(stmt, i + 1, va_arg(args, const char *), -1, SQLITE_TRANSIENT) ==
                 SQLITE_OK;
    }
    va_end(args);

    const int stepped = ok ? sqlite3_step(stmt) : SQLITE_ERROR;
    sqlite3_finalize(stmt);
    return stepped == SQLITE_DONE || stepped == SQLITE_ROW;
}

/* True when the last statement changed exactly one row: an id that names no
   task is a failure, not a silent success. */
static bool changed_one(const todo_store *store) { return sqlite3_changes(store->db) == 1; }

/* --- opening --- */

static bool migrate(todo_store *store) {
    sqlite3_stmt *stmt = NULL;
    int version = 0;
    if(sqlite3_prepare_v2(store->db, "PRAGMA user_version", -1, &stmt, NULL) != SQLITE_OK)
        return false;
    if(sqlite3_step(stmt) == SQLITE_ROW)
        version = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if(version > SCHEMA_VERSION)
        return false; /* written by a newer release: do not guess at it */
    if(version == SCHEMA_VERSION)
        return true;

    char pragma[64];
    snprintf(pragma, sizeof pragma, "PRAGMA user_version = %d", SCHEMA_VERSION);
    return sqlite3_exec(store->db, SCHEMA, NULL, NULL, NULL) == SQLITE_OK &&
           sqlite3_exec(store->db, pragma, NULL, NULL, NULL) == SQLITE_OK;
}

todo_store *todo_store_open(const char *path, char *err, size_t err_size) {
    todo_store *store = calloc(1, sizeof *store);
    if(store == NULL) {
        snprintf(err, err_size, "out of memory");
        return NULL;
    }
    if(sqlite3_open(path, &store->db) != SQLITE_OK) {
        snprintf(err, err_size, "cannot open %s: %s", path, sqlite3_errmsg(store->db));
        todo_store_close(store);
        return NULL;
    }
    if(!migrate(store)) {
        snprintf(err, err_size, "cannot prepare %s: %s", path, sqlite3_errmsg(store->db));
        todo_store_close(store);
        return NULL;
    }
    return store;
}

void todo_store_close(todo_store *store) {
    if(store == NULL)
        return;
    sqlite3_close(store->db);
    free(store);
}

const char *todo_store_error(const todo_store *store) { return sqlite3_errmsg(store->db); }

/* --- titles --- */

/* `title` without the spaces around it, into `out`. False when nothing is left
   or what is left does not fit. */
static bool clean_title(const char *title, char *out, size_t size) {
    if(title == NULL)
        return false;
    while(isspace((unsigned char)*title))
        title++;
    size_t length = strlen(title);
    while(length > 0 && isspace((unsigned char)title[length - 1]))
        length--;
    if(length == 0 || length > TODO_TITLE_MAX || length >= size)
        return false;
    memcpy(out, title, length);
    out[length] = '\0';
    return true;
}

/* --- changing tasks --- */

bool todo_add(todo_store *store, const char *title, long long *id_out) {
    char clean[TODO_TITLE_MAX + 1];
    if(!clean_title(title, clean, sizeof clean))
        return false;
    if(!run(store, "INSERT INTO todos (title) VALUES (?)", "t", clean))
        return false;
    if(id_out != NULL)
        *id_out = sqlite3_last_insert_rowid(store->db);
    return true;
}

bool todo_set_done(todo_store *store, long long id, bool done) {
    return run(store, "UPDATE todos SET done = ? WHERE id = ?", "ii", (long long)done, id) &&
           changed_one(store);
}

bool todo_rename(todo_store *store, long long id, const char *title) {
    char clean[TODO_TITLE_MAX + 1];
    return clean_title(title, clean, sizeof clean) &&
           run(store, "UPDATE todos SET title = ? WHERE id = ?", "ti", clean, id) &&
           changed_one(store);
}

bool todo_remove(todo_store *store, long long id) {
    return run(store, "DELETE FROM todos WHERE id = ?", "i", id) && changed_one(store);
}

int todo_clear_done(todo_store *store) {
    if(!run(store, "DELETE FROM todos WHERE done = 1", ""))
        return -1;
    return sqlite3_changes(store->db);
}

/* --- reading tasks --- */

/* strdup, which C17 does not have and glibc hides under a strict -std. */
static char *copy_text(const char *text) {
    const size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if(copy != NULL)
        memcpy(copy, text, length);
    return copy;
}

bool todo_list_get(todo_store *store, todo_filter filter, todo_list *out) {
    out->items = NULL;
    out->count = 0;

    static const char *const QUERIES[] = {
        [todo_filter_all] = "SELECT id, title, done FROM todos ORDER BY id",
        [todo_filter_active] = "SELECT id, title, done FROM todos WHERE done = 0 ORDER BY id",
        [todo_filter_done] = "SELECT id, title, done FROM todos WHERE done = 1 ORDER BY id",
    };
    sqlite3_stmt *stmt = NULL;
    if(sqlite3_prepare_v2(store->db, QUERIES[filter], -1, &stmt, NULL) != SQLITE_OK)
        return false;

    size_t capacity = 0;
    int stepped = SQLITE_DONE;
    bool ok = true;
    while(ok && (stepped = sqlite3_step(stmt)) == SQLITE_ROW) {
        if(out->count == capacity) {
            capacity = capacity == 0 ? 16 : capacity * 2;
            todo_item *grown = realloc(out->items, capacity * sizeof *grown);
            if(grown == NULL) {
                ok = false;
                break;
            }
            out->items = grown;
        }
        const char *title = (const char *)sqlite3_column_text(stmt, 1);
        todo_item *item = &out->items[out->count];
        item->id = sqlite3_column_int64(stmt, 0);
        item->done = sqlite3_column_int(stmt, 2) != 0;
        item->title = copy_text(title != NULL ? title : "");
        if(item->title == NULL)
            ok = false;
        else
            out->count++;
    }
    if(ok && stepped != SQLITE_DONE)
        ok = false;
    sqlite3_finalize(stmt);
    if(!ok)
        todo_list_free(out);
    return ok;
}

void todo_list_free(todo_list *list) {
    for(size_t i = 0; i < list->count; i++)
        free(list->items[i].title);
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

long long todo_count_active(todo_store *store) {
    sqlite3_stmt *stmt = NULL;
    long long count = -1;
    if(sqlite3_prepare_v2(store->db, "SELECT count(*) FROM todos WHERE done = 0", -1, &stmt,
                          NULL) != SQLITE_OK)
        return -1;
    if(sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}
