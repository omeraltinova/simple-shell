#ifndef VIEW_H
#define VIEW_H

#include <gtk/gtk.h>  // GTK header dosyasını ekle

void view_init(int argc, char **argv);
void view_main_loop();
void view_create_tab();
void view_append_output(int tab_index, const char *text);
void view_append_output_colored(int tab_index, const char *text, const char *color);
void view_clear_terminal(int tab_index);
void view_set_input_callback(void (*callback)(int tab_index, const char *input));
GtkWidget* view_get_output_widget(int tab_index);

#endif
