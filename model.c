/**
 * @file model.c
 * @brief MVC mimarisinin Model katmanı
 * * Bu modül, veri ve iş mantığını yönetir:
 * - Komut çalıştırma ve process yönetimi (Doğrudan execvp ile)
 * - Paylaşılan bellek üzerinden mesajlaşma
 * - Komut geçmişi tutma
 * - Sistem kaynaklarının yönetimi
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <string.h>
 #include <fcntl.h>
 #include <signal.h>
 #include <sys/mman.h>
 #include <semaphore.h>
 #include <sys/wait.h>
 #include <sys/time.h>
 #include <time.h>
 #include <ctype.h> // isspace için
 
 #include "view.h" // view_append_output için gerekli olabilir
 
 #define MAX_MSG_LEN 256       // Maksimum mesaj uzunluğu
 #define SHM_NAME "/terminal_shm"  // Paylaşılan bellek ismi
 #define SEM_NAME "/terminal_sem"  // Semafor ismi
 
 /**
  * @brief Process bilgilerini tutan veri yapısı
  */
 #define MAX_PROCESSES 100     // Maksimum takip edilebilecek process sayısı
 typedef struct {
     pid_t pid;                // Process ID
     char command[256];        // Çalıştırılan komut
     int status;               // Durum: 0=çalışıyor, 1=tamamlandı, 2=sonlandırıldı
     time_t start_time;        // Başlangıç zamanı
     int tab_index;            // Hangi sekmeden başlatıldığı
 } ProcessInfo;
 
 // Process tablosu ve sayısı
 static ProcessInfo process_table[MAX_PROCESSES];
 static int process_count = 0;
 
 // Paylaşılan bellek değişkenleri
 static int shm_fd = -1;
 char *shm_ptr = NULL;         // Paylaşılan bellek işaretçisi (dışarıdan erişilebilir)
 static sem_t *sem = NULL;     // Paylaşılan bellek için semafor
 
 // Komut geçmişi için değişkenler
 #define HISTORY_LIMIT 50
 static char *command_history[HISTORY_LIMIT];
 static int history_count = 0;
 
 // Komut çıktılarını controller'a iletmek için callback
 typedef void (*OutputCallback)(int tab_index, const char *text, const char *color);
 static OutputCallback output_callback = NULL;
 
 /**
  * @brief Komut çıktı callback'ini ayarlar
  * * @param callback Çıktıları işleyecek fonksiyon
  */
 void model_set_output_callback(OutputCallback callback) {
     output_callback = callback;
 }
 
 /**
  * @brief Process tablosuna yeni bir process ekler
  * * @param pid Process ID
  * @param command Çalıştırılan komut
  * @param tab_index Komutun çalıştırıldığı sekme
  * @return int Process tablosundaki indeks veya hata durumunda -1
  */
 int add_process(pid_t pid, const char* command, int tab_index) {
     if (process_count >= MAX_PROCESSES) return -1;
     
     ProcessInfo *proc = &process_table[process_count];
     proc->pid = pid;
     strncpy(proc->command, command, 255);
     proc->command[255] = '\0';
     proc->status = 0;                  // running
     proc->start_time = time(NULL);     // mevcut zaman
     proc->tab_index = tab_index;
     
     return process_count++;
 }
 
 /**
  * @brief PID'ye göre process tablosunda arama yapar
  * * @param pid Aranacak Process ID
  * @return ProcessInfo* Bulunan process bilgisi veya NULL
  */
 ProcessInfo* find_process(pid_t pid) {
     for (int i = 0; i < process_count; i++) {
         if (process_table[i].pid == pid) {
             return &process_table[i];
         }
     }
     return NULL;
 }
 
 /**
  * @brief Process durumunu günceller
  * * @param pid Güncellenecek process ID
  * @param status Yeni durum (0=çalışıyor, 1=tamamlandı, 2=sonlandırıldı)
  */
 void update_process_status(pid_t pid, int status) {
     ProcessInfo *proc = find_process(pid);
     if (proc) {
         proc->status = status;
     }
 }
 
 /**
  * @brief Process tablosunu temizler (tamamlanan processleri kaldırır)
  */
 void clean_process_table() {
     int i = 0;
     while (i < process_count) {
         if (process_table[i].status != 0) {
             // Process çalışmayı bitirmiş, tablodan çıkar
             if (i < process_count - 1) {
                 memmove(&process_table[i], &process_table[i+1], 
                         sizeof(ProcessInfo) * (process_count - i - 1));
             }
             process_count--;
         } else {
             i++;
         }
     }
 }
 
 /**
  * @brief Zombie processleri kontrol edip temizler
  * * waitpid() ile tamamlanan processleri yakalar ve durumlarını günceller
  */
 void check_zombie_processes() {
     int status;
     pid_t pid;
     
     while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
         ProcessInfo *proc = find_process(pid);
         if (proc) {
             proc->status = 1; // completed
         }
     }
     
     // Belirli aralıklarla process tablosunu temizle
     // clean_process_table(); // Bunu daha seyrek çağırmak daha verimli olabilir
 }
 
 /**
  * @brief Çalışan tüm processlerin listesini döndürür
  * * @return char* Process listesi (PID, DURUM, KOMUT formatında)
  */
 char* get_process_list() {
     static char buffer[4096]; // Güvenli bir boyut
     buffer[0] = '\0';
     
     check_zombie_processes(); // Önce zombie'leri temizle
     
     strcat(buffer, "PID\tSTATUS\tCOMMAND\n");
     for (int i = 0; i < process_count; i++) {
         char line[512];
         const char* status_str = process_table[i].status == 0 ? "RUNNING" : 
                                 (process_table[i].status == 1 ? "DONE" : "KILLED");
         
         snprintf(line, sizeof(line), "%d\t%s\t%s\n", 
                  process_table[i].pid, status_str, process_table[i].command);
         strcat(buffer, line);
     }
     
     return buffer;
 }
 
 /**
  * @brief Paylaşılan belleği ve semaforu başlatır
  */
 void model_init_shared_memory() {
     // Paylaşılan bellek oluştur ve aç
     shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
     if (shm_fd == -1) {
         perror("shm_open failed");
         exit(1);
     }
     if (ftruncate(shm_fd, MAX_MSG_LEN) == -1) {
         perror("ftruncate failed");
         close(shm_fd);
         exit(1);
     }
     shm_ptr = mmap(0, MAX_MSG_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
     if (shm_ptr == MAP_FAILED) {
         perror("mmap failed");
         close(shm_fd);
         exit(1);
     }
 
     // Semafor oluştur (veya var olanı aç)
     sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
     if (sem == SEM_FAILED) {
         perror("sem_open failed");
         munmap(shm_ptr, MAX_MSG_LEN);
         close(shm_fd);
         exit(1);
     }
 }
 
 /**
  * @brief Model katmanını başlatır
  * * Process tablosunu sıfırlar ve paylaşılan belleği başlatır
  */
 void model_init() {
     // Process tablosunu sıfırla
     memset(process_table, 0, sizeof(process_table));
     process_count = 0;
     
     // Paylaşılan belleği başlat (Eğer main'de zaten çağrılıyorsa burada tekrar çağırmaya gerek yok)
     // model_init_shared_memory(); // Genellikle main'de bir kere çağrılır
 }
 
 /**
  * @brief Paylaşılan belleğe mesaj gönderir
  * * @param tab_index Mesajı gönderen sekme indeksi
  * @param msg Gönderilecek mesaj metni
  */
 void model_send_message(int tab_index, const char *msg) {
     if (!shm_ptr || !sem) return;
     
     sem_wait(sem);  // Semafor kilidi al
     // [Tab X]: formatında mesaj oluştur
     snprintf(shm_ptr, MAX_MSG_LEN, "[Tab %d]: %s", tab_index + 1, msg);
     sem_post(sem);  // Semafor kilidi bırak
 }
 
 /**
  * @brief Paylaşılan bellekten mesaj okur ve görüntüler (Bu fonksiyon artık Controller'da handle ediliyor gibi)
  * * @param tab_index Mesajı gösterilecek sekme
  */
 void model_read_message(int tab_index) {
     // Bu fonksiyonun içeriği Controller'daki check_messages fonksiyonuna taşınmış olabilir.
     // Eğer hala kullanılacaksa:
     if (!shm_ptr || !sem) return;
     
     sem_wait(sem);  // Semafor kilidi al
     if (strlen(shm_ptr) > 0) {
         // Mesajı görüntüle (View fonksiyonu çağrılmalı)
         if (output_callback) { // Veya doğrudan view fonksiyonu
              view_append_output_colored(tab_index, shm_ptr, "deepskyblue");
              view_append_output(tab_index, "\n"); // Yeni satır ekle
         }
         shm_ptr[0] = '\0';  // Mesajı temizle
     }
     sem_post(sem);  // Semafor kilidi bırak
 }
 
 /**
  * @brief Paylaşılan bellekteki mesajı temizlemeden okur
  * * @return const char* Mevcut mesaj veya boş string
  */
 const char* model_peek_message() {
     return shm_ptr ? shm_ptr : ""; // NULL kontrolü eklendi
 }
 
 /**
  * @brief Paylaşılan bellekteki mesajı temizler
  */
 void model_clear_message() {
     if (shm_ptr && sem) { // Semafor kontrolü de eklendi
         sem_wait(sem);
         if (shm_ptr) shm_ptr[0] = '\0';
         sem_post(sem);
     } else if (shm_ptr) {
         // Semafor yoksa doğrudan temizle (ideal değil)
         shm_ptr[0] = '\0';
     }
 }
 
 /**
  * @brief Komut geçmişine komut ekler
  * * @param cmdline Eklenecek komut
  */
 void model_add_to_history(const char *cmdline) {
     // Boş veya sadece boşluk içeren komutları ekleme
     const char *p = cmdline;
     while (*p && isspace((unsigned char)*p)) {
         p++;
     }
     if (*p == '\0') {
         return; 
     }
 
     // Aynı komut peş peşe eklenmesin (isteğe bağlı)
     if (history_count > 0 && strcmp(command_history[history_count - 1], cmdline) == 0) {
         return;
     }
 
     if (history_count == HISTORY_LIMIT) {
         // Geçmiş limiti dolduğunda en eskiyi sil
         free(command_history[0]);
         memmove(&command_history[0], &command_history[1], sizeof(char*) * (HISTORY_LIMIT - 1));
         history_count--;
     }
     command_history[history_count++] = strdup(cmdline);  // Komutun kopyasını ekle
 }
 
 /**
  * @brief Belirtilen indeksteki geçmiş komutu döndürür
  * * @param index Geçmiş komut indeksi
  * @return const char* Komut metni veya NULL
  */
 const char* model_get_history(int index) {
     if (index < 0 || index >= history_count) return NULL;
     return command_history[index];
 }
 
 /**
  * @brief Toplam geçmiş komut sayısını döndürür
  * * @return int Komut sayısı
  */
 int model_get_history_count() {
     return history_count;
 }
 
 /**
  * @brief Model tarafından kullanılan kaynakları temizler
  * * Paylaşılan bellek, semafor ve komut geçmişi için ayrılan belleği serbest bırakır
  */
 void model_cleanup() {
     if (shm_ptr != MAP_FAILED && shm_ptr != NULL) munmap(shm_ptr, MAX_MSG_LEN);
     if (shm_fd != -1) {
         close(shm_fd);
         shm_unlink(SHM_NAME); // Paylaşılan bellek nesnesini kaldır
     }
     if (sem != SEM_FAILED && sem != NULL) {
          sem_close(sem);
          sem_unlink(SEM_NAME); // Semaforu kaldır
     }
 
     // Komut geçmişini temizle
     for (int i = 0; i < history_count; i++) {
         free(command_history[i]);
         command_history[i] = NULL; // İşaretçiyi NULL yap
     }
     history_count = 0; // Sayacı sıfırla
 }
 
 
 // ------------------- YENİ EKLENEN KISIM BAŞLANGICI -------------------
 
 #define MAX_ARGS 64 // Bir komut için maksimum argüman sayısı
 
 /**
  * @brief Komut satırını boşluklara göre ayırır ve argv dizisi oluşturur.
  * * @param cmdline_orig Ayrıştırılacak orijinal komut satırı string'i.
  * @param argv Sonuçların yazılacağı char* dizisi (sonu NULL ile bitmeli).
  * @param max_args argv dizisinin maksimum kapasitesi.
  * @return int Argüman sayısı veya hata durumunda -1.
  * * @note Bu fonksiyon basit bir ayrıştırıcıdır, tırnak işaretleri veya
  * kaçış karakterleri gibi karmaşık durumları işlemez.
  * strtok_r kullandığı için orijinal string'i değiştirir, bu yüzden kopya kullanılır.
  */
 static int parse_command(const char *cmdline_orig, char *argv[], int max_args) {
     char *cmdline_copy = strdup(cmdline_orig); // strtok_r orijinali bozar, kopya al
     if (!cmdline_copy) {
         perror("strdup failed in parse_command");
         return -1;
     }
 
     char *token;
     char *saveptr; // strtok_r için
     int arg_count = 0;
     const char *delimiters = " \t\n\r"; // Ayraçlar: boşluk, tab, yeni satır vs.
 
     token = strtok_r(cmdline_copy, delimiters, &saveptr);
     while (token != NULL && arg_count < max_args - 1) { // -1: Son NULL için yer bırak
         argv[arg_count++] = token;
         token = strtok_r(NULL, delimiters, &saveptr);
     }
 
     argv[arg_count] = NULL; // Argv dizisini NULL ile sonlandır
 
     // ÖNEMLİ: argv'deki işaretçiler artık cmdline_copy içindeki yerleri gösteriyor.
     // Bu yüzden execvp çağrısından *sonra* değil, *önce* free(cmdline_copy)
     // yapmak execvp çalışırsa mümkün olmaz. Bu bellek sızıntısı yaratır.
     // Daha iyi bir çözüm: Argümanları da strdup ile kopyalayıp sonra free etmek
     // veya execvp sonrası ana süreçte kopyayı free etmek (bu örnekte yapılamaz).
     // Şimdilik bu basit versiyonda sızıntı göz ardı ediliyor veya
     // execvp başarısız olursa free ediliyor.
 
     // free(cmdline_copy); // YANLIŞ YER - execvp'den sonra buraya gelinmez
 
     if (arg_count == 0) {
         free(cmdline_copy); // Boş komutsa kopyayı free et
         return 0; // Boş komut, hata değil ama argüman yok
     }
 
     // Başarılı ayrıştırma durumunda, free(cmdline_copy) çocuk süreçte
     // execvp'nin üzerine yazılacağı için yapılamaz. Ana süreçte de yapılamaz.
     // Bu basit örnekte küçük bir bellek sızıntısı olacaktır. Daha gelişmiş
     // yöntemler (argümanları da kopyalamak) bu sızıntıyı önleyebilir.
 
     return arg_count;
 }
 
 // ------------------- YENİ EKLENEN KISIM SONU ---------------------
 
 
 /**
  * @brief Komut çalıştırma fonksiyonu (Doğrudan execvp kullanan versiyon)
  * * Shell komutlarını çalıştırır, çıktılarını yakalar ve görüntüler.
  * Özel komutlar (ps) için farklı işlemler yapar.
  * * @param tab_index Komutun çalıştırılacağı sekme
  * @param cmdline Çalıştırılacak komut
  */
 void model_execute_command(int tab_index, const char *cmdline) {
     model_add_to_history(cmdline);  // Geçmişe ekle
 
     // "ps" özel komutu: çalışan süreçleri listele
     if (strcmp(cmdline, "ps") == 0) {
         char *process_list = get_process_list();
         if (output_callback) {
             // 'ps' komutunun çıktısını farklı bir renkle gösterebiliriz
             output_callback(tab_index, process_list, "lightgreen"); 
         }
         return;
     }
     
     // Boş komutu çalıştırma
     const char *p = cmdline;
     while (*p && isspace((unsigned char)*p)) p++;
     if (*p == '\0') return;
 
 
     // Pipe oluştur (çıktıları yakalamak için)
     int pipefd[2];
     if (pipe(pipefd) == -1) {
         perror("pipe failed");
         if (output_callback) output_callback(tab_index, "[Hata: Pipe oluşturulamadı]\n", "red");
         return;
     }
 
     // Fork ile yeni process oluştur
     pid_t pid = fork();
     if (pid == -1) {
         // Fork hatası
         perror("fork failed");
         close(pipefd[0]);
         close(pipefd[1]);
         if (output_callback) output_callback(tab_index, "[Hata: Süreç oluşturulamadı]\n", "red");
         return;
     } 
     
     if (pid == 0) {
         // ------- Çocuk (Child) süreç -------
         close(pipefd[0]);  // Okuma ucunu kapat
         
         // stdout ve stderr'i pipe'a yönlendir
         if (dup2(pipefd[1], STDOUT_FILENO) == -1 || dup2(pipefd[1], STDERR_FILENO) == -1) {
             perror("dup2 failed in child");
             close(pipefd[1]);
             exit(1); // Hata ile çık
         }
         close(pipefd[1]); // Yönlendirme sonrası artık gereksiz
 
         // --- DEĞİŞİKLİK BAŞLANGICI ---
         // Komutu ayrıştır ve argv dizisi oluştur
         char *my_argv[MAX_ARGS]; 
         char *cmdline_copy_for_parsing = strdup(cmdline); // Kopyasını al
         if (!cmdline_copy_for_parsing) {
              perror("strdup failed in child");
              exit(1);
         }
 
         int arg_count = parse_command(cmdline_copy_for_parsing, my_argv, MAX_ARGS);
 
         if (arg_count <= 0) {
              // parse_command zaten hata mesajı basmış olabilir veya boş komut.
              fprintf(stderr, "Invalid or empty command for execution.\n");
              free(cmdline_copy_for_parsing); // Kopyayı serbest bırak
              exit(1); // Hata ile çık
         }
 
         // Komutu doğrudan çalıştır
         execvp(my_argv[0], my_argv);
         // --- DEĞİŞİKLİK SONU ---
 
         // Eğer buraya gelirse execvp başarısız olmuştur
         // Hata mesajı execvp'nin kendisi tarafından stderr'e (pipe'a) yazılmış olmalı.
         perror("execvp failed"); 
         fprintf(stderr, "Command not found or execution failed: %s\n", my_argv[0]); // Ekstra bilgi
         free(cmdline_copy_for_parsing); // execvp başarısız olursa kopyayı free et
         exit(1); // Hata kodu ile çık
 
     } else {
         // ------- Ana (Parent) süreç -------
         close(pipefd[1]);  // Yazma ucunu kapat
         
         // Process tablosuna ekle
         add_process(pid, cmdline, tab_index);
 
         // Pipe'dan çıktıları oku ve görüntüle (Zaman aşımı ve limit kontrolü ile)
         int status = 0; // waitpid için durum değişkeni
         char buffer[256];
         ssize_t n;
         size_t total_bytes = 0;
         const size_t max_bytes = 100000;  // Maksimum çıktı limiti
         int child_exited = 0;             // Çocuk sürecin bitip bitmediğini takip et
 
         // select kullanarak hem pipe'ı dinle hem de zaman aşımı uygula
         while (total_bytes < max_bytes) {
             fd_set read_fds;
             struct timeval timeout;
 
             FD_ZERO(&read_fds);
             FD_SET(pipefd[0], &read_fds);
 
             // Timeout: Kısa aralıklarla kontrol et
             timeout.tv_sec = 0;
             timeout.tv_usec = 50000; // 50ms
 
             int ready = select(pipefd[0] + 1, &read_fds, NULL, NULL, &timeout);
 
             if (ready == -1) { // select hatası
                  perror("select failed");
                  break;
             } else if (ready > 0) { // Pipe'dan okunacak veri var
                 n = read(pipefd[0], buffer, sizeof(buffer) - 1);
                 if (n > 0) { // Veri okundu
                     buffer[n] = '\0';
                     if (output_callback) {
                          output_callback(tab_index, buffer, NULL); // Rengi Controller belirlesin
                     }
                     total_bytes += n;
                 } else { // n <= 0: Pipe kapandı (çocuk süreç bitti veya hata)
                     child_exited = 1;
                     break; 
                 }
             } else { // ready == 0: Timeout, veri gelmedi
                 // Çocuk sürecin bitip bitmediğini kontrol et (zombie olmasın)
                 pid_t result = waitpid(pid, &status, WNOHANG);
                 if (result == pid) { // Çocuk süreç bitmiş
                      child_exited = 1;
                      break;
                 } else if (result == -1) { // waitpid hatası
                      perror("waitpid error during poll");
                      break;
                 }
                 // Çocuk hala çalışıyor, döngüye devam
             }
         } // while döngüsü sonu
 
         // Döngüden çıkıldıktan sonra kontroller
         if (total_bytes >= max_bytes) {
             if (output_callback) {
                 output_callback(tab_index, "\n[Çıktı limiti aşıldı, kesildi...]\n", "orange");
             }
             // Limit aşıldıysa süreci sonlandır
             kill(pid, SIGKILL); 
             waitpid(pid, &status, 0); // Zombie olmasını bekle
             update_process_status(pid, 2); // killed
             child_exited = 1; // Artık bitmiş sayılır
         }
         
         // Eğer çocuk süreç döngü içinde bitmediyse, burada bekle
         if (!child_exited) {
              if (waitpid(pid, &status, 0) == pid) { // Başarıyla bitti
                  update_process_status(pid, 1); // completed
              } else { // Beklerken hata oluştu veya sinyal aldı
                  // Durumu kontrol et, belki zorla sonlandırıldı
                  if (!find_process(pid) || find_process(pid)->status == 0) {
                      // Eğer hala running görünüyorsa ve bekleme başarısızsa, killed sayılabilir
                      update_process_status(pid, 2); // veya başka bir hata durumu
                  }
              }
         } else {
              // Döngü içinde zaten bittiği tespit edildiyse, durumunu güncelle
              if (find_process(pid) && find_process(pid)->status == 0) { // Hala running görünüyorsa
                  update_process_status(pid, 1); // completed
              }
         }
 
         close(pipefd[0]); // Okuma ucunu kapat
     } // Ana süreç sonu
 }