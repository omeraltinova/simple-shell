#ifndef VIEW_H
#define VIEW_H

void view_init(int argc, char **argv);
void view_main_loop();
void view_create_tab();
void view_append_output(int tab_index, const char *text);
void view_set_input_callback(void (*callback)(int tab_index, const char *input));

#endif
