#define _DEFAULT_SOURCE  // For usleep()
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
void process_tc_message(struct olsr_message* msg, uint32_t sender_id);

/**
 * @brief Enhanced message processing with duplicate detection and forwarding
 * 
 * This function demonstrates the complete receive path for global OLSR:
 * 1. Receive structured message data (from RRC/TDMA layer)
 * 2. Check message header for type and routing information
 * 3. Perform duplicate detection
 * 4. Process message content directly (no deserialization needed)
 * 5. Forward message if required (MPR flooding)
 * 
 * @param message_ptr Pointer to structured message (olsr_hello*, olsr_tc*)
 * @param msg_type Type of message (MSG_HELLO, MSG_TC)
 * @param sender_id IP address of immediate sender
 * @param originator_id IP address of message originator
 * @param seq_num Message sequence number
 * @param ttl Time to live
 * @param hop_count Number of hops traveled
 * @return 0 on success, -1 on error
 */
int receive_control_message(void* message_ptr, uint8_t msg_type,
                                       uint32_t sender_id, uint32_t originator_id,
                                       uint16_t seq_num, uint8_t ttl, uint8_t hop_count) {
    if (!message_ptr) {
        printf("Error: Invalid message pointer\n");
        return -1;
    }
    
    printf("Sender: 0x%08X, Originator: 0x%08X\n", sender_id, originator_id);
    printf("Type: %d, SeqNum: %d, TTL: %d, Hops: %d\n", msg_type, seq_num, ttl, hop_count);
    
    // Step 1: Duplicate detection (except for HELLO messages which are always local)
    if (msg_type != MSG_HELLO) {
        if (is_duplicate_message(originator_id, seq_num)) {
            printf("DUPLICATE: Message already processed - discarding\n");
            return 0;  // Not an error, just duplicate
        }
        add_duplicate_entry(originator_id, seq_num);
    }
    
    // Step 2: Process based on message type
    if (msg_type == MSG_HELLO) {
        struct olsr_hello* hello_msg = (struct olsr_hello*)message_ptr;
        
        struct olsr_message msg;
        msg.msg_type = MSG_HELLO;
        msg.vtime = 6;
        msg.originator = sender_id;  // HELLO always from immediate sender
        msg.ttl = 1;
        msg.hop_count = 0;
        msg.msg_seq_num = seq_num;
        msg.body = hello_msg;
        
        process_hello_message(&msg, sender_id);
        printf("=== HELLO PROCESSING COMPLETE ===\n\n");
        return 0;
    } else if (msg_type == MSG_TC) {
        struct olsr_tc* tc_msg = (struct olsr_tc*)message_ptr;
        
        struct olsr_message msg;
        msg.msg_type = MSG_TC;
        msg.vtime = TC_VALIDITY_TIME;
        msg.originator = originator_id;  // TC keeps original originator
        msg.ttl = ttl;
        msg.hop_count = hop_count;
        msg.msg_seq_num = seq_num;
        msg.body = tc_msg;
        
        // Process TC (includes forwarding logic)
        process_tc_message(&msg, sender_id);
        printf("=== TC PROCESSING COMPLETE ===\n\n");
        return 0;
    }
    else{
        printf("Error: Unknown message type %d\n", msg_type);
        return -1;  // Unknown message type
    }
    
}

/**
 * @brief Update neighbor information from any received message
 * This provides passive link monitoring from all message types
 */
void update_neighbor_from_any_message(uint32_t sender_id, uint8_t msg_type) {
    // Find existing neighbor
    int found = 0;
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].neighbor_id == sender_id) {
            // Update existing neighbor
            neighbor_table[i].last_seen = time(NULL);
            
            char sender_str[16];
            unsigned char* bytes = (unsigned char*)&sender_id;
            snprintf(sender_str, 16, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
            printf("Updated neighbor %s from message type %d\n", sender_str, msg_type);
            found = 1;
            break;
        }
    }
    
    // If not found and it's not a control message, optionally add as new neighbor
    // (For now, we rely on HELLO messages for neighbor discovery)
    if (!found && msg_type != MSG_HELLO && msg_type != MSG_TC) {
        char sender_str[16];
        unsigned char* bytes = (unsigned char*)&sender_id;
        snprintf(sender_str, 16, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
        printf("Received data from unknown neighbor %s - waiting for HELLO\n", sender_str);
    }
}

/**
 * @brief Enhanced message processing for all message types
 * Handles control messages (HELLO/TC) and data messages with routing decisions
 */
