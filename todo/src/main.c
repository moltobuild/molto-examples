#include <todo/store.h>

#include <gtk/gtk.h>

#include <stdio.h>

/*
 * The window: an entry to add a task, three filters, the list, and a footer
 * that counts what is left. Every change goes to the store first and the list
 * is redrawn from what the store then says, so the window never shows a task
 * the database does not have.
 */

#define APP_ID "dev.moltobuild.todo"
/* Where the list lives unless TODO_DB names another file. */
#define DATA_DIR "molto-todo"
#define DATA_FILE "todos.db"

typedef struct {
    todo_store *store;
    todo_filter filter;
    GtkWidget *entry;
    GtkWidget *list;
    GtkWidget *empty; /* shown instead of the list when it has nothing */
    GtkWidget *left;  /* "3 tasks left" */
    GtkWidget *clear; /* "Clear done" */
    guint redraw;     /* a pending redraw, or 0 */
} app_state;

static void redraw_now(app_state *app);

/* Redrawn on idle rather than at once: the change usually comes from a widget
   inside a row the redraw is about to destroy. */
static gboolean redraw_idle(gpointer data) {
    app_state *app = data;
    app->redraw = 0;
    redraw_now(app);
    return G_SOURCE_REMOVE;
}

static void redraw_soon(app_state *app) {
    if(app->redraw == 0)
        app->redraw = g_idle_add(redraw_idle, app);
}

static void report(app_state *app, const char *what) {
    fprintf(stderr, "todo: could not %s: %s\n", what, todo_store_error(app->store));
    gtk_widget_error_bell(app->list);
}

/* --- one row --- */

/* A task's id, carried on the widgets that change it. */
static void set_id(GtkWidget *widget, long long id) {
    gint64 *copy = g_new(gint64, 1);
    *copy = id;
    g_object_set_data_full(G_OBJECT(widget), "task-id", copy, g_free);
}

static long long get_id(gpointer widget) {
    const gint64 *id = g_object_get_data(G_OBJECT(widget), "task-id");
    return id != NULL ? *id : -1;
}

static void on_toggled(GtkCheckButton *check, gpointer data) {
    app_state *app = data;
    if(!todo_set_done(app->store, get_id(check), gtk_check_button_get_active(check)))
        report(app, "update the task");
    redraw_soon(app);
}

static void on_remove(GtkButton *button, gpointer data) {
    app_state *app = data;
    if(!todo_remove(app->store, get_id(button)))
        report(app, "remove the task");
    redraw_soon(app);
}

/* An edit is saved when the label stops editing. A title the store refuses —
   an empty one — puts the old one back, by redrawing from the store. */
static void on_editing(GObject *label, GParamSpec *spec, gpointer data) {
    (void)spec;
    app_state *app = data;
    if(gtk_editable_label_get_editing(GTK_EDITABLE_LABEL(label)))
        return;
    const char *before = g_object_get_data(label, "task-title");
    const char *after = gtk_editable_get_text(GTK_EDITABLE(label));
    if(before != NULL && g_strcmp0(before, after) != 0 &&
       !todo_rename(app->store, get_id(label), after))
        gtk_widget_error_bell(GTK_WIDGET(label));
    redraw_soon(app);
}

static GtkWidget *task_row(app_state *app, const todo_item *item) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(row, "task");

    GtkWidget *check = gtk_check_button_new();
    gtk_check_button_set_active(GTK_CHECK_BUTTON(check), item->done);
    set_id(check, item->id);
    g_signal_connect(check, "toggled", G_CALLBACK(on_toggled), app);

    GtkWidget *title = gtk_editable_label_new(item->title);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_widget_set_tooltip_text(title, "Double-click to edit");
    if(item->done)
        gtk_widget_add_css_class(title, "done");
    set_id(title, item->id);
    g_object_set_data_full(G_OBJECT(title), "task-title", g_strdup(item->title), g_free);
    g_signal_connect(title, "notify::editing", G_CALLBACK(on_editing), app);

    GtkWidget *remove = gtk_button_new_with_label("✕");
    gtk_widget_add_css_class(remove, "remove");
    gtk_widget_set_tooltip_text(remove, "Delete");
    set_id(remove, item->id);
    g_signal_connect(remove, "clicked", G_CALLBACK(on_remove), app);

    gtk_box_append(GTK_BOX(row), check);
    gtk_box_append(GTK_BOX(row), title);
    gtk_box_append(GTK_BOX(row), remove);
    return row;
}

/* --- the whole list --- */

