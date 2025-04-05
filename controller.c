// controller.c - Kullanıcı girişlerini işleyen katman

#include <stdio.h>
#include <string.h>
#include "model.h"
#include "view.h"

#define MAX_CMD_LEN 256

// Bu fonksiyon view'den gelen inputu işleyip ilgili model fonksiyonuna yönlendirir
void on_user_input(int tab_index, const char *input) {
    if (strncmp(input, "@msg ", 5) == 0) {
        const char *msg = input + 5;
        model_send_message(tab_index, msg);
        view_append_output(tab_index, "[mesaj gönderildi]\n");
    } else {
        model_execute_command(tab_index, input);
    }
}

void controller_start(int argc, char **argv) {
    view_init(argc, argv);
    view_set_input_callback(on_user_input);
    view_main_loop();
}