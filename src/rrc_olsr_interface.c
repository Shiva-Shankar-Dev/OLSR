/**
 * OLSR-RRC Integration Layer
 * Bridges the RRC team's message queue system with OLSR routing implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#include <unistd.h>
#include "../include/olsr.h"
#include "../include/routing.h"
#include "../include/tc.h"
#include "rrc_message_queue.h"  // From RRC team

// External OLSR variables and functions
extern uint32_t node_id;
extern struct control_queue global_ctrl_queue;

// Forward declarations of your OLSR functions
extern int get_next_hop(uint32_t dest_id, uint32_t* next_hop_id, uint32_t* metric, int* hops);
extern void update_routing_table(void);
extern void send_tc_message(struct control_queue* queue);

/**
 * @brief Convert 8-bit node ID (RRC format) to 32-bit node ID (OLSR format)
 * This assumes a simple mapping - adjust based on your actual ID scheme
 */
static uint32_t convert_node_id_to_olsr(uint8_t rrc_node_id) {
    // Example: Convert node 1 to 192.168.0.1 format
    // Adjust this based on your actual addressing scheme
    return (192 << 24) | (168 << 16) | (0 << 8) | rrc_node_id;
}

/**
 * @brief Convert 32-bit node ID (OLSR format) to 8-bit node ID (RRC format)
 */
static uint8_t convert_node_id_from_olsr(uint32_t olsr_node_id) {
    // Extract the last byte
    return (uint8_t)(olsr_node_id & 0xFF);
}

/**
 * @brief Process route request from RRC layer
 * This is called by the OLSR thread when it receives MSG_OLSR_ROUTE_REQUEST
 */
static void process_rrc_route_request(LayerMessage* msg) {
    uint8_t dest_rrc = msg->data.olsr_route_req.destination_node;
    uint32_t req_id = msg->data.olsr_route_req.request_id;
    
    printf("\n=== OLSR-RRC: Processing Route Request ===\n");
    printf("Destination (RRC format): %u\n", dest_rrc);
    printf("Request ID: %u\n", req_id);
    
    // Convert RRC node ID to OLSR node ID format
    uint32_t dest_olsr = convert_node_id_to_olsr(dest_rrc);
    printf("Destination (OLSR format): 0x%08X\n", dest_olsr);
    
    // Use your OLSR get_next_hop function
    uint32_t next_hop_olsr = 0;
    uint32_t metric = 0;
    int hops = 0;
    
    int result = get_next_hop(dest_olsr, &next_hop_olsr, &metric, &hops);
    
    // Prepare response
    LayerMessage response;
    response.type = MSG_OLSR_ROUTE_RESPONSE;
    response.data.olsr_route_resp.request_id = req_id;
    response.data.olsr_route_resp.destination_node = dest_rrc;
    while(1){
        if (result == 1) {
            // Destination is self
            printf("OLSR-RRC: Destination is this node\n");
            response.data.olsr_route_resp.next_hop_node = convert_node_id_from_olsr(node_id);
            response.data.olsr_route_resp.hop_count = 0;
            break;
        } else if (result == 0) {
            // Route found
            uint8_t next_hop_rrc = convert_node_id_from_olsr(next_hop_olsr);
            printf("OLSR-RRC: Route found - next_hop=%u (OLSR: 0x%08X), hops=%d\n", 
                next_hop_rrc, next_hop_olsr, hops);
            
            response.data.olsr_route_resp.next_hop_node = next_hop_rrc;
            response.data.olsr_route_resp.hop_count = (uint8_t)hops;
            break;
        } else if (result == -2) {
            // Destination unreachable (node left network or link failure)
            printf("OLSR-RRC: Destination unreachable (node left network)\n");
            response.data.olsr_route_resp.next_hop_node = 0xFF;  // No route
            response.data.olsr_route_resp.hop_count = 0xFF;
            break;
        } else {
            // No route found - trigger route discovery
            printf("OLSR-RRC: No route found, triggering route discovery\n");
            response.data.olsr_route_resp.next_hop_node = 0xFF;  // No route
            response.data.olsr_route_resp.hop_count = 0xFF;
            
            // Trigger TC message broadcast for route discovery
            send_tc_message(&global_ctrl_queue);
            result = get_next_hop(dest_olsr, &next_hop_olsr, &metric, &hops);
        }
    }
    
    
    // Send response back to RRC
    if (message_queue_enqueue(&olsr_to_rrc_queue, &response, 5000)) {
        printf("OLSR-RRC: Route response sent successfully\n");
    } else {
        printf("OLSR-RRC: ERROR - Failed to send route response\n");
    }
    
    printf("=========================================\n\n");
}

/**
 * @brief OLSR Layer Thread - Integrated with your OLSR implementation
 * This replaces the skeleton thread provided by RRC team
 */
void* olsr_layer_thread(void* arg) {
    uint8_t my_node_id_rrc = *(uint8_t*)arg;
    free(arg);  // Free the allocated node ID
    
    // Convert to OLSR format and set global node_id
    node_id = convert_node_id_to_olsr(my_node_id_rrc);
    
    printf("OLSR-RRC: Thread started for node %u (OLSR: 0x%08X)\n", 
           my_node_id_rrc, node_id);
    
    // Initialize your OLSR protocol
    init_olsr();
    
    printf("OLSR-RRC: Waiting for route requests from RRC...\n");
    
    // Main loop - process messages from RRC
    while (1) {
        LayerMessage msg;
        
        // Wait for message from RRC (with 1 second timeout)
        if (message_queue_dequeue(&rrc_to_olsr_queue, &msg, 1000)) {
            
            if (msg.type == MSG_OLSR_ROUTE_REQUEST) {
                // Process route request using your OLSR implementation
                process_rrc_route_request(&msg);
            } else {
                printf("OLSR-RRC: Unknown message type: %d\n", msg.type);
            }
        }
        
        // Periodic OLSR maintenance (run every second)
        // This keeps your OLSR protocol running in the background
        static time_t last_maintenance = 0;
        time_t now = time(NULL);
        
        if (now - last_maintenance >= 1) {
            // Perform periodic tasks
            cleanup_neighbor_table();
            cleanup_tc_topology();
            cleanup_duplicate_table();
            cleanup_topology_links();
            
            last_maintenance = now;
        }
    }
    
    return NULL;
}

/**
 * @brief Start OLSR layer thread (called by main or RRC)
 */
pthread_t start_olsr_thread(uint8_t node_id) {
    pthread_t thread;
    uint8_t* node_id_arg = malloc(sizeof(uint8_t));
    *node_id_arg = node_id;
    
    if (pthread_create(&thread, NULL, olsr_layer_thread, node_id_arg) != 0) {
        fprintf(stderr, "OLSR-RRC: Failed to create thread\n");
        free(node_id_arg);
        return 0;
    }
    
    printf("OLSR-RRC: Thread created successfully\n");
    return thread;
}