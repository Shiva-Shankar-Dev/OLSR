/**
 * @file hello.c
 * @brief HELLO message implementation for OLSR protocol
 * @author OLSR Implementation Team
 * @date 2025-09-23
 * 
 * This file implements HELLO message creation, processing, and neighbor
 * table management functions for the OLSR protocol. It handles neighbor
 * discovery and maintains link state information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/hello.h"
#include "../include/olsr.h"
#include "../include/packet.h"
#include "../include/mpr.h"

/**
 * @brief Convert a node ID to a string representation
 * @param id The node ID to convert
 * @param buffer Buffer to store the string representation (must be at least 16 bytes)
 * @return Pointer to the buffer
 */
static char *id_to_string(uint32_t id, char* buffer) {
    unsigned char* bytes = (unsigned char*)&id;
    snprintf(buffer, 16, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
    return buffer;
}

/** @brief Global neighbor table array */
struct neighbor_entry neighbor_table[MAX_NEIGHBORS];
/** @brief Current number of neighbors in the table */
int neighbor_count = 0;
/** @brief This node's willingness to act as MPR */
uint8_t node_willingness = WILL_DEFAULT;
/** @brief This node's IP address */
uint32_t node_id = 0;

/** @brief TDMA slot reservation table for neighbors */


static struct {
    uint32_t node_id;
    int reserved_slot;
    time_t last_updated;
    int hop_distance;  // 1 for direct neighbors, 2 for two-hop
} neighbor_slots[MAX_NEIGHBORS + MAX_TWO_HOP_NEIGHBORS];

static int slot_table_size = 0;



/** @brief This node's TDMA slot reservation */
static int my_reserved_slot = -1;  // -1 means no reservation
/** @brief Global message sequence number counter */
uint16_t message_seq_num = 0;

/**
 * @brief Get this node's current reserved slot
 * @return Reserved slot number, or -1 if none
 */
int get_my_reserved_slot(void) {
    return my_reserved_slot;
}
/**
 * @brief Generate a HELLO message
 * 
 * Creates a HELLO message structure containing the node's current
 * willingness value and neighbor information. NOTE: this implementation
 * uses static storage for the returned message and for neighbor lists.
 * The returned pointer points into static buffers which are overwritten
 * on each call and must NOT be freed by the caller.
 *
 * @return Pointer to a statically allocated HELLO message (never NULL)
 *
 * @note The returned pointer and any neighbor lists have static lifetime
 *       (valid until the next call to this function). This is intentional
 *       to avoid dynamic allocation in this build. The implementation is
 *       NOT thread-safe: concurrent calls will overwrite the same buffers.
 */
struct olsr_hello* generate_hello_message(void) {
    static struct olsr_hello hello_msg_static;
    struct olsr_hello* hello_msg = &hello_msg_static;
    memset(hello_msg, 0, sizeof(struct olsr_hello));
    
    hello_msg->hello_interval = HELLO_INTERVAL;
    hello_msg->willingness = node_willingness;
    hello_msg->neighbor_count = neighbor_count;
    hello_msg->reserved_slot = my_reserved_slot; // TDMA slot reservation

    // One-hop neighbors (stored in a static array)
    // neighbors_static lives in static storage and is reused on subsequent calls.
    // Do NOT free or retain pointers across calls; they will be overwritten.
    if (neighbor_count > 0) {
        static struct hello_neighbor neighbors_static[MAX_NEIGHBORS];
        hello_msg->neighbors = neighbors_static;
        memset(hello_msg->neighbors, 0, neighbor_count * sizeof(struct hello_neighbor));

        /* neighbors_static is a static buffer and cannot be NULL; fill it directly. */
        for (int i = 0; i < neighbor_count; i++) {
            hello_msg->neighbors[i].neighbor_id = neighbor_table[i].neighbor_id;
            hello_msg->neighbors[i].link_code = neighbor_table[i].link_status;
        }
    } else {
        hello_msg->neighbors = NULL;
    }
    
    // NEW: Two-hop neighbors with TDMA information
    int two_hop_count = get_two_hop_count();
    hello_msg->two_hop_count = 0;
    
    if (two_hop_count > 0 && two_hop_count <= MAX_TWO_HOP_NEIGHBORS) {
        /* two_hop_static is static storage reused across calls. See note above. */
        static struct two_hop_hello_neighbor two_hop_static[MAX_TWO_HOP_NEIGHBORS];
        hello_msg->two_hop_neighbors = two_hop_static;
        memset(hello_msg->two_hop_neighbors, 0, two_hop_count * sizeof(struct two_hop_hello_neighbor));
        
        // Get two-hop neighbor information from MPR module
        struct two_hop_neighbor* two_hop_list = get_two_hop_table();
        
        for (int i = 0; i < two_hop_count && hello_msg->two_hop_count < MAX_TWO_HOP_NEIGHBORS; i++) {
            hello_msg->two_hop_neighbors[hello_msg->two_hop_count].two_hop_id = two_hop_list[i].neighbor_id;
            hello_msg->two_hop_neighbors[hello_msg->two_hop_count].via_neighbor_id = two_hop_list[i].one_hop_addr;
            
            // Get TDMA slot reservation for this two-hop neighbor
            hello_msg->two_hop_neighbors[hello_msg->two_hop_count].reserved_slot = get_neighbor_slot_reservation(two_hop_list[i].neighbor_id);
            
            hello_msg->two_hop_count++;
        }
    } else {
        hello_msg->two_hop_neighbors = NULL;
    }
    
    printf("Generated HELLO: willingness=%d, neighbors=%d, two_hop=%d, our_slot=%d\n", 
           hello_msg->willingness, hello_msg->neighbor_count, hello_msg->two_hop_count, 
           hello_msg->reserved_slot);
    
    return hello_msg;
}

/**
 * @brief Send a HELLO message
 * 
 * Generates a HELLO message, wraps it in an OLSR message header,
 * and simulates transmission. This function handles message creation,
 * sequence number assignment, and logging.
 * 
 * @note In this implementation, no actual network transmission occurs.
 *       The function demonstrates message creation and logging only.
 */
void send_hello_message(struct control_queue* queue) {
    if (!queue) {
        printf("Error: Control queue is NULL\n");
        return;
    }

    struct olsr_hello* hello_msg = generate_hello_message();
    if (!hello_msg) {
        printf("Error: Failed to generate HELLO message\n");
        return;
    }

    printf("HELLO message prepared (seq=%d)\n", ++message_seq_num);
    printf("Willingness: %d, Neighbors: %d, Slot=%d\n", 
           hello_msg->willingness, hello_msg->neighbor_count, hello_msg->reserved_slot);

    // Push pointer to the HELLO structure directly to the queue
    // RRC/TDMA layer will handle serialization
    int result = push_to_control_queue(queue, MSG_HELLO, (void*)hello_msg);
    if (result == 0) {
        printf("HELLO Message successfully queued for RRC/TDMA Layer\n");
    } else {
        printf("ERROR: Failed to queue HELLO Message (code=%d)\n", result);
        // Free the message if queueing failed
        free(hello_msg->neighbors);
        if (hello_msg->two_hop_neighbors) {
            free(hello_msg->two_hop_neighbors);
        }
        free(hello_msg);
    }
}

/**
 * @brief Process a received HELLO message
 * 
 * Processes an incoming HELLO message to update neighbor information
 * and determine link symmetry. The function checks if this node is
 * mentioned in the sender's neighbor list to establish bidirectional links.
 * 
 * RECEIVE FLOW CONTEXT:
 * This function is called after the following steps have been completed:
 * 1. Raw bytes received from MAC layer
 * 2. deserialize_hello() called to convert bytes → struct olsr_hello
 * 3. olsr_message wrapper created with body pointing to deserialized hello
 * 4. THIS function called to process the structured data
 * 
 * See receive_and_process_message() in main.c for the complete receive path.
 * 
 * @param msg Pointer to the OLSR message containing the HELLO
 *            msg->body must point to a deserialized struct olsr_hello
 * @param sender_addr IP address of the message sender
 * 
 * @note This function updates the neighbor table and establishes link symmetry
 */
void process_hello_message(struct olsr_message* msg, uint32_t sender_addr) {
    if (msg->msg_type != MSG_HELLO) {
        printf("Error: Not a HELLO message\n");
        return;
    }
    
    // Extract the deserialized HELLO message from the wrapper
    // This was already deserialized by deserialize_hello() before calling this function
    struct olsr_hello* hello_msg = (struct olsr_hello*)msg->body;
    
    char sender_str[16];
    printf("Received HELLO from %s: willingness=%d, neighbors=%d, two_hop=%d, slot=%d\n", 
           id_to_string(sender_addr, sender_str), hello_msg->willingness, 
           hello_msg->neighbor_count, hello_msg->two_hop_count, hello_msg->reserved_slot);
    
    // Update sender's TDMA slot reservation
    update_neighbor_slot_reservation(sender_addr, hello_msg->reserved_slot, 1);
    
    // Process two-hop neighbor TDMA information
    for (int i = 0; i < hello_msg->two_hop_count; i++) {
        uint32_t two_hop_id = hello_msg->two_hop_neighbors[i].two_hop_id;
        int slot = hello_msg->two_hop_neighbors[i].reserved_slot;
        
        if (two_hop_id != node_id) { // Don't process info about ourselves
            update_neighbor_slot_reservation(two_hop_id, slot, 2);
            
            char two_hop_str[16];
            printf("  Two-hop via %s: Node %s using slot %d\n",
                   sender_str, id_to_string(two_hop_id, two_hop_str), slot);
        }
    }
    
    // Check if we are mentioned in the sender's neighbor list (bidirectional link)
    int we_are_mentioned = 0;
    for (int i = 0; i < hello_msg->neighbor_count; i++) {
        if (hello_msg->neighbors[i].neighbor_id == node_id) {
            we_are_mentioned = 1;
            printf("We are mentioned in neighbor's HELLO message\n");
            break;
        }
    }
    if (we_are_mentioned) {
        update_neighbor(sender_addr, SYM_LINK, hello_msg->willingness);
    } else {
        update_neighbor(sender_addr, ASYM_LINK, hello_msg->willingness);
    }
    
    // Update last_hello_time for timeout tracking
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].neighbor_id == sender_addr) {
            neighbor_table[i].last_hello_time = time(NULL);
            break;
        }
    }
    
    // Extract two-hop neighbor information from HELLO message
    // Only process if sender is a symmetric neighbor
    int sender_is_symmetric = 0;
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].neighbor_id == sender_addr &&
            neighbor_table[i].link_status == SYM_LINK) {
            sender_is_symmetric = 1;
            break;
        }
    }
    
    if (sender_is_symmetric) {
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
            
            // Only add if symmetric link and not already one-hop
            if (!is_one_hop && hello_msg->neighbors[i].link_code == SYM_LINK) {
                add_two_hop_neighbor(two_hop_addr, sender_addr);
            }
        }
    }
    
    // Recalculate MPR set after topology update
    printf("Topology updated, recalculating MPR set...\n");
    calculate_mpr_set();
    update_mpr_selector_status(hello_msg, sender_addr);
    cleanup_expired_reservations(SLOT_RESERVATION_TIMEOUT);
    print_tdma_reservations();
}

