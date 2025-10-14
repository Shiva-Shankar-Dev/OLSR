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
 * Updates topology information from received TC message and
 * triggers routing table recalculation if needed.
 * 
 * @param msg Pointer to received OLSR message containing TC
 * @param sender_addr IP address of message sender
 */
void process_tc_message(struct olsr_message* msg, uint32_t sender_addr) {
    (void)sender_addr; // Suppress unused parameter warning
    if (!msg || msg->msg_type != MSG_TC) {
        printf("Error: Invalid TC message\n");
        return;
    }
    
    struct olsr_tc* tc = (struct olsr_tc*)msg->body;
    if (!tc) {
        printf("Error: Empty TC message body\n");
        return;
    }
    
    char orig_str[16];
    printf("Processing TC from %s: ANSN=%d, selectors=%d\n",
           id_to_string(msg->originator, orig_str),
           tc->ansn, tc->selector_count);
    
    // Compute validity time (now + vtime)
    time_t validity = time(NULL) + msg->vtime;
    
    // Update topology information for each MPR selector
    for (int i = 0; i < tc->selector_count; i++) {
        uint32_t selector = tc->mpr_selectors[i].neighbor_addr;
        
        // Add topology link: originator -> selector
        update_tc_topology(msg->originator, selector, validity);
        
        char orig_str2[16], sel_str[16];
        printf("  Topology: %s -> %s (valid for %ds)\n",
               id_to_string(msg->originator, orig_str2),
               id_to_string(selector, sel_str),
               msg->vtime);
    }
    
    // Update routing table with new topology information
    update_routing_table();
}

// MPR selector management is now handled through neighbor_table[].is_mpr_selector flags

/**
 * @brief Generate a TC message
 * @return Newly allocated TC message, NULL on failure
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
 */
void send_tc_message(void) {
    // Count MPR selectors first
    int mpr_selector_count = get_mpr_selector_count();
    
    // Only send if we have MPR selectors
    if (mpr_selector_count == 0) {
        printf("No MPR selectors - skipping TC message\n");
        return;
    }
    
    struct olsr_tc* tc = generate_tc_message();
    if (!tc) return;
    
    // Create message header
    struct olsr_message msg;
    msg.msg_type = MSG_TC;
    msg.vtime = 15;           // Longer validity than HELLO
    msg.originator = node_id;
    msg.ttl = 255;           // Maximum TTL for TC
    msg.hop_count = 0;
    msg.msg_seq_num = ++message_seq_num;
    msg.body = tc;
    
    msg.msg_size = sizeof(struct olsr_message) +
                   sizeof(struct olsr_tc) +
                   (tc->selector_count * sizeof(struct tc_neighbor));
    
    // TC message debug output
    printf("TC message ready: ANSN=%d, size=%d, selectors=%d\n",
           tc->ansn, msg.msg_size, tc->selector_count);
    
    // No cleanup needed for static allocations
}

// get_mpr_selector_count() is now implemented in hello.c

/**
 * @brief Get current ANSN value
 */
uint16_t get_current_ansn(void) {
    return ansn_counter;
}
