/**
 * @file controller.c
 * @brief MVC mimarisinin Controller katmanı
 * 
 * Kullanıcı girişlerini işleyen, Model ve View katmanları arasında iletişimi 
 * sağlayan Controller modülü. Komutların ve mesajların ayrıştırılması,
 * View'a çıktı gönderme ve özel komutların işlenmesi gibi görevleri yerine getirir.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>  // GTK fonksiyonları için gerekli
#include <glib.h>     // g_str_has_prefix için gerekli
#include "model.h"
#include "view.h"
#include <time.h>     // Tarih ve zaman fonksiyonları için gerekli

#define MAX_CMD_LEN 256
#define MAX_TABS 10 // Maksimum sekme sayısı

// İleri bildirimler (Forward Declarations)
static void show_help(int tab_index);
static void show_date(int tab_index);
static void show_whoami(int tab_index);
static void show_uptime(int tab_index);
static void show_joke(int tab_index);
static void handle_cd_command(int tab_index, const char *path);
static gboolean check_messages(gpointer user_data);

/**
 * @brief Kullanıcı girişlerini işleyen ana fonksiyon
 * 
 * View katmanından gelen girişleri işleyip uygun işlemleri gerçekleştirir:
 * - Özel komutları (clear, help, version vb.) tanır ve işler
 * - Mesajları (@msg ile başlayan) ayrıştırır ve gönderir
 * - Normal shell komutlarını model_execute_command() ile çalıştırır
 * 
 * @param tab_index İşlenecek sekme indeksi
 * @param input Kullanıcı girişi (komut veya mesaj)
 */
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
        view_append_output(tab_index, "[Mesaj gönderildi]\n");
    } else {
        model_execute_command(tab_index, input);
    }
}

/**
 * @brief Yardım komutunu işleyen yardımcı fonksiyon
 * 
 * Mevcut komutların listesini ve açıklamalarını görüntüler
 * 
 * @param tab_index Görüntülenecek sekme indeksi
 */
static void show_help(int tab_index) {
    view_append_output_colored(tab_index, "Desteklenen komutlar:\n", "lightblue");
    view_append_output_colored(tab_index,
        " - clear: ekranı temizler\n"
        " - help: yardım bilgisi\n"
        " - version: sürüm bilgisini gösterir\n"
        " - date: sistem tarihini gösterir\n"
        " - whoami: kullanıcı adınızı gösterir\n"
        " - uptime: sistem çalışma süresini gösterir\n"
        " - joke: rastgele bir şaka yapar\n"
        " - @msg <mesaj>: mesaj gönderir\n"
        , "lightblue");
}

/**
 * @brief Sistem tarih ve zamanını gösteren fonksiyon
 * 
 * @param tab_index Görüntülenecek sekme indeksi
 */
static void show_date(int tab_index) {
    time_t t = time(NULL);
    char *time_str = ctime(&t);
    view_append_output_colored(tab_index, time_str, "lightgreen");
}

/**
 * @brief Mevcut kullanıcı adını gösteren fonksiyon
 * 
 * @param tab_index Görüntülenecek sekme indeksi
 */
static void show_whoami(int tab_index) {
    const char *user = g_get_user_name();
    view_append_output_colored(tab_index, user, "lightblue");
    view_append_output_colored(tab_index, "\n", "lightblue");
}

/**
 * @brief Sistem çalışma süresini gösteren fonksiyon
 * 
 * /proc/uptime dosyasını okuyarak sistemin çalışma süresini hesaplar
 * ve saat:dakika:saniye formatında görüntüler
 * 
 * @param tab_index Görüntülenecek sekme indeksi
 */
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

/**
 * @brief Rastgele bir programlama şakası gösteren fonksiyon
 * 
 * @param tab_index Görüntülenecek sekme indeksi
 */