/**
 * @brief Update MPR selector status based on received HELLO
 * @param hello_msg Received HELLO message
 * @param sender_id Sender's node ID
 */
void update_mpr_selector_status(struct olsr_hello* hello_msg, uint32_t sender_id) {
    if (!hello_msg) {
        return;
    }
    
    int sender_idx = -1;
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].neighbor_id == sender_id) {
            sender_idx = i;
            break;
        }
    }
    if (sender_idx == -1) {
        return;
    }
    
    // Check if sender lists us as MPR neighbor
    int selected_as_mpr = 0;
    for (int i = 0; i < hello_msg->neighbor_count; i++) {
        if (hello_msg->neighbors[i].neighbor_id == node_id &&
            hello_msg->neighbors[i].link_code == MPR_NEIGH) {
            selected_as_mpr = 1;
            break;
        }
    }
    
    // Update MPR selector flag
    int was_selector = neighbor_table[sender_idx].is_mpr_selector;
    neighbor_table[sender_idx].is_mpr_selector = selected_as_mpr;
    
    // Log changes
    if (selected_as_mpr && !was_selector) {
        char sender_str[16];
        printf("Neighbor %s selected us as MPR\n",
               id_to_string(sender_id, sender_str));
    } else if (!selected_as_mpr && was_selector) {
        char sender_str[16];
        printf("Neighbor %s no longer selects us as MPR\n",
               id_to_string(sender_id, sender_str));
    }
}

