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
 * @brief Serialize a TC message to a buffer
 * @param tc Pointer to TC message
 * @param buffer Buffer to write to
 * @return Number of bytes written, or -1 on error
 */
int serialize_tc(const struct olsr_tc* tc, uint8_t* buffer) {
    if (!tc || !buffer) {
        return -1;
    }
    
    int offset = 0;
    
    // Serialize ANSN
    memcpy(buffer + offset, &tc->ansn, sizeof(uint16_t)); 
    offset += sizeof(uint16_t);
    
    // Serialize selector count
    memcpy(buffer + offset, &tc->selector_count, sizeof(uint8_t)); 
    offset += sizeof(uint8_t);
    
    // Serialize MPR selectors
    for (int i = 0; i < tc->selector_count; i++) {
        memcpy(buffer + offset, &tc->mpr_selectors[i], sizeof(struct tc_neighbor));
        offset += sizeof(struct tc_neighbor);
    }
    
    return offset;
}

/**
 * @brief Push a TC message to the control queue
 * 
 * Serializes and adds a TC message to the specified control queue
 * for later processing by the MAC layer.
 * 
 * @param queue Pointer to the control queue where the message will be stored
 * @param serialized_buffer Serialized TC message data
 * @param serialized_size Size of serialized data
 * @return 0 on success, -1 on failure
 * 
 * @note The control queue takes ownership of the message data
 */
int push_tc_to_queue(struct control_queue* queue, const uint8_t* serialized_buffer, int serialized_size) {
    if (!queue) {
        printf("Error: Control queue is NULL\n");
        return -1;
    }

    if (serialized_size <= 0 || !serialized_buffer) {
        printf("Error: Invalid serialized TC buffer or size\n");
        return -1;
    }

    int result = push_to_control_queue(queue, MSG_TC, serialized_buffer, (size_t)serialized_size);

    if (result == 0) {
        printf("TC message serialized and queued (size=%d bytes)\n", serialized_size);
    } else {
        printf("Error: Failed to push TC into control queue (size=%d)\n", serialized_size);
    }

    return result;
}

/**
 * @brief Send a TC message
 * 
 * Generates a TC message, wraps it in an OLSR message header,
 * and queues it for transmission. This function handles message creation,
 * sequence number assignment, and logging.
 * 
 * @param queue Pointer to the control queue for MAC layer transmission
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

    // Update header / seq for logging
    struct olsr_message hdr;
    hdr.msg_type = MSG_TC;
    hdr.vtime = 15;           // Longer validity than HELLO
    hdr.originator = node_id;
    hdr.ttl = 255;            // Maximum TTL for TC
    hdr.hop_count = 0;
    hdr.msg_seq_num = ++message_seq_num;
    hdr.body = tc_msg;
    hdr.msg_size = sizeof(struct olsr_message) + serialized_size;

    printf("TC message prepared (type=%d, size=%d, seq=%d)\n", hdr.msg_type, hdr.msg_size, hdr.msg_seq_num);
    printf("ANSN: %d, MPR Selectors: %d\n", tc_msg->ansn, tc_msg->selector_count);

    int result = push_tc_to_queue(queue, serialized_buf, serialized_size);
    if (result == 0) {
        printf("TC Message successfully queued for MAC Layer\n");
    } else {
        printf("ERROR: Failed to queue TC Message (code=%d)\n", result);
    }
}

// get_mpr_selector_count() is now implemented in hello.c

/**
 * @brief Get current ANSN value
 */
uint16_t get_current_ansn(void) {
    return ansn_counter;
}