static void redraw_now(app_state *app) {
    gtk_list_box_remove_all(GTK_LIST_BOX(app->list));

    todo_list tasks;
    if(!todo_list_get(app->store, app->filter, &tasks)) {
        report(app, "read the list");
        return;
    }
    for(size_t i = 0; i < tasks.count; i++)
        gtk_list_box_append(GTK_LIST_BOX(app->list), task_row(app, &tasks.items[i]));
    /* Not the list box's own placeholder: remove_all takes that away too. */
    static const char *const EMPTY[] = {
        [todo_filter_all] = "Nothing here yet",
        [todo_filter_active] = "Nothing left to do",
        [todo_filter_done] = "Nothing done yet",
    };
    gtk_label_set_text(GTK_LABEL(app->empty), EMPTY[app->filter]);
    gtk_widget_set_visible(app->empty, tasks.count == 0);
    gtk_widget_set_visible(app->list, tasks.count > 0);
    todo_list_free(&tasks);

    const long long left = todo_count_active(app->store);
    char text[64];
    snprintf(text, sizeof text, "%lld %s left", left < 0 ? 0 : left, left == 1 ? "task" : "tasks");
    gtk_label_set_text(GTK_LABEL(app->left), text);

    todo_list done;
    const bool any_done = todo_list_get(app->store, todo_filter_done, &done) && done.count > 0;
    todo_list_free(&done);
    gtk_widget_set_sensitive(app->clear, any_done);
}

/* --- adding, filtering, clearing --- */

static void on_add(GtkWidget *widget, gpointer data) {
    (void)widget;
    app_state *app = data;
    const char *title = gtk_editable_get_text(GTK_EDITABLE(app->entry));
    if(!todo_add(app->store, title, NULL)) {
        gtk_widget_error_bell(app->entry);
        return;
    }
    gtk_editable_set_text(GTK_EDITABLE(app->entry), "");
    redraw_now(app);
}

static void on_filter(GtkToggleButton *button, gpointer data) {
    app_state *app = data;
    if(!gtk_toggle_button_get_active(button))
        return;
    app->filter = (todo_filter)GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "filter"));
    redraw_now(app);
}

static void on_clear(GtkButton *button, gpointer data) {
    (void)button;
    app_state *app = data;
    if(todo_clear_done(app->store) < 0)
        report(app, "clear the done tasks");
    redraw_now(app);
}

static GtkWidget *filter_bar(app_state *app) {
    static const struct {
        const char *label;
        todo_filter filter;
    } FILTERS[] = {
        {"All", todo_filter_all},
        {"Active", todo_filter_active},
        {"Done", todo_filter_done},
    };
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(bar, "linked");
    gtk_widget_set_halign(bar, GTK_ALIGN_START);
    GtkWidget *first = NULL;
    GtkWidget *chosen = NULL;
    for(size_t i = 0; i < G_N_ELEMENTS(FILTERS); i++) {
        GtkWidget *button = gtk_toggle_button_new_with_label(FILTERS[i].label);
        gtk_widget_add_css_class(button, "filter");
        g_object_set_data(G_OBJECT(button), "filter", GINT_TO_POINTER((int)FILTERS[i].filter));
        if(first == NULL)
            first = button;
        else
            gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(button), GTK_TOGGLE_BUTTON(first));
        gtk_box_append(GTK_BOX(bar), button);
        if(FILTERS[i].filter == app->filter)
            chosen = button;
    }
    /* The filter the state starts with, not always the first — chosen before
       the handlers are connected, since the rest of the window does not exist
       yet for a redraw to fill. */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chosen != NULL ? chosen : first), TRUE);
    for(GtkWidget *child = gtk_widget_get_first_child(bar); child != NULL;
        child = gtk_widget_get_next_sibling(child))
        g_signal_connect(child, "toggled", G_CALLBACK(on_filter), app);
    return bar;
}

/* --- the window --- */

static const char *const STYLE =
    "window { background: #1e1e24; color: #f4f4f6; }"
    ".heading { font-size: 28px; font-weight: 700; }"
    "entry { min-height: 40px; border-radius: 10px; background: #2d2d36; color: #f4f4f6;"
    "        border: 1px solid #3a3a46; padding: 0 12px; }"
    ".add { border-radius: 10px; background: #4f7cff; color: #ffffff; font-weight: 600;"
    "       border: none; padding: 0 18px; }"
    ".add:hover { background: #6a90ff; }"
    ".filter { background: #2d2d36; color: #c9c9d6; border: 1px solid #3a3a46; padding: 4px 14px; }"
    ".filter:checked { background: #3d4a6b; color: #dbe4ff; }"
    "list { background: transparent; }"
    "list row { background: transparent; padding: 0; }"
    ".task { background: #2d2d36; border-radius: 10px; padding: 10px 12px; margin-bottom: 6px; }"
    /* Text decoration is not inherited, so it goes on the label inside too. */
    ".done, .done label { color: #7d7d8a; text-decoration-line: line-through; }"
    ".remove { background: transparent; color: #8a8a98; border: none; box-shadow: none;"
    "          font-size: 15px;"
    "          min-width: 28px; min-height: 28px; border-radius: 8px; }"
    ".remove:hover { background: #4a2f35; color: #ff7b72; }"
    ".empty { color: #7d7d8a; padding: 24px; }"
    ".footer { color: #9a9aa8; }"
    ".clear { background: transparent; color: #9a9aa8; border: none; box-shadow: none; }"
    ".clear:hover { color: #f4f4f6; }"
    ".clear:disabled { color: #4f4f5a; }";

