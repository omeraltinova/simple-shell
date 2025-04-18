/**
 * @file view.c
 * @brief MVC mimarisinin View katmanı
 * 
 * GTK 4 tabanlı kullanıcı arayüzünü oluşturan ve yöneten modül.
 * Terminal sekmelerini, giriş çıkış alanlarını ve kullanıcı etkileşimlerini yönetir.
 */

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "model.h"

#define MAX_TABS 100  // Maksimum sekme sayısı

// GTK widget ve uygulama değişkenleri
static GtkNotebook *notebook;                 // Sekme konteynerı
static GtkApplication *app;                   // GTK uygulaması
static GtkWidget *tab_outputs[MAX_TABS];      // Terminal çıktı alanları
static GtkWidget *tab_inputs[MAX_TABS];       // Terminal giriş alanları
static GtkWidget *tab_scrolls[MAX_TABS];      // Kaydırma panelleri
static int tab_count = 0;                     // Açık sekme sayısı
static int next_index = 0;                    // Bir sonraki sekme indeksi
static int history_index[MAX_TABS] = {0};     // Her sekme için geçmiş indeksi

// Callback fonksiyonları
static void (*input_callback)(int tab_index, const char *input) = NULL;
static void (*message_received_callback)(const char *msg);

/**
 * @brief CSS stillerini uygulayan fonksiyon
 * 
 * style.css dosyasını okuyarak GTK arayüzüne özel görünüm uygular
 */
void apply_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default(); 
    gtk_css_provider_load_from_path(provider, "style.css"); 

    gtk_style_context_add_provider_for_display(           
        display,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

/**
 * @brief Mesaj alma callback'ini ayarlayan fonksiyon
 * 
 * @param callback Mesajları işleyecek fonksiyon
 */
void view_set_message_callback(void (*callback)(const char *msg)) {
    message_received_callback = callback;
}

/**
 * @brief Terminal çıktı alanını en alta kaydıran fonksiyon
 * 
 * @param tab_index Kaydırılacak sekmenin indeksi
 */
static void scroll_to_bottom(int tab_index) {
    GtkTextView *text_view = GTK_TEXT_VIEW(tab_outputs[tab_index]);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_view_scroll_to_iter(text_view, &iter, 0.0, TRUE, 0.0, 1.0);
}

/**
 * @brief Terminal çıktısını renkli metin olarak ekleyen fonksiyon
 * 
 * @param buffer Metin ekleme hedefi (GtkTextBuffer)
 * @param text Eklenecek metin
 * @param color Metnin rengi
 */
static void insert_colored_text(GtkTextBuffer *buffer, const char *text, const char *color) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    GtkTextTag *tag = gtk_text_buffer_create_tag(buffer, NULL, "foreground", color, NULL);
    gtk_text_buffer_insert_with_tags(buffer, &end, text, -1, tag, NULL);
}

/**
 * @brief Belirtilen sekmeye renkli metin ekleyen fonksiyon
 * 
 * @param tab_index Hedef sekme indeksi
 * @param text Eklenecek metin
 * @param color Metnin rengi
 */
void view_append_output_colored(int tab_index, const char *text, const char *color) {
    if (tab_index < 0 || tab_index >= MAX_TABS || !tab_outputs[tab_index] || !text) return;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tab_outputs[tab_index]));
    insert_colored_text(buffer, text, color);
    scroll_to_bottom(tab_index);
}

/**
 * @brief Terminal çıktı alanını temizleyen fonksiyon
 * 
 * @param tab_index Temizlenecek sekme indeksi
 */
void view_clear_terminal(int tab_index) {
    if (tab_index < 0 || tab_index >= MAX_TABS || !tab_outputs[tab_index]) return;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tab_outputs[tab_index]));
    gtk_text_buffer_set_text(buffer, "", -1);
    scroll_to_bottom(tab_index);
}

/**
 * @brief Giriş alanında klavye olaylarını işleyen fonksiyon
 * 
 * Yukarı/aşağı ok tuşları ile komut geçmişine erişimi sağlar
 * 
 * @param controller Klavye olay denetleyicisi
 * @param keyval Basılan tuşun değeri
 * @param keycode Tuş kodu
 * @param state Tuş durumu (Shift, Ctrl, vb.)
 * @param user_data Kullanıcı verisi (sekme indeksi)
 */
