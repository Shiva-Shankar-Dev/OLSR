/**
 * @file mpr.h
 * @brief MPR (Multipoint Relay) selection functions for OLSR
 * @author OLSR Implementation Team
 * @date 2025-10-10
 * 
 * This file contains function declarations for MPR selection, two-hop neighbor
 * management, and MPR set operations in the OLSR protocol.
 */

#ifndef MPR_H
#define MPR_H

#include "olsr.h"
#include "packet.h"

/**
 * @brief Two-hop neighbor structure
 * 
 * Represents a neighbor that is two hops away, reachable through
 * one of our one-hop neighbors.
 */
struct two_hop_neighbor {
    uint32_t neighbor_id;      /**< IP address of the two-hop neighbor */
    uint32_t one_hop_addr;     /**< IP address of one-hop neighbor providing reach */
    time_t last_seen;          /**< Timestamp of last update */
    struct two_hop_neighbor *next; /**< Pointer to next entry (for linked list) */
};

/**
 * @brief Add or update a two-hop neighbor
 * 
 * Adds a new two-hop neighbor to the table or updates an existing one.
 * Two-hop neighbors are nodes reachable through one-hop neighbors.
 * 
 * @param two_hop_addr IP address of the two-hop neighbor
 * @param one_hop_addr IP address of the one-hop neighbor providing reach
 * @return 0 on success, -1 on failure
 */
int add_two_hop_neighbor(uint32_t two_hop_addr, uint32_t one_hop_addr);

/**
 * @brief Remove a two-hop neighbor from the table
 * 
 * @param two_hop_addr IP address of the two-hop neighbor to remove
 * @param one_hop_addr IP address of the one-hop neighbor
 * @return 0 on success, -1 if not found
 */
int remove_two_hop_neighbor(uint32_t two_hop_addr, uint32_t one_hop_addr);

/**
 * @brief Remove all two-hop neighbors reachable via a specific one-hop neighbor
 * 
 * @param one_hop_addr IP address of the failed one-hop neighbor
 * @return Number of two-hop neighbors removed
 */
int remove_two_hop_via_neighbor(uint32_t one_hop_addr);

/**
 * @brief Calculate MPR set using OLSR MPR selection algorithm
 * 
 * This function implements the RFC 3626 MPR selection algorithm:
 * 1. Start with an empty MPR set
 * 2. Select neighbors with willingness WILL_ALWAYS first
 * 3. For each two-hop neighbor with only one path, select that path
 * 4. Select neighbors that reach the most uncovered two-hop neighbors
 * 5. Continue until all two-hop neighbors are covered
 * 
 * @return 0 on success, -1 on failure
 */
int calculate_mpr_set(void);

/**
 * @brief Get the current MPR set
 * 
 * Copies the current MPR addresses into the provided array.
 * 
 * @param mpr_array Pointer to array to store MPR addresses
 * @param max_size Maximum size of the array
 * @return Number of MPRs copied to the array
 */
int get_mpr_set(uint32_t* mpr_array, int max_size);

/**
 * @brief Get the current MPR count
 * 
 * @return Number of MPRs in the current set
 */
int get_mpr_count(void);

/**
 * @brief Check if a neighbor is selected as MPR
 * 
 * @param neighbor_addr IP address of the neighbor
 * @return 1 if neighbor is MPR, 0 otherwise
 */
int is_mpr(uint32_t neighbor_addr);

/**
 * @brief Print the current MPR set
 * 
 * Displays all neighbors currently selected as MPRs in a human-readable format.
 */
void print_mpr_set(void);

/**
 * @brief Print the two-hop neighbor table
 * 
 * Displays all two-hop neighbors and the one-hop neighbors through which
 * they are reachable.
 */
void print_two_hop_table(void);

/**
 * @brief Clear the MPR set
 * 
 * Resets the MPR set and marks all neighbors as non-MPR.
 */
void clear_mpr_set(void);

/**
 * @brief Clear the two-hop neighbor table
 * 
 * Removes all entries from the two-hop neighbor table.
 */
void clear_two_hop_table(void);

/**
 * @brief Get the current two-hop neighbor count
 * 
 * @return Number of two-hop neighbors in the table
 */
int get_two_hop_count(void);

/**
 * @brief Get pointer to two-hop neighbor table
 * @return Pointer to two-hop neighbor table array
 */
struct two_hop_neighbor* get_two_hop_table(void);

#endif
