#ifndef PACKET_H
#define PACKET_H

#include<stdint.h>
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
	uint8_t msg_seq_num;
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

//still more functions need to be added!

#endif
