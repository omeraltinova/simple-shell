#include <gtk/gtk.h>

int tab_counter = 1;

typedef struct {
    GtkWidget *label;
    GtkWidget *entry;
    GtkTextBuffer *buffer;
} InputContext;

typedef struct {
    GtkWidget *label;
    GtkWidget *entry;
    GtkWidget *tab_box;
} RenameContext;

static void send_input(GtkWidget *widget, gpointer user_data) {
    InputContext *ctx = (InputContext *)user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));
    if (text && *text) {
        gchar *formatted = g_strdup_printf("i\n%s\n", text);
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(ctx->buffer, &end);
        gtk_text_buffer_insert(ctx->buffer, &end, formatted, -1);
        gtk_editable_set_text(GTK_EDITABLE(ctx->entry), "");
        g_free(formatted);
    }
}

static GtkWidget* create_terminal_tab() {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *text_view = gtk_text_view_new();
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), text_view);

    GtkWidget *input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    GtkWidget *entry = gtk_entry_new();
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_widget_set_margin_top(entry, 6);
    gtk_widget_set_margin_bottom(entry, 6);
    gtk_widget_set_margin_start(entry, 10);

    GtkWidget *send_button = gtk_button_new_with_label("Gönder");
    gtk_widget_set_margin_top(send_button, 6);
    gtk_widget_set_margin_bottom(send_button, 6);
    gtk_widget_set_margin_end(send_button, 10);

    InputContext *ctx = g_malloc(sizeof(InputContext));
    ctx->entry = entry;
    ctx->buffer = buffer;

    g_signal_connect(entry, "activate", G_CALLBACK(send_input), ctx);
    g_signal_connect(send_button, "clicked", G_CALLBACK(send_input), ctx);

    gtk_box_append(GTK_BOX(input_row), entry);
    gtk_box_append(GTK_BOX(input_row), send_button);

    gtk_box_append(GTK_BOX(box), scroll);
    gtk_box_append(GTK_BOX(box), input_row);

    return box;
}

static void on_close_tab(GtkButton *button, gpointer user_data) {
    GtkWidget *tab_content = GTK_WIDGET(user_data);
    GtkWidget *parent = gtk_widget_get_parent(tab_content);
    while (parent && !GTK_IS_NOTEBOOK(parent))
        parent = gtk_widget_get_parent(parent);
    if (GTK_IS_NOTEBOOK(parent)) {
        GtkNotebook *notebook = GTK_NOTEBOOK(parent);
        int page = gtk_notebook_page_num(notebook, tab_content);
        if (page != -1)
            gtk_notebook_remove_page(notebook, page);
    }
}

static void on_tab_label_rename(GtkEditable *editable, gpointer user_data) {
    RenameContext *ctx = (RenameContext *)user_data;
    const char *new_text = gtk_editable_get_text(editable);
    GtkWidget *new_label = gtk_label_new(new_text);

    gtk_box_remove(GTK_BOX(ctx->tab_box), ctx->entry);
    gtk_box_insert_child_after(GTK_BOX(ctx->tab_box), new_label, NULL);

    g_object_set_data(G_OBJECT(ctx->tab_box), "tab-label", new_label);
    g_free(ctx);
}

static gboolean on_tab_label_button_press(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    if (n_press == 2) {
        GtkWidget *tab_box = GTK_WIDGET(user_data);
        GtkWidget *label = g_object_get_data(G_OBJECT(tab_box), "tab-label");

        const char *old_text = gtk_label_get_text(GTK_LABEL(label));
        GtkWidget *entry = gtk_entry_new();
        gtk_editable_set_text(GTK_EDITABLE(entry), old_text);
        gtk_widget_grab_focus(entry);

        gtk_box_remove(GTK_BOX(tab_box), label);
        gtk_box_insert_child_after(GTK_BOX(tab_box), entry, NULL);

        RenameContext *ctx = g_malloc(sizeof(RenameContext));
        ctx->entry = entry;
        ctx->label = label;
        ctx->tab_box = tab_box;

        g_signal_connect(entry, "activate", G_CALLBACK(on_tab_label_rename), ctx);
        g_signal_connect(entry, "focus-out-event", G_CALLBACK(on_tab_label_rename), ctx);
    }
    return TRUE;
}

static GtkWidget* create_tab_label(GtkNotebook *notebook, GtkWidget *tab_content) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    gchar *title = g_strdup_printf("Terminal %d", tab_counter++);
    GtkWidget *label = gtk_label_new(title);
    g_free(title);

    GtkWidget *btn = gtk_button_new_with_label("✕");
    gtk_widget_set_margin_start(btn, 5);
    gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
    gtk_button_set_has_frame(GTK_BUTTON(btn), FALSE);
    gtk_widget_set_focusable(btn, FALSE);
    gtk_widget_set_can_focus(btn, FALSE);

    g_object_set_data(G_OBJECT(box), "tab-label", label);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_close_tab), tab_content);

    GtkGesture *click = gtk_gesture_click_new();
    g_signal_connect(click, "pressed", G_CALLBACK(on_tab_label_button_press), box);
    gtk_widget_add_controller(label, GTK_EVENT_CONTROLLER(click));

    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), btn);

    return box;
}

static GtkWidget* create_tab(GtkNotebook *notebook) {
    GtkWidget *tab_content = create_terminal_tab();
    GtkWidget *tab_label = create_tab_label(notebook, tab_content);

    gtk_notebook_append_page(notebook, tab_content, tab_label);
    return tab_content;
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "GTK 4 Terminal UI");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 500);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_append(GTK_BOX(vbox), notebook);

    GtkWidget *btn_new_tab = gtk_button_new_with_label("+");
    gtk_notebook_set_action_widget(GTK_NOTEBOOK(notebook), btn_new_tab, GTK_PACK_END);
    g_signal_connect_swapped(btn_new_tab, "clicked", G_CALLBACK(create_tab), notebook);

    create_tab(GTK_NOTEBOOK(notebook));
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.example.GtkTerminal", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
