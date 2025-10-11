/**
 * @file hello.c
 * @brief HELLO message implementation for OLSR protocol
 * @author OLSR Implementation Team
 * @date 2025-09-23
 * 
 * This file implements HELLO message creation, processing, and neighbor
 * table management functions for the OLSR protocol. It handles neighbor
 * discovery and maintains link state information.
 */

#include <stdio.h>
/**
 * @brief Stub implementation for print_neighbor_table
 * Prints a placeholder message. Replace with actual neighbor table printing logic as needed.
 */
void print_neighbor_table(void) {
    printf("[print_neighbor_table] Neighbor table printing not yet implemented.\n");
}
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/hello.h"
#include "../include/olsr.h"
#include "../include/packet.h"

/**
 * @brief Convert a node ID to a string representation
 * @param id The node ID to convert
 * @param buffer Buffer to store the string representation (must be at least 16 bytes)
 * @return Pointer to the buffer
 */
static char *id_to_string(uint32_t id, char* buffer) {
    unsigned char* bytes = (unsigned char*)&id;
    snprintf(buffer, 16, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
    return buffer;
}

/** @brief Global neighbor table array */
struct neighbor_entry neighbor_table[MAX_NEIGHBORS];
/** @brief Current number of neighbors in the table */
int neighbor_count = 0;
/** @brief This node's willingness to act as MPR */
uint8_t node_willingness = WILL_DEFAULT;
/** @brief This node's IP address */
uint32_t node_id = 0;
/** @brief Global message sequence number counter */
uint16_t message_seq_num = 0;

/**
 * @brief This node's reserved TDMA slot (-1 means no reservation)
 */
int my_reserved_slot = -1;

/**
 * @brief Set this node's TDMA slot reservation
 * @param slot Slot number to reserve (>=0), -1 to clear
 */
void set_my_slot_reservation(int slot) {
    my_reserved_slot = slot;
}

/**
 * @brief Clear this node's TDMA slot reservation
 */
void clear_my_slot_reservation(void) {
    my_reserved_slot = -1;
}

/**
 * @brief Get this node's current reserved slot
 * @return Reserved slot number, or -1 if none
 */
int get_my_reserved_slot(void) {
    return my_reserved_slot;
}
/**
 * @brief Generate a HELLO message
 * 
 * Creates a new HELLO message structure containing the node's current
 * willingness value and neighbor information. The message is used for
 * neighbor discovery and link sensing in the OLSR protocol.
 * 
 * @return Pointer to newly allocated HELLO message, or NULL on failure
 * 
 * @note The caller is responsible for freeing the returned message
 * @note If neighbor_count > 0, neighbor information is not included
 *       in this simplified implementation
 */
struct olsr_hello* generate_hello_message(void) {
    static struct olsr_hello hello_msg_static;
    struct olsr_hello* hello_msg = &hello_msg_static;
    memset(hello_msg, 0, sizeof(struct olsr_hello));
    if (!hello_msg) {
    printf("Error: Failed to allocate memory for HELLO message\n");
        return NULL;
    }
    
    hello_msg->hello_interval = HELLO_INTERVAL;
    hello_msg->willingness = node_willingness;
    hello_msg->neighbor_count = neighbor_count;
    hello_msg->reserved_slot = my_reserved_slot; // TDMA slot reservation

    if (neighbor_count > 0) {
        static struct hello_neighbor neighbors_static[MAX_NEIGHBORS];
        hello_msg->neighbors = neighbors_static;
        memset(hello_msg->neighbors, 0, neighbor_count * sizeof(struct hello_neighbor));
        
        // Allocate and fill neighbor information
        if (!hello_msg->neighbors) {
            printf("Error: Failed to allocate memory for neighbors list\n");
            return NULL;
        }
        
        for (int i = 0; i < neighbor_count; i++) {
            hello_msg->neighbors[i].neighbor_id = neighbor_table[i].neighbor_id;
            hello_msg->neighbors[i].link_code = neighbor_table[i].link_status;
        }
    } else {
        hello_msg->neighbors = NULL;
    }
    
    if (hello_msg->reserved_slot == -1)
        printf("Generated HELLO: willingness=%d, neighbors=%d, no slot reserved\n", hello_msg->willingness, hello_msg->neighbor_count);
    else
        printf("Generated HELLO: willingness=%d, neighbors=%d, reserved_slot=%d\n", hello_msg->willingness, hello_msg->neighbor_count, hello_msg->reserved_slot);
    
    return hello_msg;
}

/**
 * @brief Send a HELLO message
 * 
 * Generates a HELLO message, wraps it in an OLSR message header,
 * and simulates transmission. This function handles message creation,
 * sequence number assignment, and logging.
 * 
 * @note In this implementation, no actual network transmission occurs.
 *       The function demonstrates message creation and logging only.
 */
void send_hello_message(struct control_queue* queue) {

    struct olsr_hello* hello_msg = generate_hello_message();
    if (!hello_msg) {
        printf("Error: Failed to generate HELLO message\n");
        return;
    }
    
    // Create OLSR message header
    struct olsr_message msg;
    msg.msg_type = MSG_HELLO;      /**< Set message type to HELLO */
    msg.vtime = 6;                 /**< Validity time (encoded) */
    msg.originator = node_id;      /**< Set originator to this node's IP */
    msg.ttl = 1;                   /**< TTL = 1 for HELLO (one-hop only) */
    msg.hop_count = 0;             /**< Initial hop count */
    msg.msg_seq_num = ++message_seq_num; /**< Increment and assign sequence number */
    msg.body = hello_msg;          /**< Attach HELLO message body */
    
    // Calculate total message size
    msg.msg_size = sizeof(struct olsr_message) + sizeof(struct olsr_hello) + 
                   (hello_msg->neighbor_count * sizeof(struct hello_neighbor));

    // Debugging output
    printf("HELLO message sent (type=%d, size=%d bytes, seq=%d)\n", msg.msg_type, msg.msg_size, msg.msg_seq_num);
    printf("Willingness: %d, Neighbors: %d\n", hello_msg->willingness, hello_msg->neighbor_count);

    if(!queue){
    printf("Error: Control queue is NULL\n");
      return;
    }
    if(push_hello_to_queue(queue)==0){
    printf("HELLO Message successfully queued for MAC Layer\n");
    }
    else{
    printf("ERROR: Failed to queue HELLO Message\n");
    }
}

/**
 * @brief Process a received HELLO message
 * 
 * Processes an incoming HELLO message to update neighbor information
 * and determine link symmetry. The function checks if this node is
 * mentioned in the sender's neighbor list to establish bidirectional links.
 * 
 * @param msg Pointer to the OLSR message containing the HELLO
 * @param sender_addr IP address of the message sender
 * 
 * @note This function updates the neighbor table and establishes link symmetry
 */
void process_hello_message(struct olsr_message* msg, uint32_t sender_addr) {
    if (msg->msg_type != MSG_HELLO) {
    printf("Error: Not a HELLO message\n");
        return;
    }
    
    struct olsr_hello* hello_msg = (struct olsr_hello*)msg->body;
    
    char sender_str[16];
    if (hello_msg->reserved_slot == -1) {
     printf("Received HELLO from %s: willingness=%d, neighbors=%d, no slot reserved\n", id_to_string(sender_addr, sender_str), hello_msg->willingness, hello_msg->neighbor_count);
     printf("  -> Node %u has no slot reservation\n", sender_addr);
    } else {
     printf("Received HELLO from %s: willingness=%d, neighbors=%d, reserved_slot=%d\n", id_to_string(sender_addr, sender_str), hello_msg->willingness, hello_msg->neighbor_count, hello_msg->reserved_slot);
     printf("  -> Node %u is using slot %d\n", sender_addr, hello_msg->reserved_slot);
    }
    
    // Check if we are mentioned in the sender's neighbor list (bidirectional link)
    int we_are_mentioned = 0;
    for (int i = 0; i < hello_msg->neighbor_count; i++) {
        if (hello_msg->neighbors[i].neighbor_id == node_id) {
            we_are_mentioned = 1;
            printf("We are mentioned in neighbor's HELLO message\n");
            break;
        }
    }
    
    // Update link status based on bidirectional communication
    if (we_are_mentioned) {
        update_neighbor(sender_addr, SYM_LINK, hello_msg->willingness);
    } else {
        update_neighbor(sender_addr, ASYM_LINK, hello_msg->willingness);
    }
}

/**
 * @brief Push a HELLO message to the control queue
 * 
 * Creates a new HELLO message and adds it to the specified control queue
 * for later processing. This function combines message generation with
 * queue management.
 * 
 * @param queue Pointer to the control queue where the message will be stored
 * @return 0 on success, -1 on failure
 * 
 * @note The control queue takes ownership of the message data
 */
int push_hello_to_queue(struct control_queue* queue) {
    if (!queue) {
    printf("Error: Control queue is NULL\n");
        return -1;
    }

    // Generate HELLO message
    struct olsr_hello* hello_msg = generate_hello_message();
    if (!hello_msg) {
    printf("Error: Failed to generate HELLO message\n");
        return -1;
    }

    // Push to control queue
    int result = push_to_control_queue(queue, MSG_HELLO, (const uint8_t*)hello_msg, sizeof(struct olsr_hello));
    if (hello_msg->reserved_slot == -1)
        printf("HELLO message created and queued (willingness=%d, neighbors=%d)\n", hello_msg->willingness, hello_msg->neighbor_count);
    else
        printf("HELLO message created and queued (willingness=%d, slot reserved=%d, neighbors=%d)\n", hello_msg->willingness, hello_msg->reserved_slot, hello_msg->neighbor_count);
    return result;
}

/**
 * @brief Serialize a HELLO message to a buffer
 * @param hello Pointer to HELLO message
 * @param buffer Buffer to write to
 * @return Number of bytes written
 */
int serialize_hello(const struct olsr_hello* hello, uint8_t* buffer) {
    int offset = 0;
    memcpy(buffer + offset, &hello->hello_interval, sizeof(uint16_t)); offset += sizeof(uint16_t);
    memcpy(buffer + offset, &hello->willingness, sizeof(uint8_t)); offset += sizeof(uint8_t);
    memcpy(buffer + offset, &hello->neighbor_count, sizeof(uint8_t)); offset += sizeof(uint8_t);
    // Serialize reserved_slot (4 bytes)
    memcpy(buffer + offset, &hello->reserved_slot, sizeof(int)); offset += sizeof(int);
    // Serialize neighbors
    for (int i = 0; i < hello->neighbor_count; i++) {
        memcpy(buffer + offset, &hello->neighbors[i], sizeof(struct hello_neighbor));
        offset += sizeof(struct hello_neighbor);
    }
    return offset;
}

/**
 * @brief Deserialize a HELLO message from a buffer
 * @param hello Pointer to HELLO message to fill
 * @param buffer Buffer to read from
 * @return Number of bytes read
 */
int deserialize_hello(struct olsr_hello* hello, const uint8_t* buffer) {
    int offset = 0;
    memcpy(&hello->hello_interval, buffer + offset, sizeof(uint16_t)); offset += sizeof(uint16_t);
    memcpy(&hello->willingness, buffer + offset, sizeof(uint8_t)); offset += sizeof(uint8_t);
    memcpy(&hello->neighbor_count, buffer + offset, sizeof(uint8_t)); offset += sizeof(uint8_t);
    // Deserialize reserved_slot (4 bytes)
    memcpy(&hello->reserved_slot, buffer + offset, sizeof(int)); offset += sizeof(int);
    // Deserialize neighbors
    if (hello->neighbor_count > 0) {
        static struct hello_neighbor neighbors_static[MAX_NEIGHBORS];
        hello->neighbors = neighbors_static;
        for (int i = 0; i < hello->neighbor_count; i++) {
            memcpy(&hello->neighbors[i], buffer + offset, sizeof(struct hello_neighbor));
            offset += sizeof(struct hello_neighbor);
        }
    } else {
        hello->neighbors = NULL;
    }
    return offset;
}
