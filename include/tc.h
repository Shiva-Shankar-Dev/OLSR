/**
 * @file tc.h
 * @brief TC (Topology Control) message handling functions
 * @author OLSR Implementation Team
 * @date 2025-10-02
 * 
 * This file contains function declarations for TC message creation,
 * processing, and topology information management in the OLSR protocol.
 */

#ifndef TC_H
#define TC_H

#include "olsr.h"
#include "packet.h"

/**
 * @brief Add an MPR selector to the local list
 * @param selector_addr IP address of the MPR selector to add
 * @return 0 on success, -1 if already exists or list is full
 */
int add_mpr_selector(uint32_t selector_addr);

/**
 * @brief Remove an MPR selector from the local list
 * @param selector_addr IP address of the MPR selector to remove
 * @return 0 on success, -1 if not found
 */
int remove_mpr_selector(uint32_t selector_addr);

/**
 * @brief Send a TC message
 */
void send_tc_message(void);

/**
 * @brief Process a received TC message
 * @param msg Pointer to the OLSR message containing the TC
 * @param sender_addr IP address of the message sender
 */
void process_tc_message(struct olsr_message* msg, uint32_t sender_addr);

/**
 * @brief Push a TC message to the control queue
 * @param queue Pointer to the control queue where the message will be stored
 * @return 0 on success, -1 on failure
 */
int push_tc_to_queue(struct control_queue* queue);

/**
 * @brief Get current MPR selector count
 * @return Number of current MPR selectors
 */
int get_mpr_selector_count(void);

/**
 * @brief Get current ANSN value
 * @return Current ANSN (Advertised Neighbor Sequence Number)
 */
uint16_t get_current_ansn(void);

#endif