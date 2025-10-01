
/**
 * @file hello.h
 * @brief HELLO message handling and neighbor management functions
 * @author OLSR Implementation Team
 * @date 2025-09-23
 * 
 * This file contains function declarations for HELLO message creation,
 * processing, and neighbor table management in the OLSR protocol.
 */

#ifndef HELLO_H
#define HELLO_H

#include "olsr.h"
#include "packet.h"

/**
 * @brief Generate a new HELLO message
 * 
 * Creates a HELLO message containing the node's willingness and
 * current neighbor information for broadcast to one-hop neighbors.
 * 
 * @return Pointer to newly created HELLO message, or NULL on failure
 */
struct olsr_hello* generate_hello_message(void);

/**
 * @brief Send a HELLO message
 * 
 * Generates and simulates sending a HELLO message. In this implementation,
 * it performs message creation and logging without actual network transmission.
 */
void send_hello_message(void);

/**
 * @brief Process a received HELLO message
 * 
 * Processes an incoming HELLO message to update neighbor information
 * and maintain the neighbor table.
 * 
 * @param msg Pointer to the received OLSR message
 * @param sender_addr IP address of the message sender
 */
void process_hello_message(struct olsr_message* msg, uint32_t sender_addr);

/**
 * @brief Push a HELLO message to the control queue
 * 
 * Creates a HELLO message and adds it to the control queue for
 * later processing or transmission.
 * 
 * @param queue Pointer to the control queue
 * @return 0 on success, -1 on failure
 */
int push_hello_to_queue(struct control_queue* queue);

/**
 * @brief Add a new neighbor to the neighbor table
 * 
 * Adds a new neighbor entry to the neighbor table with specified
 * link characteristics and willingness value.
 * 
 * @param addr IP address of the neighbor
 * @param link_code Link status code (SYM_LINK, ASYM_LINK, etc.)
 * @param willingness Neighbor's willingness to act as MPR
 * @return 0 on success, -1 on failure
 */
int add_neighbor(uint32_t addr, uint8_t link_code, uint8_t willingness);

/**
 * @brief Update an existing neighbor in the table
 * 
 * Updates the link status and willingness of an existing neighbor
 * and refreshes the last-seen timestamp.
 * 
 * @param addr IP address of the neighbor to update
 * @param link_code New link status code
 * @param willingness New willingness value
 * @return 0 on success, -1 if neighbor not found
 */
int update_neighbor(uint32_t addr, uint8_t link_code, uint8_t willingness);

/**
 * @brief Find a neighbor in the neighbor table
 * 
 * Searches the neighbor table for a specific neighbor by IP address.
 * 
 * @param addr IP address of the neighbor to find
 * @return Pointer to neighbor entry if found, NULL otherwise
 */
struct neighbor_entry* find_neighbor(uint32_t addr);

/**
 * @brief Print the current neighbor table
 * 
 * Displays the contents of the neighbor table in a human-readable format,
 * showing neighbor addresses, willingness values, and link status.
 */
void print_neighbor_table(void);

#endif
