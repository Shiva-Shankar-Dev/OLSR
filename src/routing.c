/**
 * @file routing.c
 * @brief OLSR routing table management and shortest path calculation implementation
 * @author OLSR Implementation Team
 * @date 2025-10-01
 * 
 * This file implements the routing table management and shortest path calculation
 * for OLSR protocol using Dijkstra's algorithm.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "../include/olsr.h"
#include "../include/packet.h"
#include "../include/hello.h"
#include "../include/routing.h"

// Global topology database - always enabled
struct global_topology_entry {
    uint32_t from_node;
    uint32_t to_node;
    uint16_t ansn;
    time_t validity_time;
};

static struct duplicate_entry duplicate_table[MAX_DUPLICATE_ENTRIES];
static int duplicate_count = 0;
static struct global_topology_entry global_topology[MAX_TOPOLOGY_LINKS];
static int global_topology_count = 0;

// Global routing function implementations - always enabled
int is_duplicate_message(uint32_t originator, uint16_t seq_number) {
    for (int i = 0; i < duplicate_count; i++) {
        if (duplicate_table[i].originator == originator &&
            duplicate_table[i].seq_number == seq_number) {
            return 1;
        }
    }
    return 0;
}

int add_duplicate_entry(uint32_t originator, uint16_t seq_number) {
    if (duplicate_count >= MAX_DUPLICATE_ENTRIES) {
        return -1;
    }
    duplicate_table[duplicate_count].originator = originator;
    duplicate_table[duplicate_count].seq_number = seq_number;
    duplicate_table[duplicate_count].timestamp = time(NULL);
    duplicate_count++;
    return 0;
}

int add_topology_link(uint32_t from_node, uint32_t to_node, uint16_t ansn, time_t validity_time) {
    for (int i = 0; i < global_topology_count; i++) {
        if (global_topology[i].from_node == from_node &&
            global_topology[i].to_node == to_node) {
            if (ansn >= global_topology[i].ansn) {
                global_topology[i].ansn = ansn;
                global_topology[i].validity_time = validity_time;
                return 0;
            }
            return 0;
        }
    }
    
    if (global_topology_count < MAX_TOPOLOGY_LINKS) {
        global_topology[global_topology_count].from_node = from_node;
        global_topology[global_topology_count].to_node = to_node;
        global_topology[global_topology_count].ansn = ansn;
        global_topology[global_topology_count].validity_time = validity_time;
        global_topology_count++;
        return 0;
    }
    return -1;
}

int get_all_topology_links(struct topology_link* links, int max_links) {
    int count = 0;
    time_t now = time(NULL);
    
    for (int i = 0; i < global_topology_count && count < max_links; i++) {
        if (global_topology[i].validity_time > now) {
            links[count].from_id = global_topology[i].from_node;
            links[count].to_id = global_topology[i].to_node;
            links[count].cost = 1;
            links[count].validity = global_topology[i].validity_time;
            count++;
        }
    }
    return count;
}

int cleanup_topology_links(void) {
    time_t now = time(NULL);
    int cleaned = 0;
    int new_count = 0;
    
    for (int i = 0; i < global_topology_count; i++) {
        if (global_topology[i].validity_time > now) {
            if (new_count != i) {
                global_topology[new_count] = global_topology[i];
            }
            new_count++;
        } else {
            cleaned++;
        }
    }
    global_topology_count = new_count;
    return cleaned;
}

int cleanup_duplicate_table(void) {
    time_t now = time(NULL);
    int cleaned = 0;
    int new_count = 0;
    
    for (int i = 0; i < duplicate_count; i++) {
        if (now - duplicate_table[i].timestamp < DUPLICATE_HOLD_TIME) {
            if (new_count != i) {
                duplicate_table[new_count] = duplicate_table[i];
            }
            new_count++;
        } else {
            cleaned++;
        }
    }
    duplicate_count = new_count;
    return cleaned;
}

int should_forward_message(uint32_t sender_addr, uint32_t originator_addr) {
    (void)originator_addr;
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbor_table[i].neighbor_id == sender_addr &&
            neighbor_table[i].link_status == SYM_LINK &&
            neighbor_table[i].is_mpr_selector) {
            return 1;
        }
    }
    return 0;
}

int forward_tc_message(struct olsr_message* msg, uint32_t sender_addr, struct control_queue* queue) {
    (void)sender_addr;
    if (!msg || !queue || msg->ttl <= 1) {
        return -1;
    }
    
    struct olsr_tc* tc = (struct olsr_tc*)msg->body;
    
    // Create a decremented TTL copy of the message for forwarding
    msg->ttl--;
    msg->hop_count++;
    
    // Push the TC structure directly to the queue (no serialization needed)
    return push_to_control_queue(queue, MSG_TC, (void*)tc);
}

// External variables from other modules
extern uint32_t node_id;
extern struct neighbor_entry neighbor_table[];
extern int neighbor_count;

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

/** @brief Global routing table */
static struct routing_table_entry routing_table[MAX_ROUTING_ENTRIES];
/** @brief Current number of routing entries */
static int routing_table_size = 0;

