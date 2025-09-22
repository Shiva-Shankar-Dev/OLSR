#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <pthread.h>
#include "../include/utils.h"
#include "../include/packet.h"
#include "../include/hello.h"

extern volatile int running;

/**
 * Create a UDP socket
 */
int create_udp_socket(void) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error creating UDP socket");
        return -1;
    }
    return sockfd;
}

/**
 * Setup socket for broadcasting
 */
int setup_broadcast_socket(int sockfd) {
    int broadcast_enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("Error enabling broadcast");
        return -1;
    }
    
    // Allow socket reuse
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("Error setting SO_REUSEADDR");
        return -1;
    }
    
    return 0;
}

/**
 * Create broadcast address structure
 */
struct sockaddr_in create_broadcast_address(int port) {
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(port);
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    
    return broadcast_addr;
}

/**
 * Get local IP address (first non-loopback interface)
 */
uint32_t get_local_ip(void) {
    struct ifaddrs *ifaddrs_ptr;
    struct ifaddrs *ifa;
    uint32_t local_ip = 0;
    
    if (getifaddrs(&ifaddrs_ptr) == -1) {
        perror("getifaddrs");
        return 0;
    }
    
    for (ifa = ifaddrs_ptr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in* sin = (struct sockaddr_in*)ifa->ifa_addr;
            
            // Skip loopback interface
            if (sin->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
                local_ip = sin->sin_addr.s_addr;
                printf("Using interface %s with IP %s\n", 
                       ifa->ifa_name, inet_ntoa(sin->sin_addr));
                break;
            }
        }
    }
    
    freeifaddrs(ifaddrs_ptr);
    
    if (local_ip == 0) {
        printf("Warning: Could not find local IP, using localhost\n");
        local_ip = htonl(INADDR_LOOPBACK);
    }
    
    return local_ip;
}

/**
 * Print IP address in readable format
 */
void print_ip(uint32_t ip) {
    struct in_addr addr;
    addr.s_addr = ip;
    printf("%s", inet_ntoa(addr));
}

/**
 * Sleep for specified number of seconds
 */
void sleep_seconds(int seconds) {
    sleep(seconds);
}

/**
 * Setup socket for receiving broadcasts
 */
int setup_receive_socket(int sockfd) {
    struct sockaddr_in server_addr;
    int reuseaddr = 1;
    
    // Allow reuse of address
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
        perror("Error setting SO_REUSEADDR");
        return -1;
    }
    
    // Bind to OLSR port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(OLSR_PORT);
    
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding receive socket");
        return -1;
    }
    
    printf("Listening for HELLO messages on port %d\n", OLSR_PORT);
    return 0;
}

/**
 * Deserialize received HELLO packet
 */
int deserialize_hello_packet(const char* buffer, int buffer_len, struct olsr_message* msg) {
    if (!buffer || !msg || buffer_len < (int)sizeof(struct olsr_message)) {
        return -1;
    }
    
    // Copy message header
    memcpy(msg, buffer, sizeof(struct olsr_message));
    
    // Convert from network byte order
    msg->msg_size = ntohs(msg->msg_size);
    msg->originator = ntohl(msg->originator);
    msg->msg_seq_num = ntohs(msg->msg_seq_num);
    
    return 0;
}

/**
 * Validate received OLSR message
 */
int validate_olsr_message(struct olsr_message* msg) {
    if (!msg) return 0;
    
    // Check message type
    if (msg->msg_type != MSG_HELLO && msg->msg_type != MSG_TC) {
        return 0;
    }
    
    // Check TTL
    if (msg->ttl == 0) {
        return 0;
    }
    
    // Check message size is reasonable
    if (msg->msg_size < sizeof(struct olsr_message) || msg->msg_size > 1500) {
        return 0;
    }
    
    return 1;
}

/**
 * HELLO message receiver thread
 */
void* hello_receiver_thread(void* arg) {
    int sockfd = *(int*)arg;
    char buffer[1024];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    
    printf("HELLO receiver thread started\n");
    
    while (running) {
        // Receive packet
        int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, 
                                     (struct sockaddr*)&sender_addr, &addr_len);
        
        if (bytes_received < 0) {
            if (running) {  // Only print error if we're still supposed to be running
                perror("Error receiving packet");
            }
            continue;
        }
        
        if (bytes_received < (int)sizeof(struct olsr_message)) {
            printf("Received packet too small (%d bytes)\n", bytes_received);
            continue;
        }
        
        // Parse message
        struct olsr_message msg;
        if (deserialize_hello_packet(buffer, bytes_received, &msg) < 0) {
            printf("Failed to deserialize packet\n");
            continue;
        }
        
        // Validate message
        if (!validate_olsr_message(&msg)) {
            printf("Invalid OLSR message received\n");
            continue;
        }
        
        // Process based on message type
        if (msg.msg_type == MSG_HELLO) {
            uint32_t sender_ip = sender_addr.sin_addr.s_addr;
            printf("📨 Received HELLO from ");
            print_ip(sender_ip);
            printf("\n");
            
            process_hello_message(&msg, sender_ip);
        }
    }
    
    printf("HELLO receiver thread stopping\n");
    return NULL;
}