static void on_entry_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    int tab_index = GPOINTER_TO_INT(user_data);
    GtkWidget *entry = tab_inputs[tab_index];
    int count = model_get_history_count();

    if (keyval == GDK_KEY_Up) {
        // Yukarı tuşu - önceki komut
        if (count == 0) return;
        if (history_index[tab_index] > 0)
            history_index[tab_index]--;
        const char *cmd = model_get_history(history_index[tab_index]);
        if (cmd)
            gtk_editable_set_text(GTK_EDITABLE(entry), cmd);
        gtk_widget_grab_focus(entry);
    } else if (keyval == GDK_KEY_Down) {
        // Aşağı tuşu - sonraki komut
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

/**
 * @brief Giriş alanından komut alındığında çağrılan fonksiyon
 * 
 * Kullanıcının girdiği komutu alır, ekrana gösterir ve controller'a iletir
 * 
 * @param widget Giriş alanı widget'ı
 * @param user_data Kullanıcı verisi (sekme indeksi)
 */
static void on_input_activated(GtkWidget *widget, gpointer user_data) {
    int tab_index = GPOINTER_TO_INT(user_data);
    const char *text = gtk_editable_get_text(GTK_EDITABLE(tab_inputs[tab_index]));
    
    if (input_callback && text && *text) {
        // Kullanıcı girdisini göster
        gchar *prefix = g_strdup(">command input: ");
        view_append_output_colored(tab_index, prefix, "orange");
        view_append_output_colored(tab_index, text, "gold");
        view_append_output_colored(tab_index, "\n", "gold");
        g_free(prefix);
        
        // Controller'a gönder
        input_callback(tab_index, text);
    }
    
    gtk_editable_set_text(GTK_EDITABLE(tab_inputs[tab_index]), "");
}

/**
 * @brief Kullanıcı girişlerini alacak callback'i ayarlayan fonksiyon
 * 
 * @param callback Kullanıcı girişini işleyecek fonksiyon
 */
void view_set_input_callback(void (*callback)(int tab_index, const char *input)) {
    input_callback = callback;
}

/**
 * @brief Sekme kapatma olayını işleyen fonksiyon
 * 
 * @param child Kapatılacak sekmenin içerik widget'ı
 */
static void close_tab(GtkWidget *child) {
    int page = gtk_notebook_page_num(notebook, child);
    if (page != -1) {
        gtk_notebook_remove_page(notebook, page);
        tab_outputs[page] = NULL;
        tab_inputs[page] = NULL;
        tab_scrolls[page] = NULL;

        // Eğer hiç sekme kalmadıysa hoş geldiniz ekranını göster
        if (gtk_notebook_get_n_pages(notebook) == 0) {
            GtkWidget *welcome = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
            GtkWidget *label1 = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(label1), "<span font='32' weight='bold'>Welcome</span>");
            GtkWidget *label2 = gtk_label_new("Yeni bir terminal sekmesi açmak için sağ üstteki + butonunu kullanın.");
            gtk_widget_set_halign(label1, GTK_ALIGN_CENTER);
            gtk_widget_set_halign(label2, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(welcome), label1);
            gtk_box_append(GTK_BOX(welcome), label2);
            gtk_notebook_append_page(notebook, welcome, NULL);
        }
    }
}

/**
 * @brief Yeni terminal sekmesi içeriğini oluşturan fonksiyon
 * 
 * @param index Sekme indeksi
 * @return GtkWidget* Oluşturulan sekme içerik widget'ı
 */
static GtkWidget* create_terminal_tab(int index) {
    // Ana konteyner
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // Terminal çıktı alanı
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    
    // Kaydırma paneli
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), text_view);

    // Giriş satırı konteynerı
    GtkWidget *input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    // Giriş alanı
    GtkWidget *entry = gtk_entry_new();
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_widget_set_margin_top(entry, 6);
    gtk_widget_set_margin_bottom(entry, 6);
    gtk_widget_set_margin_start(entry, 10);

    // Klavye olay işleyicisi ekle
    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_entry_key_press), GINT_TO_POINTER(index));
    gtk_widget_add_controller(entry, key_ctrl);

    // Gönder butonu
    GtkWidget *send_button = gtk_button_new_with_label("Gönder");
    gtk_button_set_has_frame(GTK_BUTTON(send_button), FALSE);
    gtk_widget_set_margin_top(send_button, 6);
    gtk_widget_set_margin_bottom(send_button, 6);
    gtk_widget_set_margin_end(send_button, 10);

    g_signal_connect(entry, "activate", G_CALLBACK(on_input_activated), GINT_TO_POINTER(index));
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_input_activated), GINT_TO_POINTER(index));

    gtk_box_append(GTK_BOX(input_row), entry);
    gtk_box_append(GTK_BOX(input_row), send_button);

    // Kaydırma butonu
    GtkWidget *scroll_button = gtk_button_new_with_label("↓");
    gtk_button_set_has_frame(GTK_BUTTON(scroll_button), FALSE); // dikkat!
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

/**
 * @brief Yeni bir terminal sekmesi oluşturan fonksiyon
 */
