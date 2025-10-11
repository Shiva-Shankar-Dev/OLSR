/**
 * @file mpr.c
 * @brief MPR (Multipoint Relay) selection implementation for OLSR protocol
 * @author OLSR Implementation Team
 * @date 2025-10-10
 * 
 * This file implements the MPR selection algorithm for OLSR. MPR nodes are
 * selected to optimize flooding of broadcast messages in the network by
 * reducing redundant retransmissions while ensuring all two-hop neighbors
 * are reachable.
 */

#include "../include/olsr.h"
#include "../include/hello.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/** @brief Maximum number of two-hop neighbors */
#define MAX_TWO_HOP_NEIGHBORS 100

/**
 * @brief Two-hop neighbor structure
 * 
 * Represents a neighbor that is two hops away, reachable through
 * one of our one-hop neighbors.
 */
struct two_hop_neighbor {
    uint32_t neighbor_id;      /**< IP address of the two-hop neighbor */
    uint32_t one_hop_addr;       /**< IP address of one-hop neighbor providing reach */
    time_t last_seen;            /**< Timestamp of last update */
    struct two_hop_neighbor *next; /**< Pointer to next entry (for linked list) */
};

/** @brief Global two-hop neighbor table */
static struct two_hop_neighbor two_hop_table[MAX_TWO_HOP_NEIGHBORS];
/** @brief Current number of two-hop neighbors */
static int two_hop_count = 0;

/** @brief Array to store selected MPR addresses */
static uint32_t mpr_set[MAX_NEIGHBORS];
/** @brief Current number of MPRs in the set */
static int mpr_count = 0;

/**
 * @brief Convert a node ID to a string representation
 * @param id The node ID to convert
 * @param buffer Buffer to store the string representation (must be at least 16 bytes)
 * @return Pointer to the buffer
 */
