#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "../include/hello.h"
#include "../include/tc.h"
#include "../include/olsr.h"
#include "../include/routing.h"
// Control queue functions are declared in olsr.h
struct control_queue global_ctrl_queue;

/**
 * @brief Process a structured message received from RRC/TDMA layer
 * 
 * This function processes already deserialized OLSR messages received from
 * the RRC/TDMA layer. The RRC layer handles deserialization and passes
 * structured messages directly to OLSR for processing.
 * 
 * @param msg_type Type of the message (MSG_HELLO, MSG_TC, etc.)
 * @param message_ptr Pointer to the deserialized message structure
 * @param sender_addr IP address of sender
 * @return 0 on success, -1 on error
 */
int process_received_message(uint8_t msg_type, void* message_ptr, uint32_t sender_addr) {
    if (!message_ptr) {
        printf("Error: Invalid message pointer\n");
        return -1;
    }
    
    printf("\n=== PROCESSING MESSAGE from sender 0x%08X (type=%d) ===\n", sender_addr, msg_type);
    
    if (msg_type == MSG_HELLO) {
        // Create OLSR message wrapper for HELLO
        struct olsr_message msg;
        msg.msg_type = MSG_HELLO;
        msg.vtime = 6;  // Standard HELLO validity
        msg.originator = sender_addr;
        msg.ttl = 1;
        msg.hop_count = 0;
        msg.body = message_ptr;  // Point to the deserialized HELLO structure
        
        // Process the HELLO message
        process_hello_message(&msg, sender_addr);
        printf("=== HELLO MESSAGE PROCESSED ===\n\n");
        return 0;
    } else if (msg_type == MSG_TC) {
        // Create OLSR message wrapper for TC
        struct olsr_message msg;
        msg.msg_type = MSG_TC;
        msg.vtime = 15;  // TC validity time
        msg.originator = sender_addr;
        msg.ttl = 255;
        msg.hop_count = 0;
        msg.body = message_ptr;  // Point to the deserialized TC structure
        
        // Process the TC message
        process_tc_message(&msg, sender_addr);
        printf("=== TC MESSAGE PROCESSED ===\n\n");
        return 0;
    }
    
    printf("Warning: Unknown message type %d\n", msg_type);
    return -1;
}

void init_olsr(void){
    // Initialization code for OLSR protocol
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
            printf("Processing message of type %d\n", msg.msg_type);
            
            if (msg.msg_type == MSG_HELLO) {
                printf("HELLO message ready for RRC/TDMA transmission\n");
                // RRC/TDMA layer can access the structure via msg.message_ptr
                struct olsr_hello* hello = (struct olsr_hello*)msg.message_ptr;
                printf("  - Willingness: %d, Neighbors: %d, Slot: %d\n",
                       hello->willingness, hello->neighbor_count, hello->reserved_slot);
                
                // After RRC/TDMA processes it, free the message
                free(hello->neighbors);
                if (hello->two_hop_neighbors) {
                    free(hello->two_hop_neighbors);
                }
                free(hello);
            } else if (msg.msg_type == MSG_TC) {
                printf("TC message ready for RRC/TDMA transmission\n");
                // RRC/TDMA layer can access the structure via msg.message_ptr
                struct olsr_tc* tc = (struct olsr_tc*)msg.message_ptr;
                printf("  - ANSN: %d, Selectors: %d\n",
                       tc->ansn, tc->selector_count);
                
                // After RRC/TDMA processes it, free the message
                free(tc->mpr_selectors);
                free(tc);
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