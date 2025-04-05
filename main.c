// main.c - Uygulama giriş noktası

#include "controller.h"
#include "model.h"

int main(int argc, char **argv) {
    model_init_shared_memory();
    controller_start(argc, argv);
    model_cleanup();
    return 0;
}