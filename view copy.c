// view.c - GTK 4 UI (modüler versiyon)

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include "model.h"

#define MAX_TABS 100

static GtkNotebook *notebook;
static GtkApplication *app;
static GtkWidget *tab_outputs[MAX_TABS];
static GtkWidget *tab_inputs[MAX_TABS];
static GtkWidget *tab_scrolls[MAX_TABS];
static int tab_count = 0;
static int history_index[MAX_TABS] = {0};

static void (*input_callback)(int tab_index, const char *input) = NULL;

static void scroll_to_bottom(int tab_index) {
    GtkTextView *text_view = GTK_TEXT_VIEW(tab_outputs[tab_index]);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_view_scroll_to_iter(text_view, &iter, 0.0, TRUE, 0.0, 1.0);
}

static void insert_colored_text(GtkTextBuffer *buffer, const char *text, const char *color) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    GtkTextTag *tag = gtk_text_buffer_create_tag(buffer, NULL, "foreground", color, NULL);
    gtk_text_buffer_insert_with_tags(buffer, &end, text, -1, tag, NULL);
}

static void view_append_output_colored(int tab_index, const char *text, const char *color) {
    if (tab_index < 0 || tab_index >= tab_count || !text) return;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tab_outputs[tab_index]));

    insert_colored_text(buffer, text, color);
    scroll_to_bottom(tab_index);
}

static void on_entry_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    int tab_index = GPOINTER_TO_INT(user_data);
    GtkWidget *entry = tab_inputs[tab_index];
    int count = model_get_history_count();

    if (keyval == GDK_KEY_Up) {
        if (count == 0) return;
        if (history_index[tab_index] > 0)
            history_index[tab_index]--;
        const char *cmd = model_get_history(history_index[tab_index]);
        if (cmd)
            gtk_editable_set_text(GTK_EDITABLE(entry), cmd);
        gtk_widget_grab_focus(entry);
    } else if (keyval == GDK_KEY_Down) {
        if (count == 0) return;
        if (history_index[tab_index] < count - 1)
            history_index[tab_index]++;
        const char *cmd = model_get_history(history_index[tab_index]);
        if (cmd)
            gtk_editable_set_text(GTK_EDITABLE(entry), cmd);
        else
            gtk_editable_set_text(GTK_EDITABLE(entry), "");
        gtk_widget_grab_focus(entry);
    }
}

static void on_input_activated(GtkWidget *widget, gpointer user_data) {
    int tab_index = GPOINTER_TO_INT(user_data);
    const char *text = gtk_editable_get_text(GTK_EDITABLE(tab_inputs[tab_index]));
    if (input_callback && text && *text) {
        gchar *prefix = g_strdup(">command input: ");
        view_append_output_colored(tab_index, prefix, "orange");
        view_append_output_colored(tab_index, text, "gold");
        view_append_output_colored(tab_index, "\n", "gold");
        g_free(prefix);
        input_callback(tab_index, text);
    }

    history_index[tab_index] = model_get_history_count();
    gtk_editable_set_text(GTK_EDITABLE(tab_inputs[tab_index]), "");
}

void view_set_input_callback(void (*callback)(int tab_index, const char *input)) {
    input_callback = callback;
}

static GtkWidget* create_terminal_tab(int index) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), text_view);

    GtkWidget *input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    GtkWidget *entry = gtk_entry_new();
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_widget_set_margin_top(entry, 6);
    gtk_widget_set_margin_bottom(entry, 6);
    gtk_widget_set_margin_start(entry, 10);

    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_entry_key_press), GINT_TO_POINTER(index));
    gtk_widget_add_controller(entry, key_ctrl);

    GtkWidget *send_button = gtk_button_new_with_label("Gönder");
    gtk_widget_set_margin_top(send_button, 6);
    gtk_widget_set_margin_bottom(send_button, 6);
    gtk_widget_set_margin_end(send_button, 10);

    g_signal_connect(entry, "activate", G_CALLBACK(on_input_activated), GINT_TO_POINTER(index));
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_input_activated), GINT_TO_POINTER(index));

    gtk_box_append(GTK_BOX(input_row), entry);
    gtk_box_append(GTK_BOX(input_row), send_button);

    GtkWidget *scroll_button = gtk_button_new_with_label("↓");
    gtk_widget_set_margin_top(scroll_button, 6);
    gtk_widget_set_margin_bottom(scroll_button, 6);
    gtk_widget_set_tooltip_text(scroll_button, "En alta git");
    g_signal_connect_swapped(scroll_button, "clicked", G_CALLBACK(scroll_to_bottom), GINT_TO_POINTER(index));
    gtk_box_append(GTK_BOX(input_row), scroll_button);

    gtk_box_append(GTK_BOX(box), scroll);
    gtk_box_append(GTK_BOX(box), input_row);

    tab_outputs[index] = text_view;
    tab_inputs[index] = entry;
    tab_scrolls[index] = scroll;
    history_index[index] = model_get_history_count();

    return box;
}

void view_create_tab() {
    if (tab_count >= MAX_TABS) return;

    GtkWidget *tab_content = create_terminal_tab(tab_count);

    gchar *label_text = g_strdup_printf("Terminal %d", tab_count + 1);
    GtkWidget *label = gtk_label_new(label_text);
    g_free(label_text);

    gtk_notebook_append_page(notebook, tab_content, label);
    gtk_notebook_set_current_page(notebook, tab_count);

    tab_count++;
}

void view_append_output(int tab_index, const char *text) {
    if (g_str_has_prefix(text, "[Tab ")) {
        view_append_output_colored(tab_index, text, "deepskyblue");
    } else if (strstr(text, "[Çıktı limiti") != NULL) {
        view_append_output_colored(tab_index, text, "red");
    } else {
        view_append_output_colored(tab_index, text, "white");
    }
}

static gboolean poll_messages(gpointer user_data) {
    static char last_msg[256] = "";
    extern char *shm_ptr;

    if (shm_ptr && shm_ptr[0] != '\0' && strcmp(shm_ptr, last_msg) != 0) {
        strncpy(last_msg, shm_ptr, sizeof(last_msg));
        for (int i = 0; i < tab_count; i++) {
            view_append_output(i, shm_ptr);
            view_append_output(i, "\n");
        }
        shm_ptr[0] = '\0';
    }
    return TRUE;
}

static void activate(GtkApplication *app_local, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app_local);
    gtk_window_set_title(GTK_WINDOW(window), "Modüler Terminal");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 500);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_box_append(GTK_BOX(vbox), GTK_WIDGET(notebook));

    GtkWidget *btn_new_tab = gtk_button_new_with_label("+");
    gtk_notebook_set_action_widget(notebook, btn_new_tab, GTK_PACK_END);
    g_signal_connect_swapped(btn_new_tab, "clicked", G_CALLBACK(view_create_tab), NULL);

    view_create_tab();
    g_timeout_add(500, poll_messages, NULL);

    gtk_window_present(GTK_WINDOW(window));
}

void view_init(int argc, char **argv) {
    app = gtk_application_new("com.modular.shell", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
}

void view_main_loop() {
    g_application_run(G_APPLICATION(app), 0, NULL);
}
