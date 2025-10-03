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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <unistd.h>
    #include <arpa/inet.h>
#endif
#include "../include/hello.h"
#include "../include/olsr.h"
#include "../include/packet.h"

/** @brief Global neighbor table array */
struct neighbor_entry neighbor_table[MAX_NEIGHBORS];
/** @brief Current number of neighbors in the table */
int neighbor_count = 0;
/** @brief This node's willingness to act as MPR */
uint8_t node_willingness = WILL_DEFAULT;
/** @brief This node's IP address */
uint32_t node_ip = 0;
/** @brief Global message sequence number counter */
uint16_t message_seq_num = 0;

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
    struct olsr_hello* hello_msg = malloc(sizeof(struct olsr_hello));
    if (!hello_msg) {
        printf("Error: Failed to allocate memory for HELLO message\n");
        return NULL;
    }
    
    hello_msg->hello_interval = HELLO_INTERVAL;
    hello_msg->willingness = node_willingness;
    hello_msg->neighbor_count = neighbor_count;

    if (neighbor_count > 0) {
        hello_msg->neighbors = malloc(neighbor_count * sizeof(struct hello_neighbor));
        if (!hello_msg->neighbors) {
            printf("Error: Failed to allocate memory for neighbors list\n");
            free(hello_msg);
            return NULL;
        }
        
        for (int i = 0; i < neighbor_count; i++) {
            hello_msg->neighbors[i].neighbor_addr = neighbor_table[i].neighbor_addr;
            hello_msg->neighbors[i].link_code = neighbor_table[i].link_status;
        }
    } else {
        hello_msg->neighbors = NULL;
    }
    
    printf("Generated HELLO message: willingness=%d, neighbors=%d\n", 
           hello_msg->willingness, hello_msg->neighbor_count);
    
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
void send_hello_message(void) {
    // Generate HELLO message
    struct olsr_hello* hello_msg = generate_hello_message();
    if (!hello_msg) {
        printf("Error: Failed to generate HELLO message\n");
        return;
    }
    
    // Create OLSR message header
    struct olsr_message msg;
    msg.msg_type = MSG_HELLO;      /**< Set message type to HELLO */
    msg.vtime = 6;                 /**< Validity time (encoded) */
    msg.originator = node_ip;      /**< Set originator to this node's IP */
    msg.ttl = 1;                   /**< TTL = 1 for HELLO (one-hop only) */
    msg.hop_count = 0;             /**< Initial hop count */
    msg.msg_seq_num = ++message_seq_num; /**< Increment and assign sequence number */
    msg.body = hello_msg;          /**< Attach HELLO message body */
    
    // Calculate total message size
    msg.msg_size = sizeof(struct olsr_message) + sizeof(struct olsr_hello) + 
                   (hello_msg->neighbor_count * sizeof(struct hello_neighbor));
    
    // Simulate sending the HELLO message (no actual network transmission)
    printf("HELLO message sent (type=%d, size=%d bytes, seq=%d)\n", 
           msg.msg_type, msg.msg_size, msg.msg_seq_num);
    printf("Willingness: %d, Neighbors: %d\n", 
           hello_msg->willingness, hello_msg->neighbor_count);
    
    // Cleanup allocated memory
    if (hello_msg->neighbors) {
        free(hello_msg->neighbors);
    }
    free(hello_msg);
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
    
    printf("Received HELLO from %s: willingness=%d, neighbors=%d\n",
           inet_ntoa(*(struct in_addr*)&sender_addr),
           hello_msg->willingness, hello_msg->neighbor_count);
    
    // Update neighbor table with sender information
    update_neighbor(sender_addr, SYM_LINK, hello_msg->willingness);
    
    // Check if we are mentioned in the sender's neighbor list (bidirectional link)
    int we_are_mentioned = 0;
    for (int i = 0; i < hello_msg->neighbor_count; i++) {
        if (hello_msg->neighbors[i].neighbor_addr == node_ip) {
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
    int result = push_to_control_queue(queue, MSG_HELLO, hello_msg, sizeof(struct olsr_hello));
    
    printf("HELLO message created and queued (willingness=%d, neighbors=%d)\n", 
           hello_msg->willingness, hello_msg->neighbor_count);
    
    return result;
}

/**
 * @brief Update neighbor information in the neighbor table
 * 
 * Updates or adds a neighbor entry in the neighbor table with the
 * provided link status and willingness information.
 * 
 * @param addr IP address of the neighbor
 * @param link_code Link status code (SYM_LINK, ASYM_LINK, etc.)
 * @param willingness Neighbor's willingness to act as MPR
 * @return 0 on success, -1 on failure
 */
int update_neighbor(uint32_t addr, uint8_t link_code, uint8_t willingness) {
    // Look for existing neighbor entry
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].neighbor_addr == addr) {
            // Update existing entry
            neighbor_table[i].link_status = link_code;
            neighbor_table[i].willingness = willingness;
            neighbor_table[i].last_seen = time(NULL);
            printf("Updated neighbor %s: link=%d, willingness=%d\n",
                   inet_ntoa(*(struct in_addr*)&addr), link_code, willingness);
            return 0;
        }
    }
    
    // Add new neighbor entry if space available
    if (neighbor_count < MAX_NEIGHBORS) {
        neighbor_table[neighbor_count].neighbor_addr = addr;
        neighbor_table[neighbor_count].link_status = link_code;
        neighbor_table[neighbor_count].willingness = willingness;
        neighbor_table[neighbor_count].last_seen = time(NULL);
        neighbor_table[neighbor_count].is_mpr = 0;
        neighbor_table[neighbor_count].is_mpr_selector = 0;
        neighbor_table[neighbor_count].next = NULL;
        neighbor_count++;
        printf("Added new neighbor %s: link=%d, willingness=%d\n",
               inet_ntoa(*(struct in_addr*)&addr), link_code, willingness);
        return 0;
    }
    
    printf("Error: Neighbor table is full, cannot add %s\n",
           inet_ntoa(*(struct in_addr*)&addr));
    return -1;
}

