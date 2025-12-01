
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
void send_hello_message(struct control_queue* queue);

/**
 * @brief Set this node's TDMA slot reservation
 * @param slot Slot number to reserve (>=0), -1 to clear
 */
void set_my_slot_reservation(int slot);

/**
 * @brief Clear this node's TDMA slot reservation
 */
void clear_my_slot_reservation(void);

/**
 * @brief Get this node's current reserved slot
 * @return Reserved slot number, or -1 if none
 */
int get_my_reserved_slot(void);

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
int push_hello_to_queue(struct control_queue* queue, const uint8_t* serialized_buffer, int serialized_size);

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
void update_neighbor(uint32_t neighbor_addr, int link_type, uint8_t willingness);

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

/**
 * @brief Display all one-hop neighbors
 * 
 * Prints a formatted table of all one-hop neighbors with their details
 * including link status, willingness, MPR status, and last seen time.
 */
void display_one_hop_neighbors(void);

/**
 * @brief Display all two-hop neighbors
 * 
 * Prints a formatted table of all two-hop neighbors showing which
 * one-hop neighbor provides reachability to each two-hop neighbor.
 */
void display_two_hop_neighbors(void);

/**
 * @brief Update MPR selector status based on received HELLO
 * @param hello_msg Received HELLO message
 * @param sender_id Sender's node ID
 */
void update_mpr_selector_status(struct olsr_hello* hello_msg, uint32_t sender_id);

/**
 * @brief Get count of neighbors who selected us as MPR
 * @return Number of MPR selectors
 */
int get_mpr_selector_count(void);

// TDMA slot management functions
void set_my_slot_reservation(int slot_number);
void update_neighbor_slot_reservation(uint32_t node_id, int slot_number, int hop_distance);
int get_neighbor_slot_reservation(uint32_t node_id);
int is_slot_available(int slot_number);
int get_occupied_slots(int* occupied_slots, int max_slots);
void print_tdma_reservations(void);
void cleanup_expired_reservations(int max_age);

/**
 * @brief Check neighbor table for expired HELLO timeouts
 * @return Number of neighbors that failed timeout check
 */
int check_neighbor_timeouts(void);

/**
 * @brief Handle link failure for a specific neighbor
 * @param neighbor_id ID of the failed neighbor
 */
void handle_link_failure(uint32_t neighbor_id);

/**
 * @brief Generate emergency HELLO message after topology change
 * @param queue Pointer to the control queue
 * @return 0 on success, -1 on failure
 */
int generate_emergency_hello(struct control_queue* queue);

#endif
