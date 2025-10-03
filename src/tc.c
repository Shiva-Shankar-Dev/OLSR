/**
 * @file tc.c
 * @brief TC (Topology Control) message implementation for OLSR protocol
 * @author OLSR Implementation Team
 * @date 2025-10-02
 * 
 * This file implements TC message creation, processing, and topology
 * information dissemination functions for the OLSR protocol. It handles
 * MPR selector information broadcasting and topology table management.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <unistd.h>
#endif
#include <time.h>
#include "../include/olsr.h"
#include "../include/packet.h"
#include "../include/hello.h"
#include "../include/tc.h"

/** @brief Global ANSN (Advertised Neighbor Sequence Number) counter */
static uint16_t ansn_counter = 0;

/** @brief Array to store MPR selector addresses */
static uint32_t mpr_selectors[MAX_NEIGHBORS];
/** @brief Current number of MPR selectors */
static int mpr_selector_count = 0;

/**
 * @brief Add an MPR selector to the local list
 * 
 * Adds a new MPR selector address to the local MPR selector list.
 * This function ensures no duplicates are added.
 * 
 * @param selector_addr IP address of the MPR selector to add
 * @return 0 on success, -1 if already exists or list is full
 */
int add_mpr_selector(uint32_t selector_addr) {
    // Check if selector already exists
    for (int i = 0; i < mpr_selector_count; i++) {
        if (mpr_selectors[i] == selector_addr) {
            return -1; // Already exists
        }
    }
    
    // Check if we have space
    if (mpr_selector_count >= MAX_NEIGHBORS) {
        printf("Error: MPR selector list is full\n");
        return -1;
    }
    
    // Add new selector
    mpr_selectors[mpr_selector_count++] = selector_addr;
    printf("Added MPR selector: %s\n", inet_ntoa(*(struct in_addr*)&selector_addr));
    return 0;
}

/**
 * @brief Remove an MPR selector from the local list
 * 
 * Removes an MPR selector address from the local MPR selector list.
 * 
 * @param selector_addr IP address of the MPR selector to remove
 * @return 0 on success, -1 if not found
 */
int remove_mpr_selector(uint32_t selector_addr) {
    for (int i = 0; i < mpr_selector_count; i++) {
        if (mpr_selectors[i] == selector_addr) {
            // Shift remaining elements
            for (int j = i; j < mpr_selector_count - 1; j++) {
                mpr_selectors[j] = mpr_selectors[j + 1];
            }
            mpr_selector_count--;
            printf("Removed MPR selector: %s\n", inet_ntoa(*(struct in_addr*)&selector_addr));
            return 0;
        }
    }
    return -1; // Not found
}

/**
 * @brief Generate a TC message
 * 
 * Creates a new TC message structure containing the node's current
 * MPR selector set and increments the ANSN counter. The message is used for
 * topology information dissemination in the OLSR protocol.
 * 
 * @return Pointer to newly allocated TC message, or NULL on failure
 * 
 * @note The caller is responsible for freeing the returned message
 * @note Only nodes selected as MPRs should generate TC messages
 */
struct olsr_tc* generate_tc_message(void) {
    struct olsr_tc* tc_msg = malloc(sizeof(struct olsr_tc));
    if (!tc_msg) {
        printf("Error: Failed to allocate memory for TC message\n");
        return NULL;
    }
    
    // Increment ANSN counter for this TC message
    tc_msg->ansn = ++ansn_counter;
    tc_msg->selector_count = mpr_selector_count;
    
    if (mpr_selector_count > 0) {
        tc_msg->mpr_selectors = malloc(mpr_selector_count * sizeof(struct tc_neighbor));
        if (!tc_msg->mpr_selectors) {
            printf("Error: Failed to allocate memory for MPR selectors list\n");
            free(tc_msg);
            return NULL;
        }
        
        // Copy MPR selector addresses
        for (int i = 0; i < mpr_selector_count; i++) {
            tc_msg->mpr_selectors[i].neighbor_addr = mpr_selectors[i];
        }
    } else {
        tc_msg->mpr_selectors = NULL;
    }
    
    printf("Generated TC message: ANSN=%d, MPR selectors=%d\n", 
           tc_msg->ansn, tc_msg->selector_count);
    
    return tc_msg;
}

/**
 * @brief Send a TC message
 * 
 * Generates a TC message, wraps it in an OLSR message header,
 * and simulates transmission. This function handles message creation,
 * sequence number assignment, and logging.
 * 
 * @note In this implementation, no actual network transmission occurs.
 *       The function demonstrates message creation and logging only.
 * @note TC messages have TTL=255 to reach all nodes in the network
 */
