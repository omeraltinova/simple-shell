/**
 * @file model.c
 * @brief MVC mimarisinin Model katmanı
 * 
 * Bu modül, veri ve iş mantığını yönetir:
 * - Komut çalıştırma ve process yönetimi
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

#include "view.h"

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
 * 
 * @param callback Çıktıları işleyecek fonksiyon
 */
void model_set_output_callback(OutputCallback callback) {
    output_callback = callback;
}

/**
 * @brief Process tablosuna yeni bir process ekler
 * 
 * @param pid Process ID
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
 * 
 * @param pid Aranacak Process ID
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
 * 
 * @param pid Güncellenecek process ID
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
 * 
 * waitpid() ile tamamlanan processleri yakalar ve durumlarını günceller
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
    // clean_process_table();
}

/**
 * @brief Çalışan tüm processlerin listesini döndürür
 * 
 * @return char* Process listesi (PID, DURUM, KOMUT formatında)
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
    ftruncate(shm_fd, MAX_MSG_LEN);
    shm_ptr = mmap(0, MAX_MSG_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // Semafor oluştur
    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
}

/**
 * @brief Model katmanını başlatır
 * 
 * Process tablosunu sıfırlar ve paylaşılan belleği başlatır
 */
void model_init() {
    // Process tablosunu sıfırla
    memset(process_table, 0, sizeof(process_table));
    process_count = 0;
    
    // Paylaşılan belleği başlat
    model_init_shared_memory();
}

/**
 * @brief Paylaşılan belleğe mesaj gönderir
 * 
 * @param tab_index Mesajı gönderen sekme indeksi
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
 * @brief Paylaşılan bellekten mesaj okur ve görüntüler
 * 
 * @param tab_index Mesajı gösterilecek sekme
 */
void model_read_message(int tab_index) {
    if (!shm_ptr || !sem) return;
    
    sem_wait(sem);  // Semafor kilidi al
    if (strlen(shm_ptr) > 0) {
        // Mesajı görüntüle
        view_append_output(tab_index, shm_ptr);
        view_append_output(tab_index, "\n");
        shm_ptr[0] = '\0';  // Mesajı temizle
    }
    sem_post(sem);  // Semafor kilidi bırak
}

/**
 * @brief Paylaşılan bellekteki mesajı temizlemeden okur
 * 
 * @return const char* Mevcut mesaj veya boş string
 */
const char* model_peek_message() {
    return shm_ptr;
}

/**
 * @brief Paylaşılan bellekteki mesajı temizler
 */
void model_clear_message() {
    if (shm_ptr) shm_ptr[0] = '\0';
}

/**
 * @brief Komut geçmişine komut ekler
 * 
 * @param cmdline Eklenecek komut
 */
void model_add_to_history(const char *cmdline) {
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
 * 
 * @param index Geçmiş komut indeksi
 * @return const char* Komut metni veya NULL
 */
const char* model_get_history(int index) {
    if (index < 0 || index >= history_count) return NULL;
    return command_history[index];
}

/**
 * @brief Toplam geçmiş komut sayısını döndürür
 * 
 * @return int Komut sayısı
 */
int model_get_history_count() {
    return history_count;
}

/**
 * @brief Model tarafından kullanılan kaynakları temizler
 * 
 * Paylaşılan bellek, semafor ve komut geçmişi için ayrılan belleği serbest bırakır
 */
void model_cleanup() {
    if (shm_ptr) munmap(shm_ptr, MAX_MSG_LEN);
    if (shm_fd != -1) close(shm_fd);
    if (sem) sem_close(sem);

    // Komut geçmişini temizle
    for (int i = 0; i < history_count; i++) {
        free(command_history[i]);
    }
}

/**
 * @brief Komut çalıştırma fonksiyonu
 * 
 * Shell komutlarını çalıştırır, çıktılarını yakalar ve görüntüler.
 * Özel komutlar (ps) için farklı işlemler yapar.
 * 
 * @param tab_index Komutun çalıştırılacağı sekme
 * @param cmdline Çalıştırılacak komut
 */
void model_execute_command(int tab_index, const char *cmdline) {
    model_add_to_history(cmdline);  // Geçmişe ekle

    // "ps" özel komutu: çalışan süreçleri listele
    if (strcmp(cmdline, "ps") == 0) {
        char *process_list = get_process_list();
        if (output_callback) {
            output_callback(tab_index, process_list, "lightgreen");
        }
        return;
    }

    // Pipe oluştur (çıktıları yakalamak için)
    int pipefd[2];
    if (pipe(pipefd) == -1) return;

    // Fork ile yeni process oluştur
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        close(pipefd[0]);  // Okuma ucunu kapat
        
        // stdout ve stderr'i pipe'a yönlendir
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // Komutu çalıştır
        char *bash_args[] = { "sh", "-c", (char *)cmdline, NULL };
        execvp("sh", bash_args);

        // Eğer buraya gelirse execvp başarısız olmuştur
        perror("execvp failed");
        exit(1);
    } else {
        // Parent process
        close(pipefd[1]);  // Yazma ucunu kapat
        
        // Process tablosuna ekle
        add_process(pid, cmdline, tab_index);

        // Zaman aşımı kontrolü
        int status = 0;
        int waited = 0;
        const int timeout_ms = 3000;
        const int step = 100;

        // Child process'i belirli bir süre bekle
        while (waited < timeout_ms) {
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == 0) {
                // Hala çalışıyor, biraz daha bekle
                usleep(step * 1000);
                waited += step;
            } else if (result == pid) {
                // Process tamamlandı
                update_process_status(pid, 1); // completed
                break;
            } else {
                // Hata oluştu
                break;
            }
        }

        // Zaman aşımı kontrolü
        if (waited >= timeout_ms) {
            // Process takıldı, sonlandır
            kill(pid, SIGKILL);
            update_process_status(pid, 2); // killed
            if (output_callback) {
                output_callback(tab_index, "\n[Komut zaman aşımına uğradı]\n", "red");
            }
            waitpid(pid, NULL, 0);
        }

        // Pipe'dan çıktıları oku ve görüntüle
        fd_set read_fds;
        struct timeval timeout;
        char buffer[256];
        ssize_t n;
        size_t total_bytes = 0;
        const size_t max_bytes = 100000;  // Maksimum çıktı limiti

        while (1) {
            FD_ZERO(&read_fds);
            FD_SET(pipefd[0], &read_fds);

            timeout.tv_sec = 0;
            timeout.tv_usec = 100000; // 100ms

            int ready = select(pipefd[0] + 1, &read_fds, NULL, NULL, &timeout);
            if (ready > 0) {
                n = read(pipefd[0], buffer, sizeof(buffer) - 1);
                if (n <= 0) break;

                buffer[n] = '\0';
                view_append_output(tab_index, buffer);
                total_bytes += n;

                if (total_bytes >= max_bytes) {
                    view_append_output(tab_index, "\n[Çıktı limiti aşıldı, kesildi...]\n");
                    break;
                }
            } else {
                break;
            }
        }
        close(pipefd[0]);
    }
}