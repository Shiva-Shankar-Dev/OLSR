#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "../include/olsr.h"
#include "../include/hello.h"
#include "../include/utils.h"

// Global flag for graceful shutdown
volatile int running = 1;

/**
 * Signal handler for graceful shutdown
 */
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        printf("\nReceived shutdown signal. Cleaning up...\n");
        running = 0;
    }
}

/**
 * Initialize OLSR node
 */
int initialize_olsr_node(void) {
    // Get local IP address
    node_ip = get_local_ip();
    if (node_ip == 0) {
        printf("Error: Could not determine local IP address\n");
        return -1;
    }
    
    printf("OLSR Node initialized with IP: ");
    print_ip(node_ip);
    printf("\n");
    printf("Willingness: %d\n", node_willingness);
    printf("HELLO interval: %d seconds\n", HELLO_INTERVAL);
    
    return 0;
}

/**
 * Main OLSR HELLO sender loop
 */
int main(void) {
    printf("=== OLSR HELLO Message Demo ===\n");
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize OLSR node
    if (initialize_olsr_node() < 0) {
        return EXIT_FAILURE;
    }
    
    // Create UDP socket for sending
    int send_sockfd = create_udp_socket();
    if (send_sockfd < 0) {
        return EXIT_FAILURE;
    }
    
    // Create UDP socket for receiving
    int recv_sockfd = create_udp_socket();
    if (recv_sockfd < 0) {
        close(send_sockfd);
        return EXIT_FAILURE;
    }

    // Setup broadcasting for sender
    if (setup_broadcast_socket(send_sockfd) < 0) {
        close(send_sockfd);
        close(recv_sockfd);
        return EXIT_FAILURE;
    }
    
    // Setup receiving for listener
    if (setup_receive_socket(recv_sockfd) < 0) {
        close(send_sockfd);
        close(recv_sockfd);
        return EXIT_FAILURE;
    }
    
    // Create broadcast address
    struct sockaddr_in broadcast_addr = create_broadcast_address(OLSR_PORT);
    
    // Start receiver thread
    pthread_t receiver_thread;
    if (pthread_create(&receiver_thread, NULL, hello_receiver_thread, &recv_sockfd) != 0) {
        perror("Error creating receiver thread");
        close(send_sockfd);
        close(recv_sockfd);
        return EXIT_FAILURE;
    }
    
    printf("Starting HELLO message broadcasting...\n");
    printf("Press Ctrl+C to stop\n\n");
    
    int hello_counter = 0;
    
    // Main loop - send HELLO messages periodically
    while (running) {
        hello_counter++;
        
        printf("--- HELLO #%d ---\n", hello_counter);
        
        // Send HELLO message
        send_hello_message(send_sockfd, &broadcast_addr);
        
        // Print current neighbor table
        if (neighbor_count > 0) {
            print_neighbor_table();
        } else {
            printf("No neighbors discovered yet.\n\n");
        }
        
        // Wait for next HELLO interval
        printf("Waiting %d seconds until next HELLO...\n\n", HELLO_INTERVAL);
        sleep_seconds(HELLO_INTERVAL);
    }
    
    // Cleanup
    printf("Shutting down OLSR node...\n");
    
    // Wait for receiver thread to finish
    pthread_join(receiver_thread, NULL);
    
    print_neighbor_table();
    close(send_sockfd);
    close(recv_sockfd);
    
    printf("OLSR node stopped.\n");
    return EXIT_SUCCESS;
}
