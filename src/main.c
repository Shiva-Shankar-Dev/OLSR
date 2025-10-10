#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/hello.h"
#include "../include/tc.h"
#include "../include/olsr.h"
#include "../include/routing.h"

void init_olsr(void){
    // Initialization code for OLSR daemon
    // Set up sockets, timers, data structures, etc.
    struct control_queue ctrl_queue;
    init_control_queue(&ctrl_queue);
    printf("OLSR Initialized\n");
    send_hello_message(&ctrl_queue);
    // Further initialization as needed
}
int main() {
    printf("OLSR Starting...\n");
    // Initialization code here
    init_olsr();
    return 0;
}