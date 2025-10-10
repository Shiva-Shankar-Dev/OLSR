#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include "../include/olsr.h"
#include "../include/routing.h"
#include "../include/tc.h"
#include "../include/hello.h"
#include "../include/packet.h"

void update_neighbor(uint32_t neighbor_addr, int link_type, uint8_t willingness){
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].neighbor_addr == neighbor_addr) {
            neighbor_table[i].link_status = link_type;
            neighbor_table[i].willingness = willingness;
            neighbor_table[i].last_seen = time(NULL);
            printf("Updated neighbor: %s (link_type=%d, willingness=%d)\n",
                   inet_ntoa(*(struct in_addr*)&neighbor_addr),
                   link_type, willingness);
            return;
        }
    }
}

void add_neigbhor(uint32_t neighbor_addr, int link_type, uint8_t willingness){
    if (neighbor_count >= MAX_NEIGHBORS) {
        printf("Error: Neighbor table full\n");
        return;
    }
    
    neighbor_table[neighbor_count].neighbor_addr = neighbor_addr;
    neighbor_table[neighbor_count].link_status = link_type;
    neighbor_table[neighbor_count].willingness = willingness;
    neighbor_table[neighbor_count].last_seen = time(NULL);
    neighbor_table[neighbor_count].is_mpr = 0;
    neighbor_table[neighbor_count].is_mpr_selector = 0;
    neighbor_table[neighbor_count].next = NULL;
    
    neighbor_count++;
    
    printf("Added new neighbor: %s (link_type=%d, willingness=%d)\n",
           inet_ntoa(*(struct in_addr*)&neighbor_addr),
           link_type, willingness);
}