/**
 * @brief Get count of neighbors who selected us as MPR
 * @return Number of MPR selectors
 */
int get_mpr_selector_count(void) {
    int count = 0;
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].link_status == SYM_LINK &&
            neighbor_table[i].is_mpr_selector) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Print the current neighbor table
 * 
 * Displays the contents of the neighbor table in a human-readable format,
 * showing neighbor addresses, willingness values, and link status.
 */
void print_neighbor_table(void) {
    printf("\n=== Neighbor Table ===\n");
    printf("%-15s %-12s %-10s %-8s %-8s\n", "Neighbor ID", "Link Status", "Willingness", "Is MPR", "MPR Sel");
    printf("---------------------------------------------------------------\n");
    
    for (int i = 0; i < neighbor_count; i++) {
        char addr_str[16];
        const char* link_status_str;
        
        switch (neighbor_table[i].link_status) {
            case SYM_LINK: link_status_str = "SYM_LINK"; break;
            case ASYM_LINK: link_status_str = "ASYM_LINK"; break;
            case LOST_LINK: link_status_str = "LOST_LINK"; break;
            case MPR_NEIGH: link_status_str = "MPR_NEIGH"; break;
            default: link_status_str = "UNKNOWN"; break;
        }
        
        printf("%-15s %-12s %-10d %-8s %-8s\n",
               id_to_string(neighbor_table[i].neighbor_id, addr_str),
               link_status_str,
               neighbor_table[i].willingness,
               neighbor_table[i].is_mpr ? "YES" : "NO",
               neighbor_table[i].is_mpr_selector ? "YES" : "NO");
    }
    printf("Total neighbors: %d\n", neighbor_count);
    printf("=======================\n\n");
}

