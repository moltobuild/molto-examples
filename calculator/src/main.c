#include <calculator/calc.h>

#include <gtk/gtk.h>

#include <stdio.h>
#include <string.h>

/*
 * The window: two lines of display and a grid of buttons, all of it driven by
 * calc.c. Every button and every key ends in the same `send`, which applies
 * one input to the calculator and redraws both lines.
 */

#define APP_ID "dev.moltobuild.calculator"

typedef struct {
    calc_state state;
    GtkWidget *expression; /* what was typed, small */
    GtkWidget *result;     /* the preview or the result, large */
} calculator;

/* What a button or a key asks for, beyond the characters calc_press takes. */
enum { KEY_EQUALS = '=', KEY_CLEAR = 'C', KEY_BACKSPACE = '\b' };

/* The large line: an error, the result of `=`, a preview of what has been
   typed so far, or the last result while nothing is typed. */
static void refresh(calculator *calc) {
    /* After `=`, the line above says what was evaluated rather than repeating
       the result that is now the expression. */
    char shown[(CALC_EXPR_MAX + 16) * 3 + 4];
    if(calc->state.evaluated) {
        calc_display(calc->state.evaluated_expr, shown, sizeof shown - 4);
        strcat(shown, " =");
    } else {
        calc_display(calc->state.expr, shown, sizeof shown);
    }
    gtk_label_set_text(GTK_LABEL(calc->expression),
                       calc->state.evaluated || calc->state.length > 0 ? shown : " ");

    gtk_widget_remove_css_class(calc->result, "error");
    if(calc->state.error != calc_ok) {
        gtk_label_set_text(GTK_LABEL(calc->result), calc_error_message(calc->state.error));
        gtk_widget_add_css_class(calc->result, "error");
        return;
    }

    char value[CALC_TEXT_MAX] = "";
    if(calc->state.evaluated || calc->state.length == 0)
        snprintf(value, sizeof value, "%s", calc->state.result);
    else if(!calc_preview(&calc->state, value, sizeof value))
        value[0] = '\0';

    char display[CALC_TEXT_MAX * 3];
    calc_display(value, display, sizeof display);
    gtk_label_set_text(GTK_LABEL(calc->result), display[0] != '\0' ? display : " ");
}

static void send(calculator *calc, char key) {
    switch(key) {
    case KEY_EQUALS:
        (void)calc_equals(&calc->state);
        break;
    case KEY_CLEAR:
        calc_clear(&calc->state);
        break;
    case KEY_BACKSPACE:
        calc_backspace(&calc->state);
        break;
    default:
        if(!calc_press(&calc->state, key))
            gtk_widget_error_bell(calc->result);
        break;
    }
    refresh(calc);
}

/* --- buttons --- */

typedef struct {
    const char *label;
    char key;
    const char *style; /* a CSS class, or NULL */
    int column, row, width;
} button_spec;

static const button_spec BUTTONS[] = {
    {"C", KEY_CLEAR, "function", 0, 0, 1},
    {"(", '(', "function", 1, 0, 1},
    {")", ')', "function", 2, 0, 1},
    {"⌫", KEY_BACKSPACE, "function", 3, 0, 1},
    {"7", '7', NULL, 0, 1, 1},
    {"8", '8', NULL, 1, 1, 1},
    {"9", '9', NULL, 2, 1, 1},
    {"÷", '/', "operator", 3, 1, 1},
    {"4", '4', NULL, 0, 2, 1},
    {"5", '5', NULL, 1, 2, 1},
    {"6", '6', NULL, 2, 2, 1},
    {"×", '*', "operator", 3, 2, 1},
    {"1", '1', NULL, 0, 3, 1},
    {"2", '2', NULL, 1, 3, 1},
    {"3", '3', NULL, 2, 3, 1},
    {"−", '-', "operator", 3, 3, 1},
    {"0", '0', NULL, 0, 4, 1},
    {".", '.', NULL, 1, 4, 1},
    {"%", '%', "function", 2, 4, 1},
    {"+", '+', "operator", 3, 4, 1},
    {"=", KEY_EQUALS, "equals", 0, 5, 4},
};

static void on_button(GtkButton *button, gpointer data) {
    const char key = (char)GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "key"));
    send(data, key);
}

