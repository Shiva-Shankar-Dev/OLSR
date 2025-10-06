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
    printf("OLSR Daemon Initialized\n");
    generate_hello_message();
    send_hello_message();
    send_tc_message();
    // Further initialization as needed
}
int main() {
    printf("OLSR Daemon Starting...\n");
    // Initialization code here
    init_olsr();
    return 0;
}