/**
 * @brief Set this node's TDMA slot reservation
 * @param slot_number TDMA slot number (-1 for no reservation)
 */
void set_my_slot_reservation(int slot_number) {
    my_reserved_slot = slot_number;
    if (slot_number >= 0) {
        printf("Set my TDMA slot reservation to: %d\n", slot_number);
    } else {
        printf("Cleared my TDMA slot reservation\n");
    }
}

/**
 * @brief Update neighbor's TDMA slot reservation
 * @param node_id Neighbor's node ID
 * @param slot_number TDMA slot number (-1 for no reservation)
 * @param hop_distance 1 for direct neighbor, 2 for two-hop neighbor
 */
void update_neighbor_slot_reservation(uint32_t neighbor_id, int slot_number, int hop_distance) {
    extern uint32_t node_id; // Reference to the global node_id
    if (neighbor_id == 0 || neighbor_id == node_id) return; // Skip invalid or self
    
    time_t now = time(NULL);
    
    // Find existing entry
    for (int i = 0; i < slot_table_size; i++) {
        if (neighbor_slots[i].node_id == neighbor_id) {
            neighbor_slots[i].reserved_slot = slot_number;
            neighbor_slots[i].last_updated = now;
            neighbor_slots[i].hop_distance = hop_distance;
            
            char node_str[16];
            if (slot_number >= 0) {
                printf("Updated slot reservation: Node %s (%d-hop) -> Slot %d\n", 
                       id_to_string(neighbor_id, node_str), hop_distance, slot_number);
            } else {
                printf("Cleared slot reservation: Node %s (%d-hop)\n", 
                       id_to_string(neighbor_id, node_str), hop_distance);
            }
            return;
        }
    }
    
    // Add new entry if space available and slot is valid
    if (slot_table_size < MAX_NEIGHBORS + MAX_TWO_HOP_NEIGHBORS && slot_number >= 0) {
        neighbor_slots[slot_table_size].node_id = neighbor_id;
        neighbor_slots[slot_table_size].reserved_slot = slot_number;
        neighbor_slots[slot_table_size].last_updated = now;
        neighbor_slots[slot_table_size].hop_distance = hop_distance;
        slot_table_size++;
        
        char node_str[16];
        printf("Added slot reservation: Node %s (%d-hop) -> Slot %d\n", 
               id_to_string(neighbor_id, node_str), hop_distance, slot_number);
    }
}