static char* id_to_string(uint32_t id, char* buffer) {
    unsigned char* bytes = (unsigned char*)&id;
    snprintf(buffer, 16, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
    return buffer;
}

/**
 * @brief Add or update a two-hop neighbor entry
 * 
 * Adds a new two-hop neighbor to the table or updates an existing one.
 * Two-hop neighbors are nodes reachable through one-hop neighbors.
 * 
 * @param two_hop_addr IP address of the two-hop neighbor
 * @param one_hop_addr IP address of the one-hop neighbor providing reach
 * @return 0 on success, -1 on failure
 */
int add_two_hop_neighbor(uint32_t two_hop_addr, uint32_t one_hop_addr) {
    // Check if two-hop neighbor already exists
    for (int i = 0; i < two_hop_count; i++) {
        if (two_hop_table[i].neighbor_id == two_hop_addr &&
            two_hop_table[i].one_hop_addr == one_hop_addr) {
            // Update existing entry
            two_hop_table[i].last_seen = time(NULL);
            return 0;
        }
    }
    
    // Add new two-hop neighbor
    if (two_hop_count >= MAX_TWO_HOP_NEIGHBORS) {
        printf("Error: Two-hop neighbor table full\n");
        return -1;
    }
    
    two_hop_table[two_hop_count].neighbor_id = two_hop_addr;
    two_hop_table[two_hop_count].one_hop_addr = one_hop_addr;
    two_hop_table[two_hop_count].last_seen = time(NULL);
    two_hop_table[two_hop_count].next = NULL;
    two_hop_count++;
    
    char two_hop_str[16], one_hop_str[16];
    printf("Added two-hop neighbor: %s via %s\n",
           id_to_string(two_hop_addr, two_hop_str),
           id_to_string(one_hop_addr, one_hop_str));
    
    return 0;
}

/**
 * @brief Remove a two-hop neighbor from the table
 * 
 * @param two_hop_addr IP address of the two-hop neighbor to remove
 * @param one_hop_addr IP address of the one-hop neighbor
 * @return 0 on success, -1 if not found
 */
int remove_two_hop_neighbor(uint32_t two_hop_addr, uint32_t one_hop_addr) {
    for (int i = 0; i < two_hop_count; i++) {
        if (two_hop_table[i].neighbor_id == two_hop_addr &&
            two_hop_table[i].one_hop_addr == one_hop_addr) {
            // Shift remaining entries
            for (int j = i; j < two_hop_count - 1; j++) {
                two_hop_table[j] = two_hop_table[j + 1];
            }
            two_hop_count--;
            
            char two_hop_str[16], one_hop_str[16];
            printf("Removed two-hop neighbor: %s via %s\n",
                   id_to_string(two_hop_addr, two_hop_str),
                   id_to_string(one_hop_addr, one_hop_str));
            return 0;
        }
    }
    return -1;
}

/**
 * @brief Count how many two-hop neighbors are covered by a one-hop neighbor
 * 
 * @param one_hop_addr IP address of the one-hop neighbor
 * @param uncovered_only If 1, only count uncovered two-hop neighbors
 * @param covered_set Array indicating which two-hop neighbors are already covered
 * @return Number of two-hop neighbors reachable through this one-hop neighbor
 */
static int count_reachable_two_hop(uint32_t one_hop_addr, int uncovered_only, int* covered_set) {
    int count = 0;
    
    for (int i = 0; i < two_hop_count; i++) {
        if (two_hop_table[i].one_hop_addr == one_hop_addr) {
            if (!uncovered_only || !covered_set[i]) {
                count++;
            }
        }
    }
    
    return count;
}

/**
 * @brief Check if a one-hop neighbor is the only path to any two-hop neighbor
 * 
 * @param one_hop_addr IP address of the one-hop neighbor to check
 * @return 1 if this is the only path to at least one two-hop neighbor, 0 otherwise
 */
static int is_only_path(uint32_t one_hop_addr) {
    for (int i = 0; i < two_hop_count; i++) {
        uint32_t two_hop = two_hop_table[i].neighbor_id;
        
        // Count how many one-hop neighbors can reach this two-hop neighbor
        int path_count = 0;
        for (int j = 0; j < two_hop_count; j++) {
            if (two_hop_table[j].neighbor_id == two_hop) {
                path_count++;
            }
        }
        
        // If this one-hop neighbor provides the only path
        if (path_count == 1 && two_hop_table[i].one_hop_addr == one_hop_addr) {
            return 1;
        }
    }
    
    return 0;
}

/**
 * @brief Mark two-hop neighbors as covered by a selected MPR
 * 
 * @param one_hop_addr IP address of the MPR one-hop neighbor
 * @param covered_set Array to mark covered two-hop neighbors
 */
static void mark_covered_two_hop(uint32_t one_hop_addr, int* covered_set) {
    for (int i = 0; i < two_hop_count; i++) {
        if (two_hop_table[i].one_hop_addr == one_hop_addr) {
            covered_set[i] = 1;
        }
    }
}

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
int calculate_mpr_set(void) {
    printf("\n=== Starting MPR Calculation ===\n");
    
    // Clear current MPR set
    mpr_count = 0;
    memset(mpr_set, 0, sizeof(mpr_set));
    
    // Mark all neighbors as non-MPR initially
    for (int i = 0; i < neighbor_count; i++) {
        neighbor_table[i].is_mpr = 0;
    }
    
    // If no two-hop neighbors, no MPRs needed
    if (two_hop_count == 0) {
        printf("No two-hop neighbors found, MPR set is empty\n");
        return 0;
    }
    
    // Array to track which two-hop neighbors are covered
    int covered_set[MAX_TWO_HOP_NEIGHBORS] = {0};
    
    // Step 1: Select all neighbors with willingness WILL_ALWAYS
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].link_status == SYM_LINK &&
            neighbor_table[i].willingness == WILL_ALWAYS) {
            
            mpr_set[mpr_count++] = neighbor_table[i].neighbor_id;
            neighbor_table[i].is_mpr = 1;
            mark_covered_two_hop(neighbor_table[i].neighbor_id, covered_set);
            
            char addr_str[16];
            printf("Selected MPR (WILL_ALWAYS): %s\n",
                   id_to_string(neighbor_table[i].neighbor_id, addr_str));
        }
    }
    
    // Step 2: Select neighbors that are the only path to some two-hop neighbor
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].link_status == SYM_LINK &&
            !neighbor_table[i].is_mpr &&
            neighbor_table[i].willingness != WILL_NEVER) {
            
            if (is_only_path(neighbor_table[i].neighbor_id)) {
                mpr_set[mpr_count++] = neighbor_table[i].neighbor_id;
                neighbor_table[i].is_mpr = 1;
                mark_covered_two_hop(neighbor_table[i].neighbor_id, covered_set);
                
                char addr_str[16];
                printf("Selected MPR (only path): %s\n",
                       id_to_string(neighbor_table[i].neighbor_id, addr_str));
            }
        }
    }
    
    // Step 3: Select neighbors based on reachability and willingness
    int all_covered = 0;
    while (!all_covered) {
        all_covered = 1;
        
        // Check if all two-hop neighbors are covered
        for (int i = 0; i < two_hop_count; i++) {
            if (!covered_set[i]) {
                all_covered = 0;
                break;
            }
        }
        
        if (all_covered) {
            break;
        }
        
        // Find neighbor that covers most uncovered two-hop neighbors
        int best_neighbor_idx = -1;
        int max_new_coverage = 0;
        int best_willingness = -1;
        
        for (int i = 0; i < neighbor_count; i++) {
            if (neighbor_table[i].link_status == SYM_LINK &&
                !neighbor_table[i].is_mpr &&
                neighbor_table[i].willingness != WILL_NEVER) {
                
                int new_coverage = count_reachable_two_hop(
                    neighbor_table[i].neighbor_id, 1, covered_set);
                
                // Select neighbor with most coverage, or highest willingness if tied
                if (new_coverage > max_new_coverage ||
                    (new_coverage == max_new_coverage && 
                     neighbor_table[i].willingness > best_willingness)) {
                    max_new_coverage = new_coverage;
                    best_neighbor_idx = i;
                    best_willingness = neighbor_table[i].willingness;
                }
            }
        }
        
        // If we found a neighbor to add
        if (best_neighbor_idx >= 0) {
            mpr_set[mpr_count++] = neighbor_table[best_neighbor_idx].neighbor_id;
            neighbor_table[best_neighbor_idx].is_mpr = 1;
            mark_covered_two_hop(neighbor_table[best_neighbor_idx].neighbor_id, covered_set);
            
            char addr_str[16];
            printf("Selected MPR (coverage=%d, will=%d): %s\n",
                   max_new_coverage,
                   neighbor_table[best_neighbor_idx].willingness,
                   id_to_string(neighbor_table[best_neighbor_idx].neighbor_id, addr_str));
        } else {
            // No valid neighbor found but not all covered - shouldn't happen
            printf("Warning: Cannot cover all two-hop neighbors\n");
            break;
        }
    }
    
    printf("MPR calculation complete: %d MPRs selected\n", mpr_count);
    return 0;
}