static GtkWidget *entry_row(app_state *app) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app->entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "What needs doing?");
    gtk_entry_set_max_length(GTK_ENTRY(app->entry), TODO_TITLE_MAX);
    gtk_widget_set_hexpand(app->entry, TRUE);
    g_signal_connect(app->entry, "activate", G_CALLBACK(on_add), app);

    GtkWidget *add = gtk_button_new_with_label("Add");
    gtk_widget_add_css_class(add, "add");
    g_signal_connect(add, "clicked", G_CALLBACK(on_add), app);

    gtk_box_append(GTK_BOX(row), app->entry);
    gtk_box_append(GTK_BOX(row), add);
    return row;
}

static GtkWidget *footer(app_state *app) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(row, "footer");
    app->left = gtk_label_new("");
    gtk_widget_set_hexpand(app->left, TRUE);
    gtk_label_set_xalign(GTK_LABEL(app->left), 0.0f);
    app->clear = gtk_button_new_with_label("Clear done");
    gtk_widget_add_css_class(app->clear, "clear");
    g_signal_connect(app->clear, "clicked", G_CALLBACK(on_clear), app);
    gtk_box_append(GTK_BOX(row), app->left);
    gtk_box_append(GTK_BOX(row), app->clear);
    return row;
}

static void on_activate(GtkApplication *gtk_app, gpointer data) {
    app_state *app = data;

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css, STYLE);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    GtkWidget *window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(window), "To-do");
    gtk_window_set_default_size(GTK_WINDOW(window), 440, 600);

    GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_start(layout, 20);
    gtk_widget_set_margin_end(layout, 20);
    gtk_widget_set_margin_top(layout, 20);
    gtk_widget_set_margin_bottom(layout, 16);

    GtkWidget *heading = gtk_label_new("To-do");
    gtk_widget_add_css_class(heading, "heading");
    gtk_label_set_xalign(GTK_LABEL(heading), 0.0f);

    app->list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(app->list), GTK_SELECTION_NONE);
    app->empty = gtk_label_new("");
    gtk_widget_add_css_class(app->empty, "empty");
    gtk_widget_set_vexpand(app->empty, TRUE);
    gtk_widget_set_valign(app->empty, GTK_ALIGN_START);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(content), app->list);
    gtk_box_append(GTK_BOX(content), app->empty);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), content);
    gtk_widget_set_vexpand(scroll, TRUE);

    gtk_box_append(GTK_BOX(layout), heading);
    gtk_box_append(GTK_BOX(layout), entry_row(app));
    gtk_box_append(GTK_BOX(layout), filter_bar(app));
    gtk_box_append(GTK_BOX(layout), scroll);
    gtk_box_append(GTK_BOX(layout), footer(app));
    gtk_window_set_child(GTK_WINDOW(window), layout);

    redraw_now(app);
    gtk_window_present(GTK_WINDOW(window));
    gtk_widget_grab_focus(app->entry);
}

/* TODO_DB, or todos.db in the user's data directory, which is created. */
static char *database_path(void) {
    const char *chosen = g_getenv("TODO_DB");
    if(chosen != NULL && chosen[0] != '\0')
        return g_strdup(chosen);
    char *dir = g_build_filename(g_get_user_data_dir(), DATA_DIR, NULL);
    (void)g_mkdir_with_parents(dir, 0700);
    char *path = g_build_filename(dir, DATA_FILE, NULL);
    g_free(dir);
    return path;
}

int main(int argc, char **argv) {
    char *path = database_path();
    char err[512] = "";
    app_state app = {.filter = todo_filter_all};
    app.store = todo_store_open(path, err, sizeof err);
    g_free(path);
    if(app.store == NULL) {
        fprintf(stderr, "todo: %s\n", err);
        return 1;
    }

    GtkApplication *gtk_app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(on_activate), &app);
    const int status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    g_object_unref(gtk_app);
    if(app.redraw != 0)
        g_source_remove(app.redraw);
    todo_store_close(app.store);
    return status;
}
