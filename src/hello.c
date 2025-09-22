#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../include/hello.h"
#include "../include/olsr.h"
#include "../include/packet.h"

// Global variables (defined in olsr.h)
struct neighbor_entry neighbor_table[MAX_NEIGHBORS];
int neighbor_count = 0;
uint8_t node_willingness = WILL_DEFAULT;
uint32_t node_ip = 0;
uint16_t message_seq_num = 0;

/**
 * Generate a HELLO message containing current neighbors and willingness
 */
struct olsr_hello* generate_hello_message(void) {
    struct olsr_hello* hello_msg = malloc(sizeof(struct olsr_hello));
    if (!hello_msg) {
        printf("Error: Failed to allocate memory for HELLO message\n");
        return NULL;
    }
    
    // Set basic HELLO message fields
    hello_msg->hello_interval = HELLO_INTERVAL;
    hello_msg->willingness = node_willingness;
    hello_msg->neighbor_count = neighbor_count;
    
    // Allocate memory for neighbors list
    if (neighbor_count > 0) {
        hello_msg->neighbors = malloc(neighbor_count * sizeof(struct hello_neighbor));
        if (!hello_msg->neighbors) {
            printf("Error: Failed to allocate memory for neighbors list\n");
            free(hello_msg);
            return NULL;
        }
        
        // Copy neighbor information to HELLO message
        for (int i = 0; i < neighbor_count; i++) {
            hello_msg->neighbors[i].neighbor_addr = neighbor_table[i].addr;
            hello_msg->neighbors[i].link_code = neighbor_table[i].link_code;
        }
    } else {
        hello_msg->neighbors = NULL;
    }
    
    printf("Generated HELLO message: willingness=%d, neighbors=%d\n", 
           hello_msg->willingness, hello_msg->neighbor_count);
    
    return hello_msg;
}

/**
 * Send HELLO message via UDP broadcast
 */
void send_hello_message(int sockfd, struct sockaddr_in* broadcast_addr) {
    // Generate HELLO message
    struct olsr_hello* hello_msg = generate_hello_message();
    if (!hello_msg) {
        printf("Error: Failed to generate HELLO message\n");
        return;
    }
    
    // Create OLSR message wrapper
    struct olsr_message msg;
    msg.msg_type = MSG_HELLO;
    msg.vtime = 6; // Validity time (encoded)
    msg.originator = node_ip;
    msg.ttl = 1; // HELLO messages are only sent to 1-hop neighbors
    msg.hop_count = 0;
    msg.msg_seq_num = ++message_seq_num;
    msg.body = hello_msg;
    
    // Calculate message size
    msg.msg_size = sizeof(struct olsr_message) + sizeof(struct olsr_hello) + 
                   (hello_msg->neighbor_count * sizeof(struct hello_neighbor));
    
    // Serialize and send the packet
    char buffer[1024];
    int packet_size = serialize_hello_packet(&msg, buffer, sizeof(buffer));
    
    if (packet_size > 0) {
        ssize_t sent = sendto(sockfd, buffer, packet_size, 0, 
                             (struct sockaddr*)broadcast_addr, sizeof(*broadcast_addr));
        if (sent < 0) {
            perror("Error sending HELLO message");
        } else {
            printf("HELLO message sent (%zd bytes) to broadcast address\n", sent);
        }
    }
    
    // Cleanup
    if (hello_msg->neighbors) {
        free(hello_msg->neighbors);
    }
    free(hello_msg);
}

/**
 * Process received HELLO message
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
    
    // Update or add sender to neighbor table
    update_neighbor(sender_addr, SYM_LINK, hello_msg->willingness);
    
    // Check if we are mentioned in the sender's neighbor list
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
 * Add a new neighbor to the neighbor table
 */
int add_neighbor(uint32_t addr, uint8_t link_code, uint8_t willingness) {
    if (neighbor_count >= MAX_NEIGHBORS) {
        printf("Error: Neighbor table is full\n");
        return -1;
    }
    
    // Check if neighbor already exists
    if (find_neighbor(addr) != NULL) {
        return update_neighbor(addr, link_code, willingness);
    }
    
    // Add new neighbor
    neighbor_table[neighbor_count].addr = addr;
    neighbor_table[neighbor_count].link_code = link_code;
    neighbor_table[neighbor_count].willingness = willingness;
    neighbor_table[neighbor_count].last_heard = time(NULL);
    neighbor_table[neighbor_count].is_mpr = 0;
    neighbor_table[neighbor_count].is_mpr_selector = 0;
    
    neighbor_count++;
    
    printf("Added neighbor %s (willingness=%d, link=%d)\n",
           inet_ntoa(*(struct in_addr*)&addr), willingness, link_code);
    
    return 0;
}

/**
 * Update an existing neighbor in the neighbor table
 */
int update_neighbor(uint32_t addr, uint8_t link_code, uint8_t willingness) {
    struct neighbor_entry* neighbor = find_neighbor(addr);
    
    if (neighbor == NULL) {
        return add_neighbor(addr, link_code, willingness);
    }
    
    neighbor->link_code = link_code;
    neighbor->willingness = willingness;
    neighbor->last_heard = time(NULL);
    
    printf("Updated neighbor %s (willingness=%d, link=%d)\n",
           inet_ntoa(*(struct in_addr*)&addr), willingness, link_code);
    
    return 0;
}

/**
 * Find a neighbor in the neighbor table
 */
struct neighbor_entry* find_neighbor(uint32_t addr) {
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].addr == addr) {
            return &neighbor_table[i];
        }
    }
    return NULL;
}

/**
 * Print the current neighbor table
 */
void print_neighbor_table(void) {
    printf("\n--- Neighbor Table ---\n");
    printf("Count: %d\n", neighbor_count);
    
    for (int i = 0; i < neighbor_count; i++) {
        printf("%d. %s - Link:%d, Will:%d, MPR:%d, Last heard: %ld\n",
               i + 1,
               inet_ntoa(*(struct in_addr*)&neighbor_table[i].addr),
               neighbor_table[i].link_code,
               neighbor_table[i].willingness,
               neighbor_table[i].is_mpr,
               neighbor_table[i].last_heard);
    }
    printf("----------------------\n\n");
}

/**
 * Serialize HELLO packet for transmission
 */
int serialize_hello_packet(struct olsr_message* msg, char* buffer, int buffer_size) {
    if (!msg || !buffer || buffer_size < (int)sizeof(struct olsr_message)) {
        return -1;
    }
    
    int offset = 0;
    
    // Copy message header
    memcpy(buffer + offset, msg, sizeof(struct olsr_message));
    offset += sizeof(struct olsr_message);
    
    // Copy HELLO message body
    struct olsr_hello* hello_msg = (struct olsr_hello*)msg->body;
    memcpy(buffer + offset, hello_msg, sizeof(struct olsr_hello));
    offset += sizeof(struct olsr_hello);
    
    // Copy neighbors list
    if (hello_msg->neighbor_count > 0 && hello_msg->neighbors) {
        int neighbors_size = hello_msg->neighbor_count * sizeof(struct hello_neighbor);
        if (offset + neighbors_size <= buffer_size) {
            memcpy(buffer + offset, hello_msg->neighbors, neighbors_size);
            offset += neighbors_size;
        } else {
            printf("Error: Buffer too small for neighbors list\n");
            return -1;
        }
    }
    
    return offset;
}