/**
 * @file main.c
 * @brief Main entry point for OLSR protocol implementation
 * @author OLSR Implementation Team
 * @date 2025-10-02
 * 
 * This file contains the main function that demonstrates the OLSR protocol
 * implementation including HELLO message generation, TC message handling,
 * neighbor management, and control queue operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #define sleep(x) Sleep((x) * 1000)
#else
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

#include "../include/olsr.h"
#include "../include/packet.h"
#include "../include/hello.h"
#include "../include/tc.h"

/**
 * @brief Initialize Winsock (Windows only)
 */
#ifdef _WIN32
int init_winsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        printf("WSAStartup failed: %d\n", result);
        return -1;
    }
    return 0;
}

void cleanup_winsock() {
    WSACleanup();
}
#endif

/**
 * @brief Convert IP string to uint32_t
 * @param ip_str IP address string (e.g., "192.168.1.1")
 * @return IP address as uint32_t in network byte order
 */
uint32_t ip_string_to_uint32(const char* ip_str) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) == 1) {
        return addr.s_addr;
    }
    return 0;
}

/**
 * @brief Display menu options
 */
void display_menu() {
    printf("\n=== OLSR Protocol Demo ===\n");
    printf("1. Send HELLO message\n");
    printf("2. Send TC message\n");
    printf("3. Add neighbor manually\n");
    printf("4. Add MPR selector\n");
    printf("5. Remove MPR selector\n");
    printf("6. Show neighbor table\n");
    printf("7. Show MPR selector count\n");
    printf("8. Queue operations demo\n");
    printf("9. Simulate message reception\n");
    printf("0. Exit\n");
    printf("Choice: ");
}

/**
 * @brief Simulate adding some neighbors for demonstration
 */
void setup_demo_neighbors() {
    printf("Setting up demo neighbors...\n");
    
    // Add some sample neighbors
    update_neighbor(ip_string_to_uint32("192.168.1.10"), SYM_LINK, WILL_DEFAULT);
    update_neighbor(ip_string_to_uint32("192.168.1.20"), SYM_LINK, WILL_HIGH);
    update_neighbor(ip_string_to_uint32("192.168.1.30"), ASYM_LINK, WILL_LOW);
    
    printf("Demo neighbors added successfully.\n");
}

/**
 * @brief Simulate adding MPR selectors for TC message demo
 */
void setup_demo_mpr_selectors() {
    printf("Setting up demo MPR selectors...\n");
    
    add_mpr_selector(ip_string_to_uint32("192.168.1.15"));
    add_mpr_selector(ip_string_to_uint32("192.168.1.25"));
    
    printf("Demo MPR selectors added successfully.\n");
}

/**
 * @brief Demonstrate queue operations
 */
void demo_queue_operations() {
    printf("\n--- Queue Operations Demo ---\n");
    
    struct control_queue queue;
    init_control_queue(&queue);
    
    // Add some messages to queue
    printf("Adding messages to queue...\n");
    push_hello_to_queue(&queue);
    push_tc_to_queue(&queue);
    
    // Process messages from queue
    printf("\nProcessing messages from queue...\n");
    struct control_message* msg;
    while ((msg = pop_from_control_queue(&queue)) != NULL) {
        printf("Processing message type: %d\n", msg->msg_type);
        
        // In a real implementation, you would process the message here
        if (msg->msg_data) {
            // Note: In a real implementation, you'd need to free this data
            // after processing, but for demo purposes we'll skip that
        }
    }
}

/**
 * @brief Simulate receiving and processing messages
 */
