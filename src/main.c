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
    printf("OLSR Initialized with Link Failure Detection\n");
    struct control_message msg;
    
    time_t last_hello_time = 0;
    time_t last_timeout_check = 0;
    time_t last_cleanup = 0;
    int topology_changed = 0;
    
    // Further initialization as needed
    while(1){
        time_t now = time(NULL);
        
        // Check for neighbor timeouts every second
        if (now - last_timeout_check >= 1) {
            int failed_neighbors = check_neighbor_timeouts();
            if (failed_neighbors > 0) {
                topology_changed = 1;
                printf("TOPOLOGY CHANGE: %d neighbors failed timeout check\n", failed_neighbors);
                
                // Generate emergency HELLO after topology change
                if (generate_emergency_hello(&ctrl_queue) == 0) {
                    printf("Emergency HELLO generated due to topology change\n");
                }
            }
            last_timeout_check = now;
        }
        
        // Process retry queue for message retransmissions
        int retries_processed = process_retry_queue(&ctrl_queue);
        if (retries_processed > 0) {
            printf("Processed %d message retries\n", retries_processed);
        }
        
        // Send regular HELLO messages at specified interval
        if (now - last_hello_time >= HELLO_INTERVAL) {
            send_hello_message(&ctrl_queue);
            last_hello_time = now;
        }
        
        // Process outgoing messages from control queue
        if (pop_from_control_queue(&ctrl_queue, &msg) == 0) {
            printf("Processing message of type %d with size %zu\n", msg.msg_type, msg.data_size);
            
            // In a real implementation, this would send the message via MAC layer
            // For now, we just log the message processing
            
            if (msg.msg_type == MSG_HELLO) {
                printf("HELLO message ready for transmission\n");
            }
        }
        
        // Cleanup expired messages every 30 seconds
        if (now - last_cleanup >= 30) {
            int cleaned = cleanup_expired_messages(&ctrl_queue);
            if (cleaned > 0) {
                printf("Cleaned up %d expired messages\n", cleaned);
            }
            last_cleanup = now;
        }
        
        // Reset topology change flag after processing
        if (topology_changed) {
            topology_changed = 0;
        }
        
        // Small sleep to prevent busy waiting
        usleep(100000);  // 100ms sleep
    }

}
int main() {
    printf("OLSR Starting...\n");
    // Initialization code here
    init_olsr();
    return 0;
}