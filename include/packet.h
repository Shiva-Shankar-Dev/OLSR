#ifndef PACKET_H
#define PACKET_H

#include<stdint.h>
#include<time.h>
#include "olsr.h"

//Defining the packet structure
struct olsr_packet {
	uint16_t packet_length;
	uint16_t packet_seq_num;
	struct olsr_message *messages;
};

struct olsr_message {
	uint8_t msg_type;
	uint8_t vtime;
	uint16_t msg_size;
	uint32_t originator;
	uint8_t ttl;
	uint8_t hop_count;
	uint16_t msg_seq_num;        // Fixed: was uint8_t
	void *body;
};

struct olsr_tc{
	uint16_t ansn;
	struct tc_neighbor {
		uint32_t neighbor_addr;
	} *mpr_selectors;
	int selector_count;
};

struct olsr_hello {
	uint16_t hello_interval;
	uint8_t willingness;
	struct hello_neighbor {
		uint32_t neighbor_addr;
		uint8_t link_code;
	} *neighbors;
	int neighbor_count;
};

// Function declarations for packet operations
struct olsr_hello* generate_hello_message(void);
void free_hello_message(struct olsr_hello* hello_msg);
int serialize_hello_packet(struct olsr_message* msg, char* buffer, int buffer_size);

#endif
