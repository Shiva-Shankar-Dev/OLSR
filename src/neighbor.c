#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "../include/olsr.h"
#include "../include/routing.h"
#include "../include/tc.h"
#include "../include/hello.h"
#include "../include/packet.h"
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

void update_neighbor(uint32_t neighbor_id, int link_type, uint8_t willingness){
    // First try to update existing neighbor
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].neighbor_id == neighbor_id) {
            neighbor_table[i].link_status = link_type;
            neighbor_table[i].willingness = willingness;
            neighbor_table[i].last_seen = time(NULL);
            neighbor_table[i].last_hello_time = time(NULL);  // Initialize for timeout tracking
            char addr_str[16];
            printf("Updated neighbor: %s (link_type=%d, willingness=%d)\n",
                   id_to_string(neighbor_id, addr_str),
                   link_type, willingness);
            return;
        }
    }
    
    // If neighbor doesn't exist, add it
    add_neighbor(neighbor_id, (uint8_t)link_type, willingness);
}

int add_neighbor(uint32_t neighbor_id, uint8_t link_code, uint8_t willingness){
    if (neighbor_count >= MAX_NEIGHBORS) {
        printf("Error: Neighbor table full\n");
        return -1;
    }
    
    neighbor_table[neighbor_count].neighbor_id = neighbor_id;
    neighbor_table[neighbor_count].link_status = link_code;
    neighbor_table[neighbor_count].willingness = willingness;
    neighbor_table[neighbor_count].last_seen = time(NULL);
    neighbor_table[neighbor_count].last_hello_time = time(NULL);  // Initialize for timeout tracking
    neighbor_table[neighbor_count].is_mpr = 0;
    neighbor_table[neighbor_count].is_mpr_selector = 0;
    neighbor_table[neighbor_count].next = NULL;
    
    neighbor_count++;
    
    char addr_str[16];
    printf("Added new neighbor: %s (link_type=%d, willingness=%d)\n",
           id_to_string(neighbor_id, addr_str),
           link_code, willingness);
    
    return 0;
}

/**
 * @brief Display all one-hop neighbors
 * 
 * Prints a formatted table of all one-hop neighbors with their details
 * including link status, willingness, MPR status, and last seen time.
 */
void display_one_hop_neighbors(void) {
    printf("\n-----------------------------------------\n");
    printf("ONE-HOP NEIGHBORS TABLE\n");
    printf("--------------------------------------------\n");
    
    if (neighbor_count == 0) {
        printf("No one-hop neighbors found.\n");
        printf("--------------------------------------------\n\n");
        return;
    }
    
    printf("%-15s %-12s %-10s %-8s %-8s %-12s\n",
           "Neighbor ID", "Link Status", "Willingness", "Is MPR", "MPR Sel", "Last Seen");
    printf("----------------------------------------\n");
    
    time_t current_time = time(NULL);
    char addr_str[16];
    
    for (int i = 0; i < neighbor_count; i++) {
        const char* link_status_str;
        switch (neighbor_table[i].link_status) {
            case 0: link_status_str = "UNSPEC"; break;
            case 1: link_status_str = "ASYM"; break;
            case 2: link_status_str = "SYM"; break;
            case 3: link_status_str = "LOST"; break;
            default: link_status_str = "UNKNOWN"; break;
        }
        
        int time_since_seen = (int)(current_time - neighbor_table[i].last_seen);
        
        printf("%-15s %-12s %-10d %-8s %-8s %ds ago\n",
               id_to_string(neighbor_table[i].neighbor_id, addr_str),
               link_status_str,
               neighbor_table[i].willingness,
               neighbor_table[i].is_mpr ? "YES" : "NO",
               neighbor_table[i].is_mpr_selector ? "YES" : "NO",
               time_since_seen);
    }
    
    printf("--------------------------------------------\n");
    printf("Total one-hop neighbors: %d\n", neighbor_count);
    printf("--------------------------------------------\n\n");
}

/**
 * @brief Display all two-hop neighbors
 * 
 * Prints a formatted table of all two-hop neighbors showing which
 * one-hop neighbor provides reachability to each two-hop neighbor.
 */
void display_two_hop_neighbors(void) {
    printf("\n-----------------------------------------\n");
    printf("TWO-HOP NEIGHBORS TABLE\n");
    printf("--------------------------------------------\n");
    
    // Get the two-hop neighbor table and count from mpr.c
    struct two_hop_neighbor* two_hop_table = get_two_hop_table();
    int count = get_two_hop_count();
    
    if (!two_hop_table || count == 0) {
        printf("No two-hop neighbors found.\n");
        printf("--------------------------------------------\n\n");
        return;
    }
    
    printf("%-15s %-15s %-12s\n",
           "Two-Hop ID", "Via One-Hop", "Last Seen");
    printf("----------------------------------------\n");
    
    time_t current_time = time(NULL);
    char two_hop_str[16], one_hop_str[16];
    
    for (int i = 0; i < count; i++) {
        int time_since_seen = (int)(current_time - two_hop_table[i].last_seen);
        
        printf("%-15s %-15s %ds ago\n",
               id_to_string(two_hop_table[i].neighbor_id, two_hop_str),
               id_to_string(two_hop_table[i].one_hop_addr, one_hop_str),
               time_since_seen);
    }
    
    printf("--------------------------------------------\n");
    printf("Total two-hop neighbors: %d\n", count);
    printf("--------------------------------------------\n\n");
}