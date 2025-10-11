/**
 * @file routing.h
 * @brief OLSR routing table management and shortest path calculation
 * @author OLSR Implementation Team
 * @date 2025-10-01
 */

#ifndef ROUTING_H
#define ROUTING_H

#include <stdint.h>
#include <limits.h>
#include <time.h>

#define MAX_ROUTING_ENTRIES 100  /**< Maximum entries in routing table */
#define INFINITE_COST INT_MAX    /**< Infinite cost for unreachable nodes */
#define MAX_NODES 50            /**< Maximum nodes in topology */

/**
 * @brief Routing table entry structure
 * 
 * Represents a single entry in the OLSR routing table containing
 * destination, next hop, and cost information using application layer node IDs.
 */
struct routing_table_entry {
    uint32_t dest_id;    /**< Destination node ID (MAC/TDMA identifier) */
    uint32_t next_hop_id; /**< Next hop node ID (MAC/TDMA identifier) */
    uint32_t metric;     /**< Cost/distance to destination */
    int hops;           /**< Number of hops to destination */
    time_t timestamp;   /**< When this entry was last updated */
};

/**
 * @brief Topology link structure
 * 
 * Represents a link in the network topology graph used for
 * shortest path calculation using application layer node IDs.
 */
struct topology_link {
    uint32_t from_id;    /**< Source node ID (MAC/TDMA identifier) */
    uint32_t to_id;      /**< Destination node ID (MAC/TDMA identifier) */
    int cost;           /**< Link cost (usually 1 for OLSR) */
    time_t validity;    /**< When this link expires */
};

/**
 * @brief Calculate routing table using shortest path algorithm
 * 
 * Builds network topology from neighbor table and TC messages,
 * then applies Dijkstra's algorithm to find shortest paths.
 */
void calculate_routing_table(void);

/**
 * @brief Add entry to routing table
 * @param dest_id Destination node ID (MAC/TDMA identifier)
 * @param next_hop_id Next hop node ID (MAC/TDMA identifier)
 * @param metric Cost to destination
 * @param hops Number of hops to destination
 * @return 0 on success, -1 on failure
 */
int add_routing_entry(uint32_t dest_id, uint32_t next_hop_id, uint32_t metric, int hops);

/**
 * @brief Print current routing table
 */
void print_routing_table(void);

/**
 * @brief Find shortest path using Dijkstra's algorithm
 * @param source Source node ID (MAC/TDMA identifier)
 * @param topology Array of topology links
 * @param link_count Number of links in topology
 */
void dijkstra_shortest_path(uint32_t source, struct topology_link* topology, int link_count);

/**
 * @brief Build topology graph from neighbor and TC information
 * @param topology Output array for topology links
 * @param max_links Maximum number of links to store
 * @return Number of links found
 */
int build_topology_graph(struct topology_link* topology, int max_links);

/**
 * @brief Clear and reinitialize routing table
 */
void clear_routing_table(void);

/**
 * @brief Update routing table with new topology information
 */
void update_routing_table(void);

/**
 * @brief Add or update a topology link from TC message
 * @param from_id Source node ID (MAC/TDMA identifier)
 * @param to_id Destination node ID (MAC/TDMA identifier)
 * @param validity Validity time
 * @return 0 on success, -1 if topology table is full
 */
int update_tc_topology(uint32_t from_id, uint32_t to_id, time_t validity);

/**
 * @brief Remove expired TC topology links
 */
void cleanup_tc_topology(void);

/**
 * @brief Get next hop information for a destination node
 * @param dest_id Destination node ID (MAC/TDMA identifier)
 * @param next_hop_id Pointer to store the next hop node ID
 * @param metric Pointer to store the route metric/cost
 * @param hops Pointer to store the number of hops
 * @return 0 on success, -1 if route not found
 */
int get_next_hop(uint32_t dest_id, uint32_t* next_hop_id, uint32_t* metric, int* hops);

/**
 * @brief Check if a route exists to the destination
 * @param dest_id Destination node ID (MAC/TDMA identifier)
 * @return 1 if route exists, 0 otherwise
 */
int has_route_to(uint32_t dest_id);

/**
 * @brief Get routing table entry by destination
 * @param dest_id Destination node ID (MAC/TDMA identifier)
 * @return Pointer to routing entry or NULL if not found
 */
struct routing_table_entry* get_routing_entry(uint32_t dest_id);

#endif // ROUTING_H