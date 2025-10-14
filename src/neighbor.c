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