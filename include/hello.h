
#ifndef HELLO_H
#define HELLO_H

#include "olsr.h"
#include "packet.h"
#include <sys/socket.h>
#include <netinet/in.h>

// Function declarations for HELLO message handling
struct olsr_hello* generate_hello_message(void);
void send_hello_message(int sockfd, struct sockaddr_in* broadcast_addr);
void process_hello_message(struct olsr_message* msg, uint32_t sender_addr);

// Neighbor table management functions
int add_neighbor(uint32_t addr, uint8_t link_code, uint8_t willingness);
int update_neighbor(uint32_t addr, uint8_t link_code, uint8_t willingness);
struct neighbor_entry* find_neighbor(uint32_t addr);
void print_neighbor_table(void);

#endif
