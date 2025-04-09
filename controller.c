// controller.c - Kullanıcı girişlerini işleyen katman

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>  // GTK fonksiyonları için gerekli
#include <glib.h>     // g_str_has_prefix için gerekli
#include "model.h"
#include "view.h"
#include <time.h>     // Tarih ve zaman fonksiyonları için gerekli

#define MAX_CMD_LEN 256
#define MAX_TABS 10 // Sabit bir MAX_TABS değeri tanımlandı

// İleri bildirimler (Forward Declarations)
static void show_help(int tab_index);
static void show_date(int tab_index);
static void show_whoami(int tab_index);
static void show_uptime(int tab_index);
static void show_joke(int tab_index);
static void handle_cd_command(int tab_index, const char *path);
static gboolean check_messages(gpointer user_data);

void apply_css(void);

// Bu fonksiyon view'den gelen inputu işleyip ilgili model fonksiyonuna yönlendirir
void on_user_input(int tab_index, const char *input) {
    // Komut geçmişini güncelle
    model_add_to_history(input);
    
    // Özel komutları işle
    if (strcmp(input, "clear") == 0) {
        view_clear_terminal(tab_index);
        return;
    }
    
    if (strcmp(input, "help") == 0) {
        show_help(tab_index);
        return;
    }
    
    if (strcmp(input, "version") == 0) {
        view_append_output_colored(tab_index, "Modüler Terminal v1.0\n", "lightgreen");
        return;
    }
    
    if (strcmp(input, "date") == 0) {
        show_date(tab_index);
        return;
    }
    
    if (strcmp(input, "whoami") == 0) {
        show_whoami(tab_index);
        return;
    }
    
    if (strcmp(input, "uptime") == 0) {
        show_uptime(tab_index);
        return;
    }
    
    if (strcmp(input, "joke") == 0) {
        show_joke(tab_index);
        return;
    }
    
    // cd komutu
    if (g_str_has_prefix(input, "cd ")) {
        handle_cd_command(tab_index, input + 3);
        return;
    }
    
    // Mesajlar ve normal komutlar
    if (strncmp(input, "@msg ", 5) == 0) {
        const char *msg = input + 5;
        model_send_message(tab_index, msg);
        view_append_output(tab_index, "[mesaj gönderildi]\n");
    } else {
        model_execute_command(tab_index, input);
    }
}

// Yardımcı fonksiyonlar
static void show_help(int tab_index) {
    view_append_output_colored(tab_index, "Desteklenen komutlar:\n", "lightblue");
    view_append_output_colored(tab_index,
        " - clear: ekranı temizler\n"
        " - help: yardım bilgisi\n"
        " - version: sürüm bilgisini gösterir\n"
        // diğer yardım metinleri...
        , "lightblue");
}

static void show_date(int tab_index) {
    time_t t = time(NULL);
    char *time_str = ctime(&t);
    view_append_output_colored(tab_index, time_str, "lightgreen");
}

static void show_whoami(int tab_index) {
    const char *user = g_get_user_name();
    view_append_output_colored(tab_index, user, "lightblue");
    view_append_output_colored(tab_index, "\n", "lightblue");
}

static void show_uptime(int tab_index) {
    FILE *fp = fopen("/proc/uptime", "r");
    if (fp) {
        double up;
        fscanf(fp, "%lf", &up);
        fclose(fp);
        int hours = (int)(up / 3600);
        int minutes = ((int)up % 3600) / 60;
        int seconds = (int)up % 60;
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Sistem %d saat %d dakika %d saniyedir açık\n", hours, minutes, seconds);
        view_append_output_colored(tab_index, buffer, "lightyellow");
    } else {
        view_append_output_colored(tab_index, "Uptime bilgisi alınamadı\n", "red");
    }
}

static void show_joke(int tab_index) {
    view_append_output_colored(tab_index, "Why do programmers hate nature?\nIt has too many bugs.\n", "magenta");
}

static void handle_cd_command(int tab_index, const char *path) {
    if (chdir(path) == 0) {
        view_append_output_colored(tab_index, "Dizin değiştirildi\n", "lightgreen");
    } else {
        view_append_output_colored(tab_index, "Hedef dizine geçilemedi\n", "red");
    }
}

static void handle_command_output(int tab_index, const char *output, const char *color) {
    if (output && *output) {
        if (color && *color)
            view_append_output_colored(tab_index, output, color);
        else
            view_append_output(tab_index, output);
    }
}

static gboolean check_messages(gpointer user_data) {
    const char *msg = model_peek_message();
    if (msg && msg[0] != '\0') {
        for (int i = 0; i < MAX_TABS; i++) {
            // Var olan sekmelere mesajı gönder
            if (i < MAX_TABS && GTK_IS_TEXT_VIEW(view_get_output_widget(i))) {
                view_append_output_colored(i, msg, "deepskyblue");
                view_append_output(i, "\n");
            }
        }
        model_clear_message();
    }
    return G_SOURCE_CONTINUE;
}

void controller_start(int argc, char **argv) {
    model_init();
    model_set_output_callback(handle_command_output);
    
    view_init(argc, argv);
    // apply_css();
    view_set_input_callback(on_user_input);
    
    // Mesaj kontrolü için timer ekle
    g_timeout_add(500, check_messages, NULL);
    
    view_main_loop();
}