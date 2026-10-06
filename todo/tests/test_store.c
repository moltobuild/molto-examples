#include <moltest.h>

#include <todo/store.h>

#include <stdio.h>
#include <string.h>

/* The store against a real SQLite: in memory for most cases, on disk for the
   one that is about surviving a restart. */

static todo_store *fresh(void) {
    char err[256] = "";
    return todo_store_open(":memory:", err, sizeof err);
}

DESCRIBE(a_new_store_is_empty) {
    todo_store *store = fresh();
    ASSERT_NOT_NULL(store);
    todo_list list;
    ASSERT_TRUE(todo_list_get(store, todo_filter_all, &list));
    EXPECT_EQ(0u, list.count);
    EXPECT_EQ(0, todo_count_active(store));
    todo_list_free(&list);
    todo_store_close(store);
}

DESCRIBE(tasks_come_back_in_the_order_they_were_added) {
    todo_store *store = fresh();
    ASSERT_TRUE(todo_add(store, "buy milk", NULL));
    ASSERT_TRUE(todo_add(store, "write the README", NULL));
    todo_list list;
    ASSERT_TRUE(todo_list_get(store, todo_filter_all, &list));
    ASSERT_EQ(2u, list.count);
    EXPECT_STREQ("buy milk", list.items[0].title);
    EXPECT_STREQ("write the README", list.items[1].title);
    EXPECT_FALSE(list.items[0].done);
    todo_list_free(&list);
    todo_store_close(store);
}

DESCRIBE(a_title_is_trimmed_and_an_empty_one_refused) {
    todo_store *store = fresh();
    long long id = 0;
    ASSERT_TRUE(todo_add(store, "   call mom  ", &id));
    EXPECT_FALSE(todo_add(store, "    ", NULL));
    EXPECT_FALSE(todo_add(store, "", NULL));
    EXPECT_FALSE(todo_add(store, NULL, NULL));

    char long_title[TODO_TITLE_MAX + 2];
    memset(long_title, 'x', sizeof long_title - 1);
    long_title[sizeof long_title - 1] = '\0';
    EXPECT_FALSE(todo_add(store, long_title, NULL));

    todo_list list;
    ASSERT_TRUE(todo_list_get(store, todo_filter_all, &list));
    ASSERT_EQ(1u, list.count);
    EXPECT_STREQ("call mom", list.items[0].title);
    EXPECT_EQ(id, list.items[0].id);
    todo_list_free(&list);
    todo_store_close(store);
}

DESCRIBE(the_filters_split_active_from_done) {
    todo_store *store = fresh();
    long long first = 0;
    long long second = 0;
    ASSERT_TRUE(todo_add(store, "one", &first));
    ASSERT_TRUE(todo_add(store, "two", &second));
    ASSERT_TRUE(todo_set_done(store, first, true));

    todo_list active;
    todo_list done;
    ASSERT_TRUE(todo_list_get(store, todo_filter_active, &active));
    ASSERT_TRUE(todo_list_get(store, todo_filter_done, &done));
    ASSERT_EQ(1u, active.count);
    ASSERT_EQ(1u, done.count);
    EXPECT_STREQ("two", active.items[0].title);
    EXPECT_STREQ("one", done.items[0].title);
    EXPECT_TRUE(done.items[0].done);
    EXPECT_EQ(1, todo_count_active(store));

    todo_list_free(&active);
    todo_list_free(&done);
    todo_store_close(store);
}

DESCRIBE(a_task_can_be_renamed_undone_and_removed) {
    todo_store *store = fresh();
    long long id = 0;
    ASSERT_TRUE(todo_add(store, "draft", &id));
    EXPECT_TRUE(todo_rename(store, id, " final "));
    EXPECT_FALSE(todo_rename(store, id, "  "));
    EXPECT_TRUE(todo_set_done(store, id, true));
    EXPECT_TRUE(todo_set_done(store, id, false));

    todo_list list;
    ASSERT_TRUE(todo_list_get(store, todo_filter_all, &list));
    EXPECT_STREQ("final", list.items[0].title);
    EXPECT_FALSE(list.items[0].done);
    todo_list_free(&list);

    EXPECT_TRUE(todo_remove(store, id));
    EXPECT_EQ(0, todo_count_active(store));
    todo_store_close(store);
}

/* An id that names no task is a failure, so a stale row in the window cannot
   look like it changed something. */
DESCRIBE(changing_a_task_that_does_not_exist_fails) {
    todo_store *store = fresh();
    EXPECT_FALSE(todo_set_done(store, 42, true));
    EXPECT_FALSE(todo_rename(store, 42, "ghost"));
    EXPECT_FALSE(todo_remove(store, 42));
    todo_store_close(store);
}

DESCRIBE(clearing_the_done_tasks_keeps_the_rest) {
    todo_store *store = fresh();
    long long a = 0;
    long long b = 0;
    ASSERT_TRUE(todo_add(store, "a", &a));
    ASSERT_TRUE(todo_add(store, "b", &b));
    ASSERT_TRUE(todo_add(store, "c", NULL));
    ASSERT_TRUE(todo_set_done(store, a, true));
    ASSERT_TRUE(todo_set_done(store, b, true));

    EXPECT_EQ(2, todo_clear_done(store));
    EXPECT_EQ(0, todo_clear_done(store));
    todo_list list;
    ASSERT_TRUE(todo_list_get(store, todo_filter_all, &list));
    ASSERT_EQ(1u, list.count);
    EXPECT_STREQ("c", list.items[0].title);
    todo_list_free(&list);
    todo_store_close(store);
}

/* A title is bound, never pasted into the SQL. */
DESCRIBE(a_title_with_quotes_is_stored_as_written) {
    todo_store *store = fresh();
    const char *tricky = "'); DROP TABLE todos; --";
    ASSERT_TRUE(todo_add(store, tricky, NULL));
    todo_list list;
    ASSERT_TRUE(todo_list_get(store, todo_filter_all, &list));
    ASSERT_EQ(1u, list.count);
    EXPECT_STREQ(tricky, list.items[0].title);
    todo_list_free(&list);
    todo_store_close(store);
}

DESCRIBE(the_list_survives_closing_and_opening_again) {
    char dir[256];
    ASSERT_TRUE(moltest_temp_dir("todo_store", dir, sizeof dir));
    char path[512];
    snprintf(path, sizeof path, "%s/todos.db", dir);
    char err[256] = "";

    todo_store *store = todo_store_open(path, err, sizeof err);
    ASSERT_NOT_NULL(store);
    long long id = 0;
    ASSERT_TRUE(todo_add(store, "persist me", &id));
    ASSERT_TRUE(todo_set_done(store, id, true));
    todo_store_close(store);

    store = todo_store_open(path, err, sizeof err);
    ASSERT_NOT_NULL(store);
    todo_list list;
    ASSERT_TRUE(todo_list_get(store, todo_filter_all, &list));
    ASSERT_EQ(1u, list.count);
    EXPECT_STREQ("persist me", list.items[0].title);
    EXPECT_TRUE(list.items[0].done);
    todo_list_free(&list);
    todo_store_close(store);

    (void)remove(path);
    (void)remove(dir);
}
