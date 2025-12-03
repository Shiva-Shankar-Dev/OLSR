#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "../include/olsr.h"
#include "../include/packet.h"
#include "../include/hello.h"
#include "../include/routing.h"
#include "../include/tc.h"
#include "../include/mpr.h"

// Global routing functions are in routing.c
// Forward declarations
int is_duplicate_message(uint32_t originator, uint16_t seq_number);
int add_duplicate_entry(uint32_t originator, uint16_t seq_number);
int should_forward_message(uint32_t sender_addr, uint32_t originator_addr);
int forward_tc_message(struct olsr_message* msg, uint32_t sender_addr, struct control_queue* queue);
int add_topology_link(uint32_t from_node, uint32_t to_node, uint16_t ansn, time_t validity_time);
extern struct control_queue global_ctrl_queue;

/**
 * @brief Convert a node ID to a string representation
 * @param id The node ID to convert
 * @param buffer Buffer to store the string representation (must be at least 16 bytes)
 * @return Pointer to the buffer
 */
static char* id_to_string(uint32_t id, char* buffer) {
    unsigned char* bytes = (unsigned char*)&id;
    snprintf(buffer, 16, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
    return buffer;
}

/** @brief Global ANSN (Advertised Neighbor Sequence Number) counter */
static uint16_t ansn_counter = 0;

/**
 * @brief Process a received TC message
 * 
 * Enhanced TC processing with duplicate detection, proper sequencing,
 * and message forwarding for global routing.
 * 
 * RECEIVE FLOW CONTEXT:
 * This function is called after the following steps:
 * 1. Raw bytes received from MAC layer
 * 2. TC message deserialized (bytes → struct olsr_tc)
 * 3. olsr_message wrapper created with body pointing to deserialized TC
 * 4. THIS function called to process the structured data
 * 
 * @param msg Pointer to received OLSR message containing TC
 *            msg->body must point to a deserialized struct olsr_tc
 * @param sender_addr IP address of message sender
 */
void process_tc_message(struct olsr_message* msg, uint32_t sender_addr) {
    if (!msg || msg->msg_type != MSG_TC) {
        printf("Error: Invalid TC message\n");
        return;
    }
    
    char orig_str[16], sender_str[16];
    printf("\n=== PROCESSING TC MESSAGE ===\n");
    printf("From: %s (via %s)\n", 
           id_to_string(msg->originator, orig_str),
           id_to_string(sender_addr, sender_str));
    printf("TTL: %d, Hops: %d, SeqNum: %d\n", msg->ttl, msg->hop_count, msg->msg_seq_num);
    
    // Step 1: Duplicate Detection
    if (is_duplicate_message(msg->originator, msg->msg_seq_num)) {
        printf("TC_PROCESS: Duplicate message - ignoring\n");
        printf("=== TC PROCESSING COMPLETE (DUPLICATE) ===\n\n");
        return;
    }
    
    // Step 2: Add to duplicate table
    add_duplicate_entry(msg->originator, msg->msg_seq_num);
    
    // Extract the deserialized TC message from the wrapper
    struct olsr_tc* tc = (struct olsr_tc*)msg->body;
    if (!tc) {
        printf("Error: Empty TC message body\n");
        return;
    }
    
    printf("TC Content: ANSN=%d, MPR Selectors=%d\n", tc->ansn, tc->selector_count);
    
    // Step 3: Process TC content - update global topology
    time_t validity = time(NULL) + msg->vtime;
    int topology_updated = 0;
    
    for (int i = 0; i < tc->selector_count; i++) {
        uint32_t selector = tc->mpr_selectors[i].neighbor_addr;
        
        // Add to global topology database
        if (add_topology_link(msg->originator, selector, tc->ansn, validity) == 0) {
            topology_updated = 1;
        }
        
        // Also update old routing topology (for compatibility)
        update_tc_topology(msg->originator, selector, validity);
    }
    
    // Step 4: Update routing table if topology changed
    if (topology_updated) {
        printf("TC_PROCESS: Topology updated - recalculating routes\n");
        update_routing_table();
    }
    
    // Step 5: Message Forwarding (MPR flooding)
    if (should_forward_message(sender_addr, msg->originator)) {
        printf("TC_PROCESS: This node selected as MPR - forwarding message\n");
        if (forward_tc_message(msg, sender_addr, &global_ctrl_queue) == 0) {
            printf("TC_PROCESS: Message queued for forwarding\n");
        } else {
            printf("TC_PROCESS: Failed to forward message\n");
        }
    }
    
    printf("=== TC PROCESSING COMPLETE ===\n\n");
}

// MPR selector management is now handled through neighbor_table[].is_mpr_selector flags

/**
 * @brief Generate a TC message
 * 
 * Creates a TC message structure containing MPR selector information.
 * NOTE: this implementation uses static storage for the returned message
 * and for the MPR selector list. The returned pointer points into static
 * buffers which are overwritten on each call and must NOT be freed by caller.
 *
 * @return Pointer to a statically allocated TC message (never NULL)
 * 
 * @note The returned pointer and MPR selector list have static lifetime
 *       (valid until the next call to this function). This is intentional
 *       to avoid dynamic allocation in this build. The implementation is
 *       NOT thread-safe: concurrent calls will overwrite the same buffers.
 */
struct olsr_tc* generate_tc_message(void) {
    static struct olsr_tc tc_msg;
    static struct tc_neighbor mpr_selectors_static[MAX_NEIGHBORS];
    
    // Clear the message
    memset(&tc_msg, 0, sizeof(struct olsr_tc));
    memset(mpr_selectors_static, 0, sizeof(mpr_selectors_static));
    
    // Count neighbors who selected us as MPR
    int selector_count = 0;
    for (int i = 0; i < neighbor_count && selector_count < MAX_NEIGHBORS; i++) {
        if (neighbor_table[i].link_status == SYM_LINK &&
            neighbor_table[i].is_mpr_selector) {
            mpr_selectors_static[selector_count].neighbor_addr = neighbor_table[i].neighbor_id;
            selector_count++;
            
            char selector_str[16];
            printf("  Including MPR selector: %s\n",
                   id_to_string(neighbor_table[i].neighbor_id, selector_str));
        }
    }
    
    tc_msg.ansn = ++ansn_counter;
    tc_msg.selector_count = selector_count;
    tc_msg.mpr_selectors = (selector_count > 0) ? mpr_selectors_static : NULL;
    
    printf("Generated TC message: ANSN=%d, MPR selectors=%d\n", 
           tc_msg.ansn, tc_msg.selector_count);
    
    return &tc_msg;
}

/**
 * @brief Send a TC message
 * 
 * Generates a TC message, wraps it in an OLSR message header,
 * and queues it for transmission. This function handles message creation,
 * sequence number assignment, and logging.
 * 
 * @param queue Pointer to the control queue for RRC/TDMA layer transmission
 * 
 * @note TC messages are only sent if there are MPR selectors to advertise
 */
void send_tc_message(struct control_queue* queue) {
    if (!queue) {
        printf("Error: Control queue is NULL\n");
        return;
    }
    
    // Count MPR selectors first
    int mpr_selector_count = get_mpr_selector_count();
    
    // Only send if we have MPR selectors
    if (mpr_selector_count == 0) {
        printf("No MPR selectors - skipping TC message\n");
        return;
    }
    
    struct olsr_tc* tc_msg = generate_tc_message();
    if (!tc_msg) {
        printf("Error: Failed to generate TC message\n");
        return;
    }

    uint8_t serialized_buf[1024];
    int serialized_size = serialize_tc(tc_msg, serialized_buf);
    if (serialized_size <= 0) {
        printf("Error: Failed to serialize TC message\n");
        return;
    }

    // Create proper OLSR message header with full sequencing
    struct olsr_message hdr;
    hdr.msg_type = MSG_TC;
    hdr.vtime = TC_VALIDITY_TIME;  // Use constant for consistency
    hdr.originator = node_id;
    hdr.ttl = 255;                 // Maximum TTL for network-wide flooding
    hdr.hop_count = 0;             // This is the originating node
    hdr.msg_seq_num = ++message_seq_num;
    hdr.body = tc_msg;
    hdr.msg_size = sizeof(struct olsr_message) + serialized_size;

    printf("\n=== GENERATING TC MESSAGE ===\n");
    printf("Originator: 0x%08X, SeqNum: %d, TTL: %d\n", hdr.originator, hdr.msg_seq_num, hdr.ttl);
    printf("ANSN: %d, MPR Selectors: %d, Validity: %ds\n", 
           tc_msg->ansn, tc_msg->selector_count, hdr.vtime);
    
    // Add to our own duplicate table to prevent processing our own message
    add_duplicate_entry(hdr.originator, hdr.msg_seq_num);

    // Push pointer to the TC structure directly to the queue
    // RRC/TDMA layer will handle serialization
    int result = push_to_control_queue(queue, MSG_TC, (void*)tc_msg);
    if (result == 0) {
        printf("TC Message successfully queued for RRC/TDMA Layer\n");
    } else {
        printf("ERROR: Failed to queue TC Message (code=%d)\n", result);
        // Note: tc_msg uses static storage, so no need to free memory
    }
}

// get_mpr_selector_count() is now implemented in hello.c

/**
 * @brief Deserialize a TC message from a buffer
 * 
 * RECEIVE FLOW - Step 1: Deserialize
 * Converts received bytes into a structured TC message for processing.
 * This is called before process_tc_message().
 * 
 * Flow: receive_bytes → [deserialize_tc()] → process_tc_message()
 * 
 * @param tc Pointer to TC message structure to fill
 * @param buffer Buffer containing serialized data
 * @return Number of bytes read, or -1 on error
 */
int deserialize_tc(struct olsr_tc* tc, const uint8_t* buffer) {
    if (!tc || !buffer) {
        return -1;
    }
    
    int offset = 0;
    
    // Deserialize ANSN
    memcpy(&tc->ansn, buffer + offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    
    // Deserialize selector count
    memcpy(&tc->selector_count, buffer + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    
    // Validate selector count
    if (tc->selector_count > MAX_NEIGHBORS) {
        printf("Error: Invalid selector count %d in TC message\n", tc->selector_count);
        return -1;
    }
    
    // Allocate static storage for MPR selectors (similar to generate_tc_message)
    static struct tc_neighbor mpr_selectors_static[MAX_NEIGHBORS];
    
    // Deserialize MPR selectors
    for (int i = 0; i < tc->selector_count; i++) {
        memcpy(&mpr_selectors_static[i], buffer + offset, sizeof(struct tc_neighbor));
        offset += sizeof(struct tc_neighbor);
    }
    
    tc->mpr_selectors = (tc->selector_count > 0) ? mpr_selectors_static : NULL;
    
    printf("Deserialized TC: ANSN=%d, selectors=%d, bytes=%d\n", 
           tc->ansn, tc->selector_count, offset);
    
    return offset;
}

/**
 * @brief Get current ANSN value
 */
uint16_t get_current_ansn(void) {
    return ansn_counter;
}
