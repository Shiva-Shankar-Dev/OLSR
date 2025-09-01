#ifndef PACKET_H
#define PACKET_H

#include<stdint.h>
#include "olsr.h"

//Defining the packet structure
struct olsr_header{
	uint8_t msg_type;
	uint8_t length;
	uint32_t originator;
	uint16_t seqno;
}__attribute__((packed));

//still more functions need to be added!

#endif
