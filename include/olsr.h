#ifndef OLSR_H
#define OLSR_H

#include<stdint.h>
#include<netinet/in.h>

//Defining the port number for olsr
#define OLSR_PORT 698

//Defining the numbers/id for the type of messages
#define MSG_HELLO 1
#define MSG_TC 2
#define MSG_VOICE 101

// Willingness values
#define WILL_NEVER    0
#define WILL_LOW      1
#define WILL_DEFAULT  3
#define WILL_HIGH     6
#define WILL_ALWAYS   7

// Link codes for HELLO messages
#define UNSPEC_LINK   0
#define ASYM_LINK     1
#define SYM_LINK      2
#define LOST_LINK     3

// Default intervals (in seconds)
#define HELLO_INTERVAL 2
#define TC_INTERVAL    5

// Maximum number of neighbors
#define MAX_NEIGHBORS 50

// Neighbor table entry
struct neighbor_entry {
    uint32_t addr;           // IP address of neighbor
    uint8_t link_code;       // Link status (SYM_LINK, ASYM_LINK, etc.)
    uint8_t willingness;     // Neighbor's willingness to be MPR
    time_t last_heard;       // Last time we heard from this neighbor
    int is_mpr;              // 1 if this neighbor is selected as MPR
    int is_mpr_selector;     // 1 if this neighbor selected us as MPR
};

// Global neighbor table
extern struct neighbor_entry neighbor_table[MAX_NEIGHBORS];
extern int neighbor_count;

// Global node configuration
extern uint8_t node_willingness;
extern uint32_t node_ip;
extern uint16_t message_seq_num;

#endif