static GtkWidget *button_grid(calculator *calc) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_widget_set_vexpand(grid, TRUE);

    for(size_t i = 0; i < G_N_ELEMENTS(BUTTONS); i++) {
        const button_spec *spec = &BUTTONS[i];
        GtkWidget *button = gtk_button_new_with_label(spec->label);
        /* Keys only: a button that took focus would swallow Enter. */
        gtk_widget_set_focusable(button, FALSE);
        gtk_widget_add_css_class(button, "key");
        if(spec->style != NULL)
            gtk_widget_add_css_class(button, spec->style);
        g_object_set_data(G_OBJECT(button), "key", GINT_TO_POINTER((int)spec->key));
        g_signal_connect(button, "clicked", G_CALLBACK(on_button), calc);
        gtk_grid_attach(GTK_GRID(grid), button, spec->column, spec->row, spec->width, 1);
    }
    return grid;
}

/* --- keyboard --- */

static gboolean on_key(GtkEventControllerKey *controller, guint keyval, guint keycode,
                       GdkModifierType modifiers, gpointer data) {
    (void)controller;
    (void)keycode;
    if(modifiers & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_META_MASK))
        return FALSE;

    switch(keyval) {
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
        send(data, KEY_EQUALS);
        return TRUE;
    case GDK_KEY_BackSpace:
        send(data, KEY_BACKSPACE);
        return TRUE;
    case GDK_KEY_Escape:
    case GDK_KEY_Delete:
        send(data, KEY_CLEAR);
        return TRUE;
    default:
        break;
    }

    const gunichar c = gdk_keyval_to_unicode(keyval);
    if(c == '=')
        send(data, KEY_EQUALS);
    else if(c == ',') /* a decimal comma, as many keypads type it */
        send(data, '.');
    else if(c == 0x00D7) /* × */
        send(data, '*');
    else if(c == 0x00F7) /* ÷ */
        send(data, '/');
    else if(c != 0 && c < 128 && strchr("0123456789.+-*/%()", (int)c) != NULL)
        send(data, (char)c);
    else
        return FALSE;
    return TRUE;
}

/* --- the window --- */

static const char *const STYLE =
    "window { background: #1e1e24; }"
    ".display { padding: 12px 8px 20px 8px; }"
    ".expression { font-size: 18px; color: #9a9aa8; }"
    /* One height for a number and for a message, so the keys never move. */
    ".result { font-size: 40px; font-weight: 600; color: #f4f4f6; min-height: 52px; }"
    ".result.error { font-size: 22px; color: #ff7b72; }"
    ".key { font-size: 20px; border-radius: 14px; min-height: 48px;"
    "       background: #2d2d36; color: #f4f4f6; border: none; box-shadow: none; }"
    ".key:hover { background: #393945; }"
    ".key:active { background: #4a4a58; }"
    ".function { background: #3a3a46; color: #c9c9d6; }"
    ".operator { background: #3d4a6b; color: #dbe4ff; }"
    ".equals { background: #4f7cff; color: #ffffff; font-weight: 700; }"
    ".equals:hover { background: #6a90ff; }";

static GtkWidget *display_line(const char *style) {
    GtkWidget *label = gtk_label_new(" ");
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_START);
    gtk_widget_add_css_class(label, style);
    return label;
}

static void on_activate(GtkApplication *app, gpointer data) {
    calculator *calc = data;

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css, STYLE);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Calculator");
    gtk_window_set_default_size(GTK_WINDOW(window), 340, 520);

    GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(layout, 16);
    gtk_widget_set_margin_end(layout, 16);
    gtk_widget_set_margin_top(layout, 16);
    gtk_widget_set_margin_bottom(layout, 16);

    GtkWidget *display = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(display, "display");
    calc->expression = display_line("expression");
    calc->result = display_line("result");
    gtk_box_append(GTK_BOX(display), calc->expression);
    gtk_box_append(GTK_BOX(display), calc->result);

    gtk_box_append(GTK_BOX(layout), display);
    gtk_box_append(GTK_BOX(layout), button_grid(calc));
    gtk_window_set_child(GTK_WINDOW(window), layout);

    GtkEventController *keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key), calc);
    gtk_widget_add_controller(window, keys);

    refresh(calc);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    calculator calc;
    calc_init(&calc.state);

    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), &calc);
    const int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