/**
 * @brief Get neighbor's TDMA slot reservation
 * @param node_id Neighbor's node ID
 * @return Slot number, or -1 if no reservation
 */
int get_neighbor_slot_reservation(uint32_t node_id) {
    for (int i = 0; i < slot_table_size; i++) {
        if (neighbor_slots[i].node_id == node_id) {
            return neighbor_slots[i].reserved_slot;
        }
    }
    return -1; // No reservation found
}

/**
 * @brief Check if a TDMA slot is available for use
 * @param slot_number Slot number to check
 * @return 1 if available, 0 if occupied by any neighbor (1-hop or 2-hop)
 */
int is_slot_available(int slot_number) {
    if (slot_number < 0) return 0;
    
    // Check if we are using this slot
    if (my_reserved_slot == slot_number) {
        return 0; // We are using it
    }
    
    // Check if any one-hop or two-hop neighbor is using this slot
    for (int i = 0; i < slot_table_size; i++) {
        if (neighbor_slots[i].reserved_slot == slot_number) {
            char node_str[16];
            printf("Slot %d is occupied by node %s (%d-hop)\n", 
                   slot_number, id_to_string(neighbor_slots[i].node_id, node_str),
                   neighbor_slots[i].hop_distance);
            return 0; // Slot occupied
        }
    }
    
    return 1; // Slot available
}

/**
 * @brief Get list of occupied slots in neighborhood
 * @param occupied_slots Array to store occupied slot numbers
 * @param max_slots Maximum slots to return
 * @return Number of occupied slots found
 */
int get_occupied_slots(int* occupied_slots, int max_slots) {
    int count = 0;
    
    // Add our own slot if we have one
    if (my_reserved_slot >= 0 && count < max_slots) {
        occupied_slots[count++] = my_reserved_slot;
    }
    
    // Add neighbor slots
    for (int i = 0; i < slot_table_size && count < max_slots; i++) {
        if (neighbor_slots[i].reserved_slot >= 0) {
            // Check if already in list (avoid duplicates)
            int already_added = 0;
            for (int j = 0; j < count; j++) {
                if (occupied_slots[j] == neighbor_slots[i].reserved_slot) {
                    already_added = 1;
                    break;
                }
            }
            if (!already_added) {
                occupied_slots[count++] = neighbor_slots[i].reserved_slot;
            }
        }
    }
    
    return count;
}

/**
 * @brief Print current TDMA slot reservations
 */
void print_tdma_reservations(void) {
    printf("\n=== TDMA Slot Reservations ===\n");
    printf("%-15s %-10s %-12s %-8s\n", "Node ID", "Slot", "Age (sec)", "Hops");
    printf("------------------------------------------\n");
    
    // Print our reservation first
    if (my_reserved_slot >= 0) {
        printf("%-15s %-10d %-12s %-8s\n", "THIS_NODE", my_reserved_slot, "N/A", "0");
    }
    
    // Print neighbor reservations
    time_t now = time(NULL);
    for (int i = 0; i < slot_table_size; i++) {
        if (neighbor_slots[i].reserved_slot >= 0) {
            char node_str[16];
            printf("%-15s %-10d %-12ld %-8d\n",
                   id_to_string(neighbor_slots[i].node_id, node_str),
                   neighbor_slots[i].reserved_slot,
                   (long)(now - neighbor_slots[i].last_updated),
                   neighbor_slots[i].hop_distance);
        }
    }
    
    printf("===============================\n\n");
}

/**
 * @brief Clean up expired slot reservations
 * @param max_age Maximum age in seconds before expiration
 */
void cleanup_expired_reservations(int max_age) {
    time_t now = time(NULL);
    int removed_count = 0;
    
    // Compact the array by removing expired entries
    int write_pos = 0;
    for (int read_pos = 0; read_pos < slot_table_size; read_pos++) {
        if ((now - neighbor_slots[read_pos].last_updated) <= max_age) {
            // Keep this entry
            if (write_pos != read_pos) {
                neighbor_slots[write_pos] = neighbor_slots[read_pos];
            }
            write_pos++;
        } else {
            // Remove this entry
            char node_str[16];
            printf("Expired slot reservation: Node %s (Slot %d, Age %ld sec)\n",
                   id_to_string(neighbor_slots[read_pos].node_id, node_str),
                   neighbor_slots[read_pos].reserved_slot,
                   (long)(now - neighbor_slots[read_pos].last_updated));
            removed_count++;
        }
    }
    
    slot_table_size = write_pos;
    
    if (removed_count > 0) {
        printf("Cleaned up %d expired TDMA reservations\n", removed_count);
    }
}