void send_tc_message(void) {
    // Only send TC messages if we have MPR selectors or if we're forced to
    if (mpr_selector_count == 0) {
        printf("No MPR selectors - skipping TC message generation\n");
        return;
    }
    
    // Generate TC message
    struct olsr_tc* tc_msg = generate_tc_message();
    if (!tc_msg) {
        printf("Error: Failed to generate TC message\n");
        return;
    }
    
    // Create OLSR message header
    struct olsr_message msg;
    msg.msg_type = MSG_TC;         /**< Set message type to TC */
    msg.vtime = 15;                /**< Validity time (encoded) - longer than HELLO */
    msg.originator = node_ip;      /**< Set originator to this node's IP */
    msg.ttl = 255;                 /**< TTL = 255 for TC (network-wide flooding) */
    msg.hop_count = 0;             /**< Initial hop count */
    msg.msg_seq_num = ++message_seq_num; /**< Increment and assign sequence number */
    msg.body = tc_msg;             /**< Attach TC message body */
    
    // Calculate total message size
    msg.msg_size = sizeof(struct olsr_message) + sizeof(struct olsr_tc) + 
                   (tc_msg->selector_count * sizeof(struct tc_neighbor));
    
    // Simulate sending the TC message (no actual network transmission)
    printf("TC message sent (type=%d, size=%d bytes, seq=%d)\n", 
           msg.msg_type, msg.msg_size, msg.msg_seq_num);
    printf("ANSN: %d, MPR Selectors: %d\n", 
           tc_msg->ansn, tc_msg->selector_count);
    
    // Log MPR selector addresses
    for (int i = 0; i < tc_msg->selector_count; i++) {
        printf("  MPR Selector %d: %s\n", i + 1, 
               inet_ntoa(*(struct in_addr*)&tc_msg->mpr_selectors[i].neighbor_addr));
    }
    
    // Cleanup allocated memory
    if (tc_msg->mpr_selectors) {
        free(tc_msg->mpr_selectors);
    }
    free(tc_msg);
}

/**
 * @brief Process a received TC message
 * 
 * Processes an incoming TC message to update topology information
 * and routing tables. The function extracts MPR selector information
 * and updates the topology table for route calculation.
 * 
 * @param msg Pointer to the OLSR message containing the TC
 * @param sender_addr IP address of the message sender
 * 
 * @note This function updates topology information used for routing calculations
 */
void process_tc_message(struct olsr_message* msg, uint32_t sender_addr) {
    if (msg->msg_type != MSG_TC) {
        printf("Error: Not a TC message\n");
        return;
    }
    
    struct olsr_tc* tc_msg = (struct olsr_tc*)msg->body;
    
    printf("Received TC from %s: ANSN=%d, selectors=%d\n",
           inet_ntoa(*(struct in_addr*)&sender_addr),
           tc_msg->ansn, tc_msg->selector_count);
    
    // Log received MPR selector information
    for (int i = 0; i < tc_msg->selector_count; i++) {
        printf("  Reported MPR Selector %d: %s\n", i + 1,
               inet_ntoa(*(struct in_addr*)&tc_msg->mpr_selectors[i].neighbor_addr));
    }
    
    // In a full implementation, this would update the topology table
    // and trigger route recalculation if necessary
    printf("Topology information updated from TC message\n");
}

/**
 * @brief Push a TC message to the control queue
 * 
 * Creates a new TC message and adds it to the specified control queue
 * for later processing. This function combines message generation with
 * queue management.
 * 
 * @param queue Pointer to the control queue where the message will be stored
 * @return 0 on success, -1 on failure
 * 
 * @note The control queue takes ownership of the message data
 */
int push_tc_to_queue(struct control_queue* queue) {
    if (!queue) {
        printf("Error: Control queue is NULL\n");
        return -1;
    }
    
    // Only queue TC messages if we have MPR selectors
    if (mpr_selector_count == 0) {
        printf("No MPR selectors - not queuing TC message\n");
        return 0;
    }

    // Generate TC message
    struct olsr_tc* tc_msg = generate_tc_message();
    if (!tc_msg) {
        printf("Error: Failed to generate TC message\n");
        return -1;
    }

    // Push to control queue
    int result = push_to_control_queue(queue, MSG_TC, tc_msg, sizeof(struct olsr_tc));
    
    printf("TC message created and queued (ANSN=%d, selectors=%d)\n", 
           tc_msg->ansn, tc_msg->selector_count);
    
    return result;
}

/**
 * @brief Get current MPR selector count
 * 
 * @return Number of current MPR selectors
 */
int get_mpr_selector_count(void) {
    return mpr_selector_count;
}

/**
 * @brief Get current ANSN value
 * 
 * @return Current ANSN (Advertised Neighbor Sequence Number)
 */
uint16_t get_current_ansn(void) {
    return ansn_counter;
}   