/**
 * @brief Get the current MPR set
 * 
 * @param mpr_array Pointer to array to store MPR addresses
 * @param max_size Maximum size of the array
 * @return Number of MPRs in the set
 */
int get_mpr_set(uint32_t* mpr_array, int max_size) {
    int count = (mpr_count < max_size) ? mpr_count : max_size;
    
    for (int i = 0; i < count; i++) {
        mpr_array[i] = mpr_set[i];
    }
    
    return count;
}

/**
 * @brief Get the current MPR count
 * 
 * @return Number of MPRs in the current set
 */
int get_mpr_count(void) {
    return mpr_count;
}

/**
 * @brief Check if a neighbor is selected as MPR
 * 
 * @param neighbor_id IP address of the neighbor
 * @return 1 if neighbor is MPR, 0 otherwise
 */
int is_mpr(uint32_t neighbor_id) {
    for (int i = 0; i < mpr_count; i++) {
        if (mpr_set[i] == neighbor_id) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Print the current MPR set
 * 
 * Displays all neighbors currently selected as MPRs in a human-readable format.
 */
void print_mpr_set(void) {
    printf("\n=== MPR Set (%d entries) ===\n", mpr_count);
    
    if (mpr_count == 0) {
        printf("MPR set is empty\n");
        return;
    }
    
    for (int i = 0; i < mpr_count; i++) {
        char addr_str[16];
        printf("  MPR[%d]: %s\n", i + 1,
               id_to_string(mpr_set[i], addr_str));
    }
    
    printf("=========================\n\n");
}

/**
 * @brief Print the two-hop neighbor table
 * 
 * Displays all two-hop neighbors and the one-hop neighbors through which
 * they are reachable.
 */
void print_two_hop_table(void) {
    printf("\n=== Two-Hop Neighbor Table (%d entries) ===\n", two_hop_count);
    
    if (two_hop_count == 0) {
        printf("No two-hop neighbors\n");
        return;
    }
    
    for (int i = 0; i < two_hop_count; i++) {
        char two_hop_str[16], one_hop_str[16];
        printf("  %s via %s\n",
               id_to_string(two_hop_table[i].neighbor_id, two_hop_str),
               id_to_string(two_hop_table[i].one_hop_addr, one_hop_str));
    }
    
    printf("=========================================\n\n");
}

/**
 * @brief Clear the MPR set
 * 
 * Resets the MPR set and marks all neighbors as non-MPR.
 */
void clear_mpr_set(void) {
    mpr_count = 0;
    memset(mpr_set, 0, sizeof(mpr_set));
    
    for (int i = 0; i < neighbor_count; i++) {
        neighbor_table[i].is_mpr = 0;
    }
    
    printf("MPR set cleared\n");
}

/**
 * @brief Clear the two-hop neighbor table
 * 
 * Removes all entries from the two-hop neighbor table.
 */
void clear_two_hop_table(void) {
    two_hop_count = 0;
    memset(two_hop_table, 0, sizeof(two_hop_table));
    
    printf("Two-hop neighbor table cleared\n");
}

/**
 * @brief Update two-hop neighbors based on received HELLO messages
 * 
 * This function should be called after processing HELLO messages to update
 * the two-hop neighbor information. It extracts two-hop neighbors from the
 * HELLO messages of one-hop neighbors.
 * 
 * @param hello_msg Pointer to the HELLO message
 * @param sender_addr IP address of the sender (one-hop neighbor)
 */
void update_two_hop_neighbors_from_hello(struct olsr_hello* hello_msg, uint32_t sender_addr) {
    if (!hello_msg) {
        printf("Error: NULL HELLO message\n");
        return;
    }
    
    // Check if sender is a symmetric neighbor
    int is_symmetric = 0;
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].neighbor_id == sender_addr &&
            neighbor_table[i].link_status == SYM_LINK) {
            is_symmetric = 1;
            break;
        }
    }
    
    if (!is_symmetric) {
        return; // Only process HELLO from symmetric neighbors
    }
    
    // Add all symmetric neighbors of the sender as our two-hop neighbors
    for (int i = 0; i < hello_msg->neighbor_count; i++) {
        uint32_t two_hop_addr = hello_msg->neighbors[i].neighbor_id;
        
        // Skip if the two-hop neighbor is actually us
        if (two_hop_addr == node_id) {
            continue;
        }
        
        // Skip if the two-hop neighbor is already a one-hop neighbor
        int is_one_hop = 0;
        for (int j = 0; j < neighbor_count; j++) {
            if (neighbor_table[j].neighbor_id == two_hop_addr) {
                is_one_hop = 1;
                break;
            }
        }
        
        if (!is_one_hop && hello_msg->neighbors[i].link_code == SYM_LINK) {
            add_two_hop_neighbor(two_hop_addr, sender_addr);
        }
    }
}

/**
 * @brief Get the current two-hop neighbor count
 * 
 * @return Number of two-hop neighbors in the table
 */
int get_two_hop_count(void) {
    return two_hop_count;
}
