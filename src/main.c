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

// Forward declarations for global routing functions
int cleanup_duplicate_table(void);
int cleanup_topology_links(void);
int is_duplicate_message(uint32_t originator, uint16_t seq_number);
int add_duplicate_entry(uint32_t originator, uint16_t seq_number);
void process_tc_message(struct olsr_message* msg, uint32_t sender_addr);

/**
 * @brief Enhanced message processing with duplicate detection and forwarding
 * 
 * This function demonstrates the complete receive path for global OLSR:
 * 1. Receive serialized bytes (from MAC layer in real implementation)
 * 2. Parse message header for type and routing information
 * 3. Perform duplicate detection
 * 4. Deserialize to structured format
 * 5. Process message content
 * 6. Forward message if required (MPR flooding)
 * 
 * @param serialized_buffer Raw bytes received from network
 * @param buffer_size Size of received data
 * @param sender_addr IP address of immediate sender
 * @param originator_addr IP address of message originator
 * @param msg_type Type of message (MSG_HELLO, MSG_TC)
 * @param seq_num Message sequence number
 * @param ttl Time to live
 * @param hop_count Number of hops traveled
 * @return 0 on success, -1 on error
 */
int receive_and_process_message_enhanced(const uint8_t* serialized_buffer, size_t buffer_size, 
                                       uint32_t sender_addr, uint32_t originator_addr,
                                       uint8_t msg_type, uint16_t seq_num, uint8_t ttl, uint8_t hop_count) {
    if (!serialized_buffer || buffer_size == 0) {
        printf("Error: Invalid received buffer\n");
        return -1;
    }
    
    printf("\n=== ENHANCED MESSAGE PROCESSING ===\n");
    printf("Sender: 0x%08X, Originator: 0x%08X\n", sender_addr, originator_addr);
    printf("Type: %d, SeqNum: %d, TTL: %d, Hops: %d\n", msg_type, seq_num, ttl, hop_count);
    
    // Step 1: Duplicate detection (except for HELLO messages which are always local)
    if (msg_type != MSG_HELLO) {
        if (is_duplicate_message(originator_addr, seq_num)) {
            printf("DUPLICATE: Message already processed - discarding\n");
            return 0;  // Not an error, just duplicate
        }
        add_duplicate_entry(originator_addr, seq_num);
    }
    
    // Step 2: Process based on message type
    if (msg_type == MSG_HELLO) {
        struct olsr_hello hello_msg;
        memset(&hello_msg, 0, sizeof(hello_msg));
        
        int bytes_read = deserialize_hello(&hello_msg, serialized_buffer);
        if (bytes_read > 0) {
            struct olsr_message msg;
            msg.msg_type = MSG_HELLO;
            msg.vtime = 6;
            msg.originator = sender_addr;  // HELLO always from immediate sender
            msg.ttl = 1;
            msg.hop_count = 0;
            msg.msg_seq_num = seq_num;
            msg.body = &hello_msg;
            
            process_hello_message(&msg, sender_addr);
            printf("=== HELLO PROCESSING COMPLETE ===\n\n");
            return 0;
        }
    } else if (msg_type == MSG_TC) {
        struct olsr_tc tc_msg;
        memset(&tc_msg, 0, sizeof(tc_msg));
        
        int bytes_read = deserialize_tc(&tc_msg, serialized_buffer);
        if (bytes_read > 0) {
            struct olsr_message msg;
            msg.msg_type = MSG_TC;
            msg.vtime = TC_VALIDITY_TIME;
            msg.originator = originator_addr;  // TC keeps original originator
            msg.ttl = ttl;
            msg.hop_count = hop_count;
            msg.msg_seq_num = seq_num;
            msg.body = &tc_msg;
            
            // Process TC (includes forwarding logic)
            process_tc_message(&msg, sender_addr);
            printf("=== TC PROCESSING COMPLETE ===\n\n");
            return 0;
        }
    }
    
    printf("Warning: Unable to deserialize message type %d\n", msg_type);
    return -1;
}

/**
 * @brief Legacy message processing function (backward compatibility)
 */
int receive_and_process_message(const uint8_t* serialized_buffer, size_t buffer_size, uint32_t sender_addr) {
    // Use enhanced processing with default values
    return receive_and_process_message_enhanced(serialized_buffer, buffer_size,
                                              sender_addr, sender_addr, MSG_HELLO, 0, 1, 0);
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
    
    // Further initialization for global routing
    time_t last_tc_time = 0;
    time_t last_global_cleanup = 0;
    
    printf("OLSR Global Routing Loop Started\n");
    
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
        
        // Send TC messages at specified interval (if we have MPR selectors)
        if (now - last_tc_time >= TC_INTERVAL) {
            send_tc_message(&ctrl_queue);
            last_tc_time = now;
        }
        
        // Process outgoing messages from control queue
        if (pop_from_control_queue(&ctrl_queue, &msg) == 0) {
            printf("\n--- OUTGOING MESSAGE ---\n");
            printf("Type: %d, Size: %zu bytes\n", msg.msg_type, msg.data_size);
            
            // SEND PATH: In a real implementation, this would send via MAC layer
            // The MAC layer would transmit msg.msg_data (serialized bytes) over the network
            
            if (msg.msg_type == MSG_HELLO) {
                printf("HELLO message transmitted to all neighbors\n");
            } else if (msg.msg_type == MSG_TC) {
                printf("TC message flooded to network (TTL=255)\n");
            }
            printf("--- MESSAGE TRANSMITTED ---\n\n");
        }
        
        // Global routing maintenance every 30 seconds
        if (now - last_global_cleanup >= 30) {
            printf("\n=== GLOBAL ROUTING MAINTENANCE ===\n");
            
            // Cleanup expired control messages
            int expired_msgs = cleanup_expired_messages(&ctrl_queue);
            if (expired_msgs > 0) {
                printf("Cleaned up %d expired control messages\n", expired_msgs);
            }
            
            // Cleanup expired duplicate entries
            int expired_dupes = cleanup_duplicate_table();
            if (expired_dupes > 0) {
                printf("Cleaned up %d expired duplicate entries\n", expired_dupes);
            }
            
            // Cleanup expired topology links
            int expired_links = cleanup_topology_links();
            if (expired_links > 0) {
                printf("Cleaned up %d expired topology links\n", expired_links);
                topology_changed = 1;  // Recalculate routing if topology changed
            }
            
            printf("=== MAINTENANCE COMPLETE ===\n\n");
            last_global_cleanup = now;
        }
        
        // Recalculate routing table if topology changed
        if (topology_changed) {
            printf("TOPOLOGY_CHANGE: Recalculating routing table\n");
            update_routing_table();
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