/** @brief Topology information from TC messages */
static struct topology_link tc_topology[MAX_NODES * MAX_NODES];
/** @brief Number of links in TC topology */
static int tc_topology_size = 0;

/**
 * @brief Add or update a topology link from TC message (legacy compatibility)
 * @param from_id Source node ID
 * @param to_id Destination node ID  
 * @param validity Validity time
 * @return 0 on success, -1 if topology table is full
 * 
 * NOTE: This function maintains the legacy tc_topology array for backward
 * compatibility. New code should use the global topology database directly.
 */
int update_tc_topology(uint32_t from_id, uint32_t to_id, time_t validity) {
    if (tc_topology_size >= MAX_NODES * MAX_NODES) {
        return -1;  // Topology table full
    }
    
    tc_topology[tc_topology_size].from_id = from_id;
    tc_topology[tc_topology_size].to_id = to_id;
    tc_topology[tc_topology_size].cost = 1;  // Standard OLSR cost
    tc_topology[tc_topology_size].validity = validity;
    tc_topology_size++;
    
    char from_str[16], to_str[16];
    printf("Legacy TC topology: %s -> %s (validity=%lds)\n",
           id_to_string(from_id, from_str),
           id_to_string(to_id, to_str),
           (long)(validity - time(NULL)));
    
    return 0;
}

/**
 * @brief Remove expired TC topology links
 */
void cleanup_tc_topology(void) {
    time_t now = time(NULL);
    int i = 0;
    
    while (i < tc_topology_size) {
        if (tc_topology[i].validity <= now) {
            // Remove expired link by shifting remaining entries
            char from_str[16], to_str[16];
            printf("Removing expired TC link: %s -> %s\n",
                   id_to_string(tc_topology[i].from_id, from_str),
                   id_to_string(tc_topology[i].to_id, to_str));
            
            for (int j = i; j < tc_topology_size - 1; j++) {
                tc_topology[j] = tc_topology[j + 1];
            }
            tc_topology_size--;
        } else {
            i++;
        }
    }
}

/**
 * @brief Find minimum distance vertex not yet processed
 */
static int find_min_distance(int* dist, int* sptSet, int node_count) {
    int min = INFINITE_COST;
    int min_index = -1;
    
    for (int v = 0; v < node_count; v++) {
        if (sptSet[v] == 0 && dist[v] <= min) {
            min = dist[v];
            min_index = v;
        }
    }
    return min_index;
}

/**
 * @brief Find index of node in node array
 */
