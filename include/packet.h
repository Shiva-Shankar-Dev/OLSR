/**
 * @file packet.h
 * @brief OLSR packet and message structure definitions
 * @author OLSR Implementation Team
 * @date 2025-09-23
 * 
 * This file contains the core data structures for OLSR (Optimized Link State Routing)
 * protocol packets, messages, and their components.
 */

#ifndef PACKET_H
#define PACKET_H

#include<stdint.h>
#include<time.h>
#include "olsr.h"

/**
 * @brief OLSR packet structure
 * 
 * Main packet container that holds OLSR messages for transmission
 * over the network. Each packet can contain multiple OLSR messages.
 */
struct olsr_packet {
	uint16_t packet_length;    /**< Total length of the packet including header */
	uint16_t packet_seq_num;   /**< Sequence number to prevent duplicate packets */
	struct olsr_message *messages; /**< Pointer to linked list of OLSR messages */
};

/**
 * @brief OLSR message structure
 * 
 * Generic message structure that can contain different types of OLSR messages
 * such as HELLO or TC (Topology Control) messages.
 */
struct olsr_message {
	uint8_t msg_type;      /**< Message type (MSG_HELLO, MSG_TC, etc.) */
	uint8_t vtime;         /**< Validity time for the message */
	uint16_t msg_size;     /**< Size of the message including header */
	uint32_t originator;   /**< IP address of the message originator */
  uint8_t ttl;           /**< Time To Live - hop limit for message */ 
	uint8_t hop_count;     /**< Number of hops message has traveled */
	uint16_t msg_seq_num;  /**< Message sequence number */
	void *body;            /**< Pointer to message body (olsr_hello/olsr_tc) */
};

/**
 * @brief Topology Control (TC) message structure
 * 
 * TC messages are used to disseminate topology information throughout
 * the network. They contain MPR selector information.
 */
struct olsr_tc{
	uint16_t ansn;         /**< Advertised Neighbor Sequence Number */
	struct tc_neighbor {
		uint32_t neighbor_addr; /**< IP address of MPR selector */
	} *mpr_selectors;      /**< Array of MPR selectors */
	int selector_count;    /**< Number of MPR selectors in the array */
};

/**
 * @brief HELLO message structure
 * 
 * HELLO messages are used for neighbor discovery and link sensing.
 * They are broadcast periodically to one-hop neighbors.
 */
struct olsr_hello {
	uint16_t hello_interval; /**< Interval between HELLO messages in seconds */
	uint8_t willingness;     /**< Node's willingness to act as MPR (0-7) */
	struct hello_neighbor {
		uint32_t neighbor_addr; /**< IP address of discovered neighbor */
		uint8_t link_code;      /**< Link type and neighbor type code */
	} *neighbors;            /**< Array of neighbor information */
	int neighbor_count;      /**< Number of neighbors in the array */
};

/**
 * @brief Generate a HELLO message
 * @return Pointer to newly created HELLO message, or NULL on failure
 */
struct olsr_hello* generate_hello_message(void);

/**
 * @brief Generate a TC message  
 * @return Pointer to newly created TC message, or NULL on failure
 */
struct olsr_tc* generate_tc_message(void);

#endif