/**
 * @brief Initialize the control queue
 * 
 * Initializes a control queue structure by setting all counters to zero.
 * 
 * @param queue Pointer to the control queue to initialize
 */
void init_control_queue(struct control_queue* queue) {
    if (!queue) {
        printf("Error: Cannot initialize NULL control queue\n");
        return;
    }
    
    queue->front = 0;
    queue->rear = -1;
    queue->count = 0;
    
    printf("Control queue initialized\n");
}

/**
 * @brief Push a message to the control queue
 * 
 * Adds a new message to the rear of the control queue using circular
 * buffer implementation.
 * 
 * @param queue Pointer to the control queue
 * @param msg_type Type of the message (MSG_HELLO, MSG_TC, etc.)
 * @param msg_data Pointer to message data
 * @param data_size Size of the message data in bytes
 * @return 0 on success, -1 on failure
 */
int push_to_control_queue(struct control_queue* queue, uint8_t msg_type, void* msg_data, int data_size) {
    if (!queue) {
        printf("Error: Control queue is NULL\n");
        return -1;
    }
    
    if (queue->count >= MAX_QUEUE_SIZE) {
        printf("Error: Control queue is full\n");
        return -1;
    }
    
    // Calculate next rear position (circular buffer)
    queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
    
    // Add message to queue
    queue->messages[queue->rear].msg_type = msg_type;
    queue->messages[queue->rear].timestamp = (uint32_t)time(NULL);
    queue->messages[queue->rear].msg_data = msg_data;
    queue->messages[queue->rear].data_size = data_size;
    
    queue->count++;
    
    printf("Message queued: type=%d, size=%d bytes, queue_count=%d\n", 
           msg_type, data_size, queue->count);
    
    return 0;
}

/**
 * @brief Pop a message from the control queue
 * 
 * Removes and returns the message at the front of the control queue.
 * 
 * @param queue Pointer to the control queue
 * @return Pointer to control message, or NULL if queue is empty
 */
struct control_message* pop_from_control_queue(struct control_queue* queue) {
    if (!queue || queue->count == 0) {
        return NULL;
    }
    
    struct control_message* msg = &queue->messages[queue->front];
    
    // Move front pointer (circular buffer)
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
    queue->count--;
    
    printf("Message dequeued: type=%d, remaining_count=%d\n", 
           msg->msg_type, queue->count);
    
    return msg;
}

/**
 * @brief Find a neighbor in the neighbor table
 * 
 * Searches the neighbor table for a specific neighbor by IP address.
 * 
 * @param addr IP address of the neighbor to find
 * @return Pointer to neighbor entry if found, NULL otherwise
 */
struct neighbor_entry* find_neighbor(uint32_t addr) {
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].neighbor_addr == addr) {
            return &neighbor_table[i];
        }
    }
    return NULL;
}

/**
 * @brief Print the current neighbor table
 * 
 * Displays the contents of the neighbor table in a human-readable format,
 * showing neighbor addresses, willingness values, and link status.
 */
void print_neighbor_table(void) {
    printf("\n=== NEIGHBOR TABLE ===\n");
    printf("Count: %d/%d\n", neighbor_count, MAX_NEIGHBORS);
    
    if (neighbor_count == 0) {
        printf("No neighbors found.\n");
        return;
    }
    
    printf("%-15s %-10s %-10s %-8s %-8s\n", 
           "IP Address", "Link", "Will", "MPR", "MPR_Sel");
    printf("----------------------------------------------------------\n");
    
    for (int i = 0; i < neighbor_count; i++) {
        char ip_str[16];
        strcpy(ip_str, inet_ntoa(*(struct in_addr*)&neighbor_table[i].neighbor_addr));
        
        const char* link_status;
        switch (neighbor_table[i].link_status) {
            case SYM_LINK: link_status = "SYM"; break;
            case ASYM_LINK: link_status = "ASYM"; break;
            case LOST_LINK: link_status = "LOST"; break;
            default: link_status = "UNSPEC"; break;
        }
        
        printf("%-15s %-10s %-10d %-8s %-8s\n",
               ip_str,
               link_status,
               neighbor_table[i].willingness,
               neighbor_table[i].is_mpr ? "YES" : "NO",
               neighbor_table[i].is_mpr_selector ? "YES" : "NO");
    }
    printf("======================\n\n");
}