static void show_joke(int tab_index) {
    // Şaka koleksiyonu
    const char *jokes[] = {
        "Why do programmers hate nature?\nIt has too many bugs.\n",
        "Why do programmers always mix up Christmas and Halloween?\nBecause Oct 31 == Dec 25\n",
        "A SQL query walks into a bar, walks up to two tables and asks, 'Can I join you?'\n",
        "How many programmers does it take to change a light bulb?\nNone, that's a hardware problem.\n",
        "Why do Java programmers wear glasses?\nBecause they don't C#\n",
        "!false\nIt's funny because it's true.\n",
        "A programmer's wife tells him: 'Go to the store and buy a loaf of bread. If they have eggs, buy a dozen.'\nThe programmer returns with 12 loaves of bread.\n",
        "Why did the functions stop calling each other?\nBecause they had too many arguments.\n",
        "Why was the JavaScript developer sad?\nBecause he didn't Node how to Express himself.\n",
        "How do you tell an introverted programmer from an extroverted programmer?\nThe extroverted programmer looks at YOUR shoes when talking to you.\n"
    };
    
    // Rastgele şaka seç
    int joke_count = sizeof(jokes) / sizeof(jokes[0]);
    int random_index = rand() % joke_count;
    
    // Seçilen şakayı göster
    view_append_output_colored(tab_index, jokes[random_index], "magenta");
}

/**
 * @brief Dizin değiştirme (cd) komutunu işleyen fonksiyon
 * 
 * @param tab_index Görüntülenecek sekme indeksi
 * @param path Geçilecek dizin yolu
 */
static void handle_cd_command(int tab_index, const char *path) {
    if (chdir(path) == 0) {
        view_append_output_colored(tab_index, "Dizin değiştirildi\n", "lightgreen");
    } else {
        view_append_output_colored(tab_index, "Hedef dizine geçilemedi\n", "red");
    }
}

/**
 * @brief Komut çıktılarını görüntüleyen yardımcı fonksiyon
 * 
 * Model katmanından gelen çıktıları View katmanına iletir.
 * Çıktı renkli ise ona göre görüntüleme yapar.
 * 
 * @param tab_index Görüntülenecek sekme indeksi
 * @param output Görüntülenecek çıktı metni
 * @param color Çıktı rengi (NULL ise varsayılan renk kullanılır)
 */
static void handle_command_output(int tab_index, const char *output, const char *color) {
    if (output && *output) {
        if (color && *color)
            view_append_output_colored(tab_index, output, color);
        else
            view_append_output(tab_index, output);
    }
}

/**
 * @brief Paylaşılan bellek mesajlarını düzenli aralıklarla kontrol eden zamanlayıcı fonksiyonu
 * 
 * GTK zamanlayıcısı tarafından düzenli olarak çağrılır ve paylaşılan belleği kontrol eder.
 * Yeni mesaj varsa, tüm aktif sekmelerde görüntüler.
 * 
 * @param user_data Kullanıcı verisi (bu fonksiyon için kullanılmaz)
 * @return gboolean Zamanlayıcının devam etmesi için G_SOURCE_CONTINUE döndürür
 */
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

/**
 * @brief Controller modülünü başlatan ana fonksiyon
 * 
 * MVC mimarisini başlatır:
 * 1. Model katmanını başlatır
 * 2. Komut çıktıları için callback fonksiyonu ayarlar
 * 3. View katmanını başlatır
 * 4. Kullanıcı girişleri için callback fonksiyonu ayarlar
 * 5. Mesaj kontrolü için zamanlayıcı ekler
 * 6. View ana döngüsünü başlatır
 * 
 * @param argc Komut satırı argüman sayısı
 * @param argv Komut satırı argümanları
 */
void controller_start(int argc, char **argv) {
    model_init();  // Model katmanını başlat
    model_set_output_callback(handle_command_output);  // Çıktı callback'ini ayarla
    
    view_init(argc, argv);  // View katmanını başlat
    view_set_input_callback(on_user_input);  // Giriş callback'ini ayarla
    
    // Mesaj kontrolü için zamanlayıcı ekle
    g_timeout_add(500, check_messages, NULL);
    
    // View ana döngüsünü başlat (bloke eden çağrı)
    view_main_loop();
}