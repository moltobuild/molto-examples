#ifndef TODO_STORE_H
#define TODO_STORE_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The to-do list, kept in an SQLite database.
 *
 * Everything the window does to a task goes through here, and nothing here
 * knows there is a window: the tests open the same store on a database of
 * their own. Every write is one statement, so a crash leaves the list as it was
 * before the last click or after it, never between.
 */

/* Longest title kept, in bytes. A longer one is refused rather than cut. */
#define TODO_TITLE_MAX 500

typedef struct todo_store todo_store;

typedef struct {
    long long id;
    char *title; /* owned by the list it came in */
    bool done;
} todo_item;

typedef struct {
    todo_item *items;
    size_t count;
} todo_list;

typedef enum {
    todo_filter_all,
    todo_filter_active, /* not done */
    todo_filter_done,
} todo_filter;

/* Open, or create, the database at `path` — ":memory:" for one that lives as
   long as the store. NULL, with a reason in `err`, when SQLite cannot. The
   directory the file goes in must exist. */
todo_store *todo_store_open(const char *path, char *err, size_t err_size);
void todo_store_close(todo_store *store);

/* Add a task at the end of the list. Leading and trailing spaces are dropped;
   a title with nothing else, or longer than TODO_TITLE_MAX, is refused.
   `id_out` may be NULL. */
bool todo_add(todo_store *store, const char *title, long long *id_out);

/* Change one task. False when SQLite fails, when no task has that id, or —
   for a rename — when the new title would be refused by `todo_add`. */
bool todo_set_done(todo_store *store, long long id, bool done);
bool todo_rename(todo_store *store, long long id, const char *title);
bool todo_remove(todo_store *store, long long id);

/* Remove every task that is done. The number removed, or -1 on failure. */
int todo_clear_done(todo_store *store);

/* The tasks `filter` keeps, oldest first. Free with todo_list_free. */
bool todo_list_get(todo_store *store, todo_filter filter, todo_list *out);
void todo_list_free(todo_list *list);

/* How many tasks are not done yet, or -1 on failure. */
long long todo_count_active(todo_store *store);

/* The last error SQLite reported, for a message. */
const char *todo_store_error(const todo_store *store);

#endif /* TODO_STORE_H */