void simulate_message_reception() {
    printf("\n--- Message Reception Simulation ---\n");
    
    // Simulate receiving a HELLO message
    printf("Simulating HELLO message reception...\n");
    struct olsr_hello* hello_msg = generate_hello_message();
    if (hello_msg) {
        struct olsr_message msg;
        msg.msg_type = MSG_HELLO;
        msg.originator = ip_string_to_uint32("192.168.1.50");
        msg.body = hello_msg;
        
        process_hello_message(&msg, ip_string_to_uint32("192.168.1.50"));
        
        if (hello_msg->neighbors) {
            free(hello_msg->neighbors);
        }
        free(hello_msg);
    }
    
    // Simulate receiving a TC message (if we have MPR selectors)
    if (get_mpr_selector_count() > 0) {
        printf("Simulating TC message reception...\n");
        struct olsr_tc* tc_msg = generate_tc_message();
        if (tc_msg) {
            struct olsr_message msg;
            msg.msg_type = MSG_TC;
            msg.originator = ip_string_to_uint32("192.168.1.60");
            msg.body = tc_msg;
            
            process_tc_message(&msg, ip_string_to_uint32("192.168.1.60"));
            
            if (tc_msg->mpr_selectors) {
                free(tc_msg->mpr_selectors);
            }
            free(tc_msg);
        }
    }
}

/**
 * @brief Main function - Entry point of the OLSR protocol demo
 */
int main() {
    int choice;
    char ip_input[16];
    
#ifdef _WIN32
    // Initialize Winsock on Windows
    if (init_winsock() != 0) {
        printf("Failed to initialize Winsock\n");
        return 1;
    }
#endif

    printf("OLSR Protocol Implementation Demo\n");
    printf("==================================\n");
    
    // Initialize node parameters
    node_ip = ip_string_to_uint32("192.168.1.100");  // This node's IP
    node_willingness = WILL_DEFAULT;
    
    printf("Node initialized with IP: %s\n", inet_ntoa(*(struct in_addr*)&node_ip));
    printf("Node willingness: %d\n", node_willingness);
    
    // Setup some demo data
    setup_demo_neighbors();
    setup_demo_mpr_selectors();
    
    // Main program loop
    do {
        display_menu();
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            while (getchar() != '\n'); // Clear input buffer
            continue;
        }
        
        switch (choice) {
            case 1: // Send HELLO message
                printf("\n--- Sending HELLO Message ---\n");
                send_hello_message();
                break;
                
            case 2: // Send TC message
                printf("\n--- Sending TC Message ---\n");
                send_tc_message();
                break;
                
            case 3: // Add neighbor manually
                printf("Enter neighbor IP address: ");
                if (scanf("%15s", ip_input) == 1) {
                    uint32_t neighbor_ip = ip_string_to_uint32(ip_input);
                    if (neighbor_ip != 0) {
                        update_neighbor(neighbor_ip, SYM_LINK, WILL_DEFAULT);
                    } else {
                        printf("Invalid IP address format.\n");
                    }
                }
                break;
                
            case 4: // Add MPR selector
                printf("Enter MPR selector IP address: ");
                if (scanf("%15s", ip_input) == 1) {
                    uint32_t selector_ip = ip_string_to_uint32(ip_input);
                    if (selector_ip != 0) {
                        add_mpr_selector(selector_ip);
                    } else {
                        printf("Invalid IP address format.\n");
                    }
                }
                break;
                
            case 5: // Remove MPR selector
                printf("Enter MPR selector IP to remove: ");
                if (scanf("%15s", ip_input) == 1) {
                    uint32_t selector_ip = ip_string_to_uint32(ip_input);
                    if (selector_ip != 0) {
                        remove_mpr_selector(selector_ip);
                    } else {
                        printf("Invalid IP address format.\n");
                    }
                }
                break;
                
            case 6: // Show neighbor table
                print_neighbor_table();
                break;
                
            case 7: // Show MPR selector count
                printf("Current MPR selector count: %d\n", get_mpr_selector_count());
                printf("Current ANSN: %d\n", get_current_ansn());
                break;
                
            case 8: // Queue operations demo
                demo_queue_operations();
                break;
                
            case 9: // Simulate message reception
                simulate_message_reception();
                break;
                
            case 0: // Exit
                printf("Exiting OLSR demo...\n");
                break;
                
            default:
                printf("Invalid choice. Please try again.\n");
                break;
        }
        
        if (choice != 0) {
            printf("\nPress Enter to continue...");
            while (getchar() != '\n'); // Clear input buffer
            getchar(); // Wait for Enter
        }
        
    } while (choice != 0);

#ifdef _WIN32
    cleanup_winsock();
#endif

    printf("OLSR demo completed.\n");
    return 0;
}