/**
 * @brief Check neighbor table for expired HELLO timeouts
 * 
 * Scans the neighbor table for neighbors that haven't sent HELLO messages
 * within the timeout period (6 seconds). Removes expired neighbors and
 * triggers link failure recovery.
 * 
 * @return Number of neighbors that failed timeout check
 */
int check_neighbor_timeouts(void) {
    time_t now = time(NULL);
    int failed_count = 0;
    int write_pos = 0;
    
    // Scan neighbor table for expired entries
    for (int read_pos = 0; read_pos < neighbor_count; read_pos++) {
        time_t time_since_hello = now - neighbor_table[read_pos].last_hello_time;
        
        if (time_since_hello > HELLO_TIMEOUT) {
            // Neighbor has timed out
            char neighbor_str[16];
            printf("LINK FAILURE DETECTED: Neighbor %s (timeout %ld sec > %d sec)\n",
                   id_to_string(neighbor_table[read_pos].neighbor_id, neighbor_str),
                   (long)time_since_hello, HELLO_TIMEOUT);
            
            // Handle the link failure
            handle_link_failure(neighbor_table[read_pos].neighbor_id);
            failed_count++;
            // Mark as failed in any pending messages
        } else {
            // Keep this neighbor (not expired)
            if (write_pos != read_pos) {
                neighbor_table[write_pos] = neighbor_table[read_pos];
            }
            write_pos++;
        }
    }
    
    // Update neighbor count after removals
    neighbor_count = write_pos;
    
    if (failed_count > 0) {
        printf("Removed %d failed neighbors from neighbor table\n", failed_count);
        printf("Topology changed, recalculating MPR set...\n");
        calculate_mpr_set();
        
        // Emergency HELLO will be generated by main loop when it detects the failure
        printf("Link failures detected - emergency HELLO will be triggered\n");
    }
    
    return failed_count;
}

/**
 * @brief Handle link failure for a specific neighbor
 * 
 * Processes the failure of a specific neighbor by cleaning up related
 * data structures and triggering topology updates.
 * 
 * @param neighbor_id ID of the failed neighbor
 */
void handle_link_failure(uint32_t neighbor_id) {
    char neighbor_str[16];
    printf("Processing link failure for neighbor %s\n", 
           id_to_string(neighbor_id, neighbor_str));
    
    // Remove from TDMA slot reservations
    update_neighbor_slot_reservation(neighbor_id, -1, 1);
    
    // Remove from two-hop neighbor information via MPR module
    remove_two_hop_via_neighbor(neighbor_id);
    
    // Mark as failed in any pending messages
    // This will be handled by the retry queue cleanup
    
    printf("Link failure cleanup completed for neighbor %s\n", neighbor_str);
}

/**
 * @brief Generate emergency HELLO message after topology change
 * 
 * Creates and queues an immediate HELLO message to inform neighbors
 * about topology changes, bypassing normal HELLO interval timing.
 * 
 * @param queue Pointer to the control queue
 * @return 0 on success, -1 on failure
 */
int generate_emergency_hello(struct control_queue* queue) {
    printf("Generating EMERGENCY HELLO due to topology change\n");

    if (!queue) {
        printf("Error: Control queue is NULL\n");
        return -1;
    }

    struct olsr_hello* hello_msg = generate_hello_message();
    if (!hello_msg) return -1;

    // Push pointer to the HELLO structure directly to the queue
    int result = push_to_control_queue(queue, MSG_HELLO, (void*)hello_msg);
    if (result == 0) {
        printf("Emergency HELLO successfully queued\n");
    } else {
        printf("ERROR: Failed to queue emergency HELLO\n");
        // Free the message if queueing failed
        free(hello_msg->neighbors);
        if (hello_msg->two_hop_neighbors) {
            free(hello_msg->two_hop_neighbors);
        }
        free(hello_msg);
    }
    return result;
}