void view_create_tab() {
    // Eğer "Welcome" ekranı varsa kaldır
    if (gtk_notebook_get_n_pages(notebook) == 1 && !gtk_notebook_get_tab_label(notebook, gtk_notebook_get_nth_page(notebook, 0))) {
        gtk_notebook_remove_page(notebook, 0);
    }
    int index = next_index++;
    if (index >= MAX_TABS) return;

    GtkWidget *tab_content = create_terminal_tab(index);

    gchar *label_text = g_strdup_printf("Terminal %d", index + 1);
    GtkWidget *label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget *label = gtk_label_new(label_text);
    g_free(label_text);

    GtkWidget *close_button = gtk_button_new_with_label("X");
    gtk_widget_set_size_request(close_button, 20, 20);
    gtk_button_set_has_frame(GTK_BUTTON(close_button), FALSE);
    gtk_widget_set_valign(close_button, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(close_button, GTK_ALIGN_CENTER);
    g_signal_connect_swapped(close_button, "clicked", G_CALLBACK(close_tab), tab_content);

    gtk_box_append(GTK_BOX(label_box), label);
    gtk_box_append(GTK_BOX(label_box), close_button);

    gtk_notebook_append_page(notebook, tab_content, label_box);
    gtk_notebook_set_tab_reorderable(notebook, tab_content, TRUE);
    gtk_notebook_set_current_page(notebook, index);

    tab_count++;
}

/**
 * @brief Terminal çıktısını ekleyen fonksiyon
 * 
 * @param tab_index Hedef sekme indeksi
 * @param text Eklenecek metin
 */
void view_append_output(int tab_index, const char *text) {
    if (g_str_has_prefix(text, "[Tab ")) {
        view_append_output_colored(tab_index, text, "deepskyblue");
    } else if (strstr(text, "[Çıktı limiti") != NULL) {
        view_append_output_colored(tab_index, text, "red");
    } else {
        view_append_output_colored(tab_index, text, "white");
    }
}

/**
 * @brief Belirtilen sekmenin çıktı widget'ını döndüren fonksiyon
 * 
 * @param tab_index Sekme indeksi
 * @return GtkWidget* Çıktı widget'ı
 */
GtkWidget* view_get_output_widget(int tab_index) {
    if (tab_index >= 0 && tab_index < MAX_TABS)
        return tab_outputs[tab_index];
    return NULL;
}

/**
 * @brief Mesajları kontrol eden ve işleyen fonksiyon
 * 
 * @param user_data Kullanıcı verisi
 * @return gboolean Devam durumu
 */
static gboolean poll_messages(gpointer user_data) {
    static char last_msg[256] = "";
    
    const char *msg = model_peek_message();
    
    if (msg && msg[0] != '\0' && strcmp(msg, last_msg) != 0) {
        strncpy(last_msg, msg, sizeof(last_msg) - 1);
        last_msg[sizeof(last_msg) - 1] = '\0';  // Güvenli null-terminasyon
        
        // Tüm aktif sekmelere mesajı gönder
        for (int i = 0; i < MAX_TABS; i++) {
            if (tab_outputs[i] && GTK_IS_TEXT_VIEW(tab_outputs[i])) {
                view_append_output_colored(i, msg, "deepskyblue");
                view_append_output(i, "\n");
            }
        }
        
        model_clear_message(); // Mesajı temizle
    }
    return G_SOURCE_CONTINUE;
}

/**
 * @brief GTK uygulamasını başlatan fonksiyon
 * 
 * @param app_local GTK uygulaması
 * @param user_data Kullanıcı verisi
 */
static void activate(GtkApplication *app_local, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app_local);
    apply_css();
    gtk_window_set_title(GTK_WINDOW(window), "Modüler Terminal");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 500);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_box_append(GTK_BOX(vbox), GTK_WIDGET(notebook));

    GtkWidget *btn_new_tab = gtk_button_new_with_label("+");
    gtk_button_set_has_frame(GTK_BUTTON(btn_new_tab), FALSE);
    gtk_notebook_set_action_widget(notebook, btn_new_tab, GTK_PACK_END);
    g_signal_connect_swapped(btn_new_tab, "clicked", G_CALLBACK(view_create_tab), NULL);

    view_create_tab();
    g_timeout_add(500, poll_messages, NULL);

    gtk_window_present(GTK_WINDOW(window));
}

/**
 * @brief GTK uygulamasını başlatan ve ayarlayan fonksiyon
 * 
 * @param argc Argüman sayısı
 * @param argv Argüman dizisi
 */
void view_init(int argc, char **argv) {
    app = gtk_application_new("com.modular.shell", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
}

/**
 * @brief GTK ana döngüsünü başlatan fonksiyon
 */
void view_main_loop() {
    g_application_run(G_APPLICATION(app), 0, NULL);
}
