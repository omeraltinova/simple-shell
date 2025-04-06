#ifndef MODEL_H
#define MODEL_H

void model_init(); // Eksik bildirim ekle
void model_init_shared_memory();
void model_execute_command(int tab_index, const char *cmdline);
void model_send_message(int tab_index, const char *msg);
void model_read_message(int tab_index);
void model_cleanup();
const char* model_peek_message();
void model_clear_message();
const char* model_get_history(int index);
int model_get_history_count();
void model_add_to_history(const char *cmdline); // Eksik bildirim ekle
void model_set_output_callback(void (*callback)(int tab_index, const char *text, const char *color)); // Eksik bildirim ekle

#endif
