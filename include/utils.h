#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

// Network utility functions
uint32_t get_local_ip(void);
void print_ip(uint32_t ip);
int create_udp_socket(void);
int setup_broadcast_socket(int sockfd);
int setup_receive_socket(int sockfd);
struct sockaddr_in create_broadcast_address(int port);

// Threading utilities  
void* hello_receiver_thread(void* arg);
void sleep_seconds(int seconds);

// OLSR protocol functions
int deserialize_hello_packet(const char* buffer, int buffer_len, struct olsr_message* msg);
int validate_olsr_message(struct olsr_message* msg);

#endif