static int find_node_index(uint32_t* nodes, int node_count, uint32_t target_id) {
    for (int i = 0; i < node_count; i++) {
        if (nodes[i] == target_id) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Build complete topology graph from neighbor table and global topology database
 * 
 * Enhanced version that uses the global topology database from TC messages
 * for comprehensive multi-hop routing calculation.
 */
int build_topology_graph(struct topology_link* topology, int max_links) {
    int link_count = 0;
    time_t now = time(NULL);
    
    printf("\n=== BUILDING COMPLETE TOPOLOGY GRAPH ===\n");
    
    // Step 1: Add direct neighbor links (1-hop)
    int direct_links = 0;
    for (int i = 0; i < neighbor_count && link_count < max_links; i++) {
        if (neighbor_table[i].link_status == SYM_LINK) {
            topology[link_count].from_id = node_id;
            topology[link_count].to_id = neighbor_table[i].neighbor_id;
            topology[link_count].cost = 1;  // Standard OLSR cost
            topology[link_count].validity = neighbor_table[i].last_seen + 10;
            link_count++;
            direct_links++;
            
            char node_str[16], neighbor_str[16];
            printf("Direct link: %s -> %s (cost=1)\n",
                   id_to_string(node_id, node_str),
                   id_to_string(neighbor_table[i].neighbor_id, neighbor_str));
        }
    }
    
    // Step 2: Clean up expired topology links
    cleanup_topology_links();
    
    // Step 3: Add all valid topology links from global database (multi-hop)
    struct topology_link global_links[MAX_TOPOLOGY_LINKS];
    int global_count = get_all_topology_links(global_links, MAX_TOPOLOGY_LINKS);
    
    if (global_count > 0) {
        printf("Using global topology database with %d links\n", global_count);
    } else {
        printf("No global topology links available\n");
    }
    
    int tc_links_added = 0;
    for (int i = 0; i < global_count && link_count < max_links; i++) {
        // Avoid duplicate links (check if already added as direct link)
        int is_duplicate = 0;
        for (int j = 0; j < link_count; j++) {
            if (topology[j].from_id == global_links[i].from_id &&
                topology[j].to_id == global_links[i].to_id) {
                is_duplicate = 1;
                break;
            }
        }
        
        if (!is_duplicate) {
            topology[link_count].from_id = global_links[i].from_id;
            topology[link_count].to_id = global_links[i].to_id;
            topology[link_count].cost = 1;  // Standard OLSR cost
            topology[link_count].validity = global_links[i].validity;
            link_count++;
            tc_links_added++;
            
            char from_str[16], to_str[16];
            printf("Global link: %s -> %s (cost=1)\n",
                   id_to_string(global_links[i].from_id, from_str),
                   id_to_string(global_links[i].to_id, to_str));
        }
    }
    
    // Step 4: Also add legacy TC topology for backward compatibility
    cleanup_tc_topology();
    int legacy_tc_added = 0;
    for (int i = 0; i < tc_topology_size && link_count < max_links; i++) {
        if (tc_topology[i].validity > now) {
            // Check if this link is already in the topology
            int is_duplicate = 0;
            for (int j = 0; j < link_count; j++) {
                if (topology[j].from_id == tc_topology[i].from_id &&
                    topology[j].to_id == tc_topology[i].to_id) {
                    is_duplicate = 1;
                    break;
                }
            }
            
            if (!is_duplicate) {
                topology[link_count] = tc_topology[i];
                link_count++;
                legacy_tc_added++;
            }
        }
    }
    
    printf("\nTopology Summary:\n");
    printf("  Direct neighbors: %d\n", direct_links);
    printf("  Global TC links:  %d\n", tc_links_added);
    printf("  Legacy TC links:  %d\n", legacy_tc_added);
    printf("  Total links:      %d\n", link_count);
    printf("=== TOPOLOGY GRAPH COMPLETE ===\n\n");
    
    return link_count;
}

/**
 * @brief Apply Dijkstra's algorithm for shortest path calculation
 */
void dijkstra_shortest_path(uint32_t source, struct topology_link* topology, int link_count) {
    // Build list of unique nodes
    uint32_t nodes[MAX_NODES];
    int node_count = 0;
    
    // Add source node
    nodes[node_count++] = source;
    
    // Add all nodes from topology links
    for (int i = 0; i < link_count && node_count < MAX_NODES; i++) {
        if (-1 == find_node_index(nodes, node_count, topology[i].from_id)) {
            nodes[node_count++] = topology[i].from_id;
        }
        if (-1 == find_node_index(nodes, node_count, topology[i].to_id)) {
            nodes[node_count++] = topology[i].to_id;
        }
    }
    
    printf("Dijkstra: Found %d unique nodes in topology\n", node_count);
    
    // Initialize arrays
    int dist[MAX_NODES];
    int sptSet[MAX_NODES];
    uint32_t parent[MAX_NODES];
    
    for (int i = 0; i < node_count; i++) {
        dist[i] = INFINITE_COST;
        sptSet[i] = 0;
        parent[i] = 0;
    }
    // Find source index
    int src_index = find_node_index(nodes, node_count, source);
    if (-1 == src_index) {
        printf("Error: Source node not found in topology\n");
        return;
    }
    
    dist[src_index] = 0;
    
    // Main Dijkstra loop
    for (int count = 0; count < node_count - 1; count++) {
        int u = find_min_distance(dist, sptSet, node_count);
        if (-1 == u) break;
        
        sptSet[u] = 1;
        
        // Update distances to adjacent nodes
        for (int i = 0; i < link_count; i++) {
            if (topology[i].from_id == nodes[u]) {
                int v = find_node_index(nodes, node_count, topology[i].to_id);
                if (v != -1 && !sptSet[v] && dist[u] != INFINITE_COST) {
                    int new_dist = dist[u] + topology[i].cost;
                    if (new_dist < dist[v]) {
                        dist[v] = new_dist;
                        parent[v] = nodes[u];
                    }
                }
            }
        }
    }
    
    // Update routing table with results
    clear_routing_table();
    
    for (int i = 0; i < node_count; i++) {
        if (nodes[i] != source && dist[i] != INFINITE_COST) {
            // Trace back to find next hop
            uint32_t next_hop = nodes[i];
            uint32_t current = nodes[i];
            
            while (parent[find_node_index(nodes, node_count, current)] != source &&
                   parent[find_node_index(nodes, node_count, current)] != 0) {
                current = parent[find_node_index(nodes, node_count, current)];
                next_hop = current;
            }
            
            if (parent[find_node_index(nodes, node_count, current)] == source) {
                next_hop = current;
            }
            
            add_routing_entry(nodes[i], next_hop, dist[i], dist[i]);
        }
    }
}

/**
 * @brief Calculate routing table using complete network topology and Dijkstra's algorithm
 * 
 * Enhanced version that uses both direct neighbors and global topology database
 * to calculate optimal routes to all reachable destinations in the network.
 */
void calculate_routing_table(void) {
    if (node_id == 0) {
        printf("Error: Node ID not set for routing calculation\n");
        return;
    }
    
    printf("\n=== CALCULATING ROUTING TABLE ===\n");
    char node_str[16];
    printf("Source node: %s\n", id_to_string(node_id, node_str));
    
    // Build complete network topology
    struct topology_link topology[MAX_NODES * MAX_NODES];
    int link_count = build_topology_graph(topology, MAX_NODES * MAX_NODES);
    
    if (link_count > 0) {
        printf("Running Dijkstra with %d topology links...\n", link_count);
        dijkstra_shortest_path(node_id, topology, link_count);
        print_routing_table();
    } else {
        clear_routing_table();
        printf("No topology links found - network disconnected or no neighbors\n");
    }
    
    printf("=== ROUTING CALCULATION COMPLETE ===\n\n");
}

/**
 * @brief Add or update routing table entry
 */
int add_routing_entry(uint32_t dest_id, uint32_t next_hop_id, uint32_t metric, int hops) {
    // Check if entry already exists
    for (int i = 0; i < routing_table_size; i++) {
        if (routing_table[i].dest_id == dest_id) {
            // Update existing entry
            routing_table[i].next_hop_id = next_hop_id;
            routing_table[i].metric = metric;
            routing_table[i].hops = hops;
            routing_table[i].timestamp = time(NULL);
            
            char dest_str[16], next_hop_str[16];
            printf("Updated route: %s via %s (cost=%u, hops=%d)\n",
                   id_to_string(dest_id, dest_str),
                   id_to_string(next_hop_id, next_hop_str),
                   metric, hops);
            return 0;
        }
    }
    
    // Add new entry if space available
    if (routing_table_size < MAX_ROUTING_ENTRIES) {
        routing_table[routing_table_size].dest_id = dest_id;
        routing_table[routing_table_size].next_hop_id = next_hop_id;
        routing_table[routing_table_size].metric = metric;
        routing_table[routing_table_size].hops = hops;
        routing_table[routing_table_size].timestamp = time(NULL);
        routing_table_size++;
        
        char dest_str[16], next_hop_str[16];
        printf("Added route: %s via %s (cost=%u, hops=%d)\n",
               id_to_string(dest_id, dest_str),
               id_to_string(next_hop_id, next_hop_str),
               metric, hops);
        return 0;
    }
    
    return -1;  // Table full
}

/**
 * @brief Print routing table
 */
void print_routing_table(void) {
    printf("\n=== Routing Table ===\n");
    printf("%-15s %-15s %-8s %-8s %-8s\n", "Destination", "Next Hop", "Cost", "Hops", "Age(s)");
    printf("---------------------------------------------------------------\n");
    
    time_t now = time(NULL);
    for (int i = 0; i < routing_table_size; i++) {
        char dest_str[16], next_hop_str[16];
        printf("%-15s %-15s %-8u %-8d %-8ld\n",
               id_to_string(routing_table[i].dest_id, dest_str),
               id_to_string(routing_table[i].next_hop_id, next_hop_str),
               routing_table[i].metric,
               routing_table[i].hops,
               (long)(now - routing_table[i].timestamp));
    }
    printf("Total entries: %d\n\n", routing_table_size);
}

/**
 * @brief Clear routing table
 */
void clear_routing_table(void) {
    routing_table_size = 0;
    memset(routing_table, 0, sizeof(routing_table));
    printf("Routing table cleared\n");
}

/**
 * @brief Update routing table in response to topology changes
 * 
 * This function is called whenever the network topology changes due to:
 * - New neighbor discovery
 * - Neighbor timeout/failure  
 * - Receipt of TC messages with new topology information
 */
void update_routing_table(void) {
    printf("ROUTING_UPDATE: Topology changed - recalculating routes\n");
    calculate_routing_table();
}

/**
 * @brief Get next hop information for a destination node
 * @param dest_id Destination node ID (MAC/TDMA identifier)
 * @param next_hop_id Pointer to store the next hop node ID
 * @param metric Pointer to store the route metric/cost
 * @param hops Pointer to store the number of hops
 * @return 0 on success, -1 if route not found
 */
int get_next_hop(uint32_t dest_id, uint32_t* next_hop_id, uint32_t* metric, int* hops) {
    if (!next_hop_id || !metric || !hops) {
        printf("Error: NULL pointer passed to get_next_hop\n");
        return -1;
    }
    
    // Search for the destination in routing table
    for (int i = 0; i < routing_table_size; i++) {
        if (routing_table[i].dest_id == dest_id) {
            *next_hop_id = routing_table[i].next_hop_id;
            *metric = routing_table[i].metric;
            *hops = routing_table[i].hops;
            
            char dest_str[16], next_hop_str[16];
            printf("Route found: %s via %s (cost=%u, hops=%d)\n",
                   id_to_string(dest_id, dest_str),
                   id_to_string(*next_hop_id, next_hop_str),
                   *metric, *hops);
            return 0;
        }
    }
    
    // Route not found
    char dest_str[16];
    printf("No route found to destination: %s\n", id_to_string(dest_id, dest_str));
    return -1;
}

/**
 * @brief Check if a route exists to the destination
 * @param dest_id Destination node ID (MAC/TDMA identifier)
 * @return 1 if route exists, 0 otherwise
 */
int has_route_to(uint32_t dest_id) {
    for (int i = 0; i < routing_table_size; i++) {
        if (routing_table[i].dest_id == dest_id) {
            return 1;  // Route exists
        }
    }
    return 0;  // No route found
}

/**
 * @brief Get routing table entry by destination
 * @param dest_id Destination node ID (MAC/TDMA identifier)
 * @return Pointer to routing entry or NULL if not found
 */
struct routing_table_entry* get_routing_entry(uint32_t dest_id) {
    for (int i = 0; i < routing_table_size; i++) {
        if (routing_table[i].dest_id == dest_id) {
            return &routing_table[i];  // Return pointer to entry
        }
    }
    return NULL;  // Entry not found
}