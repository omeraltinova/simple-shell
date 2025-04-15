/**
 * @file main.c
 * @brief Programın ana giriş noktası
 * 
 * Bu dosya, terminal uygulamasının başlangıç noktasını içerir.
 * MVC (Model-View-Controller) mimarisini başlatır ve uygulama akışını yönetir.
 */

#include "controller.h"
#include "model.h"

/**
 * @brief Programın ana fonksiyonu
 * 
 * @param argc Komut satırı argüman sayısı
 * @param argv Komut satırı argümanları
 * @return int Program çıkış kodu (başarılı: 0)
 * 
 * Uygulama akışı:
 * 1. Paylaşılan belleği başlatır
 * 2. Controller'ı çalıştırır
 * 3. Program sonlandığında kaynakları temizler
 */
int main(int argc, char **argv) {
    model_init_shared_memory();  // Paylaşılan belleği başlat
    controller_start(argc, argv); // Controller'ı çalıştır (ana döngü)
    model_cleanup();             // Kaynakları temizle
    return 0;
}