void receive_message(void* message_ptr, uint8_t msg_type, uint32_t sender_id, 
                     uint32_t originator_id, uint32_t dest_id, uint16_t seq_num, 
                     uint8_t ttl, uint8_t hop_count) {
    
    printf("\n=== MESSAGE RECEIVED ===\n");
    printf("Type: %d, Sender: 0x%08X, Originator: 0x%08X, Dest: 0x%08X\n",
           msg_type, sender_id, originator_id, dest_id);
    printf("SeqNum: %d, TTL: %d, Hops: %d\n", seq_num, ttl, hop_count);
    
    // STEP 1: ALWAYS update neighbor table from sender (passive monitoring)
    // This works for ALL message types - important for robust routing!
    update_neighbor_from_any_message(sender_id, msg_type);
    
    // STEP 2: Process based on message type
    if (msg_type == MSG_HELLO || msg_type == MSG_TC) {
        // Control messages - use existing processing
        receive_control_message(message_ptr, msg_type, sender_id, originator_id, 
                              seq_num, ttl, hop_count);
    } else {
        // Data messages (MSG_DATA, MSG_VOICE, MSG_FILE, etc.)
        // Check routing decision
        uint32_t next_hop = 0;
        uint32_t metrics = 0;
        int hops = 0;
        int route_result = get_next_hop(dest_id, &next_hop, &metrics, &hops);
        
        if (dest_id == node_id || route_result == 1) {
            // Message is for us - deliver to application
            printf("✓ Message delivered to application (destination reached)\n");
            // In real implementation: deliver_to_application(message_ptr, msg_type);
            
        } else if (route_result == 0 && ttl > 0) {
            // Need to forward - route exists
            printf("→ Forwarding message to next hop: 0x%08X (TTL=%d)\n", 
                   next_hop, ttl - 1);
            // In real implementation: forward_data_message(message_ptr, msg_type, 
            //                        dest_id, next_hop, ttl - 1, hop_count + 1);
            
        } else if (route_result == -1) {
            // No route found
            printf("✗ No route to destination 0x%08X - dropping message\n", dest_id);
            // Could trigger emergency HELLO here if needed
            
        } else {
            // TTL expired
            printf("✗ TTL expired - dropping message\n");
        }
    }
    
    printf("=== MESSAGE PROCESSING COMPLETE ===\n\n");
}
void init_olsr(void){
    // Initialization code for OLSR protocol
    struct control_queue ctrl_queue;
    init_control_queue(&ctrl_queue);
    printf("OLSR Initialized with Link Failure Detection\n");
    struct control_message msg;
    
    time_t now = time(NULL);
    
    // Initialize timing variables
    time_t last_hello_time = now;  // Initialize to current time
    time_t last_tc_time = now;     // Initialize to current time  
    time_t last_timeout_check = now;
    time_t last_global_cleanup = now;
    int topology_changed = 0;
    
    printf("OLSR Global Routing Loop Started\n");
    
    // Send initial HELLO and TC messages immediately for network discovery
    printf("Sending initial HELLO message for network discovery...\n");
    send_hello_message(&ctrl_queue);
    
    printf("Sending initial TC message for topology advertisement...\n");
    send_tc_message(&ctrl_queue);
    
    while(1){
        now = time(NULL);
        
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
            printf("Type: %d, Message pointer: %p\n", msg.msg_type, msg.message_ptr);
            
            // SEND PATH: In a real implementation, this would send via MAC layer
            // The MAC layer would handle serialization of msg.message_ptr and transmit over the network
            
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
void simulate(){
    init_control_queue(&global_ctrl_queue);
    printf("Control queue initialized for testing\n");
    
    send_hello_message(&global_ctrl_queue);
    printf("HELLO message sent for testing\n");
    
    // Create a simple HELLO message structure for testing
    struct olsr_hello test_hello;
    memset(&test_hello, 0, sizeof(struct olsr_hello));
    test_hello.hello_interval = 2;  // 2 second interval
    test_hello.willingness = 3;  // WILL_DEFAULT
    test_hello.neighbor_count = 0;
    test_hello.neighbors = NULL;  // No neighbors in test message
    test_hello.two_hop_count = 0;
    test_hello.two_hop_neighbors = NULL;
    test_hello.reserved_slot = -1;
    
    receive_control_message((void*)&test_hello, MSG_HELLO, 0xC0A80001, 0xC0A80001, 1, 1, 0);
    printf("HELLO message received and processed for testing\n");
    
    send_tc_message(&global_ctrl_queue);
    printf("TC message sent for testing\n");
    
    // Create a simple TC message structure for testing
    struct olsr_tc test_tc;
    memset(&test_tc, 0, sizeof(struct olsr_tc));
    test_tc.ansn = 1;  // Advertised Neighbor Sequence Number
    test_tc.selector_count = 0;
    test_tc.mpr_selectors = NULL;  // No MPR selectors in test message
    
    receive_control_message((void*)&test_tc, MSG_TC, 0xC0A80001, 0xC0A80002, 1, 255, 1);
    printf("TC message received and processed for testing\n");
    
    print_routing_table();
    display_one_hop_neighbors();
    printf("Routing table printed for testing\n");
    
    printf("\n\n=== TESTING ENHANCED MESSAGE HANDLING ===\n");
    
    // Test 1: Receive a data message for this node (destination reached)
    printf("\n--- Test 1: Data message for this node ---\n");
    char test_data[] = "Hello World Data";
    receive_message((void*)test_data, 3, 0xC0A80001, 0xC0A80002, node_id, 100, 5, 2);
    
    // Test 2: Receive a data message for another node (needs forwarding)
    printf("\n--- Test 2: Data message needing forwarding ---\n");
    receive_message((void*)test_data, 3, 0xC0A80001, 0xC0A80002, 0xC0A80099, 101, 5, 2);
    
    // Test 3: Receive another HELLO to show neighbor update
    printf("\n--- Test 3: Another HELLO message (neighbor update) ---\n");
    receive_message((void*)&test_hello, MSG_HELLO, 0xC0A80001, 0xC0A80001, 0xFFFFFFFF, 2, 1, 0);
    
    // Test 4: Receive data from updated neighbor
    printf("\n--- Test 4: Data message from known neighbor ---\n");
    receive_message((void*)test_data, 3, 0xC0A80001, 0xC0A80001, node_id, 102, 5, 1);
    
    printf("\n=== ENHANCED MESSAGE HANDLING TEST COMPLETE ===\n");
}

int main() {
    printf("OLSR Starting...\n");
    // Initialization code here
    //init_olsr();
    simulate();
    return 0;
}