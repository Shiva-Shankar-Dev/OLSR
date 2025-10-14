#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/hello.h"
#include "../include/tc.h"
#include "../include/olsr.h"
#include "../include/routing.h"
// Control queue functions are declared in olsr.h

void init_olsr(void){
    // Initialization code for OLSR daemon
    // Set up sockets, timers, data structures, etc.
    struct control_queue ctrl_queue;
    init_control_queue(&ctrl_queue);
    printf("OLSR Initialized\n");
    struct control_message msg;
    // Further initialization as needed
    while(1){
        send_hello_message(&ctrl_queue);
        pop_from_control_queue(&ctrl_queue, &msg);
        printf("Popped message of type %d with size %zu\n", msg.msg_type, msg.data_size);
        sleep(HELLO_INTERVAL);
    }
    
}
int main() {
    printf("OLSR Starting...\n");
    // Initialization code here
    init_olsr();
    return 0;
}