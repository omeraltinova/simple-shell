// model.c - Komut çalıştırma ve shared memory/messaging

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

#include "view.h"

#define MAX_MSG_LEN 256
#define SHM_NAME "/terminal_shm"
#define SEM_NAME "/terminal_sem"

static int shm_fd = -1;
char *shm_ptr = NULL; // Artık dışarıdan erişilebilir
static sem_t *sem = NULL;

#define HISTORY_LIMIT 50
static char *command_history[HISTORY_LIMIT];
static int history_count = 0;

void model_init_shared_memory() {
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, MAX_MSG_LEN);
    shm_ptr = mmap(0, MAX_MSG_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
}

void model_send_message(int tab_index, const char *msg) {
    if (!shm_ptr || !sem) return;
    sem_wait(sem);
    snprintf(shm_ptr, MAX_MSG_LEN, "[Tab %d]: %s", tab_index + 1, msg);
    sem_post(sem);
}

void model_read_message(int tab_index) {
    if (!shm_ptr || !sem) return;
    sem_wait(sem);
    if (strlen(shm_ptr) > 0) {
        view_append_output(tab_index, shm_ptr);
        view_append_output(tab_index, "\n");
        shm_ptr[0] = '\0';
    }
    sem_post(sem);
}

const char* model_peek_message() {
    return shm_ptr;
}

void model_clear_message() {
    if (shm_ptr) shm_ptr[0] = '\0';
}

void model_add_to_history(const char *cmdline) {
    if (history_count == HISTORY_LIMIT) {
        free(command_history[0]);
        memmove(&command_history[0], &command_history[1], sizeof(char*) * (HISTORY_LIMIT - 1));
        history_count--;
    }
    command_history[history_count++] = strdup(cmdline);
}

const char* model_get_history(int index) {
    if (index < 0 || index >= history_count) return NULL;
    return command_history[index];
}

int model_get_history_count() {
    return history_count;
}



void model_cleanup() {
    if (shm_ptr) munmap(shm_ptr, MAX_MSG_LEN);
    if (shm_fd != -1) close(shm_fd);
    if (sem) sem_close(sem);

    for (int i = 0; i < history_count; i++) {
        free(command_history[i]);
    }
}
void model_execute_command(int tab_index, const char *cmdline) {
    model_add_to_history(cmdline);

    int pipefd[2];
    if (pipe(pipefd) == -1) return;

    pid_t pid = fork();
    if (pid == 0) {
        // Child
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        char *bash_args[] = { "sh", "-c", (char *)cmdline, NULL };
        execvp("sh", bash_args);

        perror("execvp failed");
        exit(1);
    } else {
        // Parent
        close(pipefd[1]);

        int status = 0;
        int waited = 0;
        const int timeout_ms = 3000;
        const int step = 100;

        while (waited < timeout_ms) {
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == 0) {
                usleep(step * 1000);
                waited += step;
            } else if (result == pid) {
                break;
            } else {
                break;
            }
        }

        if (waited >= timeout_ms) {
            kill(pid, SIGKILL);
            view_append_output(tab_index, "\n[Muhtemel hatalı girişten dolayı komut zaman aşımına uğradı ve sonlandırıldı]\n");
            waitpid(pid, NULL, 0);
        }

        fd_set read_fds;
        struct timeval timeout;
        char buffer[256];
        ssize_t n;
        size_t total_bytes = 0;
        const size_t max_bytes = 100000;

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