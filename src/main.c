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
 * @brief Simulate receiving and processing a message from the network
 * 
 * This function demonstrates the complete receive path:
 * 1. Receive serialized bytes (from MAC layer in real implementation)
 * 2. Deserialize to structured format
 * 3. Create olsr_message wrapper
 * 4. Call appropriate processing function
 * 
 * @param serialized_buffer Raw bytes received from network
 * @param buffer_size Size of received data
 * @param sender_addr IP address of sender
 * @return 0 on success, -1 on error
 */
int receive_and_process_message(const uint8_t* serialized_buffer, size_t buffer_size, uint32_t sender_addr) {
    if (!serialized_buffer || buffer_size == 0) {
        printf("Error: Invalid received buffer\n");
        return -1;
    }
    
    printf("\n=== RECEIVING MESSAGE from sender 0x%08X (size=%zu bytes) ===\n", sender_addr, buffer_size);
    
    // In a complete implementation, you would parse a header to determine message type
    // For now, we'll check the serialized structure to identify the message type
    
    // Try to deserialize as HELLO (most common in OLSR)
    struct olsr_hello hello_msg;
    memset(&hello_msg, 0, sizeof(hello_msg));
    
    int bytes_read = deserialize_hello(&hello_msg, serialized_buffer);
    if (bytes_read > 0) {
        printf("Deserialized HELLO message: %d bytes\n", bytes_read);
        
        // Create OLSR message wrapper
        struct olsr_message msg;
        msg.msg_type = MSG_HELLO;
        msg.vtime = 6;  // Standard HELLO validity
        msg.originator = sender_addr;
        msg.ttl = 1;
        msg.hop_count = 0;
        msg.body = &hello_msg;
        
        // Process the HELLO message
        process_hello_message(&msg, sender_addr);
        printf("=== HELLO MESSAGE PROCESSED ===\n\n");
        return 0;
    }
    
    // Could add TC message deserialization here
    printf("Warning: Unable to deserialize message\n");
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
            printf("Processing message of type %d with size %zu\n", msg.msg_type, msg.data_size);
            
            // SEND PATH: In a real implementation, this would send via MAC layer
            // The MAC layer would transmit msg.msg_data (serialized bytes) over the network
            
            if (msg.msg_type == MSG_HELLO) {
                printf("HELLO message ready for transmission\n");
            } else if (msg.msg_type == MSG_TC) {
                printf("TC message ready for transmission\n");
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