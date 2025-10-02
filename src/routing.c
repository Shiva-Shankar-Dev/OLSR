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
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include "../include/olsr.h"
#include "../include/packet.h"
#include "../include/hello.h"
#include "../include/routing.h"

/** @brief Global routing table */
static struct routing_table_entry routing_table[MAX_ROUTING_ENTRIES];
/** @brief Current number of routing entries */
static int routing_table_size = 0;

/**
 * @brief Find minimum distance vertex not yet processed
 */

static int find_min_distance(int* dist, int* sptSet, uint32_t* nodes, int node_count) {
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
static int find_node_index(uint32_t* nodes, int node_count, uint32_t target_ip) {
    for (int i = 0; i < node_count; i++) {
        if (nodes[i] == target_ip) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Build topology graph from neighbor table
 */
int build_topology_graph(struct topology_link* topology, int max_links) {
    int link_count = 0;
    
    // Add direct neighbor links from neighbor table
    for (int i = 0; i < neighbor_count && link_count < max_links; i++) {
        if (neighbor_table[i].link_status == SYM_LINK) {
            topology[link_count].from_addr = node_ip;
            topology[link_count].to_addr = neighbor_table[i].neighbor_addr;
            topology[link_count].cost = 1;  // Standard OLSR cost
            topology[link_count].validity = neighbor_table[i].last_seen + 10;
            link_count++;
            
            printf("Added topology link: %s -> %s (cost=1)\n",
                   inet_ntoa(*(struct in_addr*)&node_ip),
                   inet_ntoa(*(struct in_addr*)&neighbor_table[i].neighbor_addr));
        }
    }
    
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
        if (find_node_index(nodes, node_count, topology[i].from_addr) == -1) {
            nodes[node_count++] = topology[i].from_addr;
        }
        if (find_node_index(nodes, node_count, topology[i].to_addr) == -1) {
            nodes[node_count++] = topology[i].to_addr;
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
    if (src_index == -1) {
        printf("Error: Source node not found in topology\n");
        return;
    }
    
    dist[src_index] = 0;
    
    // Main Dijkstra loop
    for (int count = 0; count < node_count - 1; count++) {
        int u = find_min_distance(dist, sptSet, nodes, node_count);
        if (u == -1) break;
        
        sptSet[u] = 1;
        
        // Update distances to adjacent nodes
        for (int i = 0; i < link_count; i++) {
            if (topology[i].from_addr == nodes[u]) {
                int v = find_node_index(nodes, node_count, topology[i].to_addr);
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
 * @brief Calculate routing table using shortest path algorithm
 */
void calculate_routing_table(void) {
    printf("=== Calculating OLSR Routing Table ===\n");
    
    if (node_ip == 0) {
        printf("Error: Node IP not set\n");
        return;
    }
    
    struct topology_link topology[MAX_NODES * MAX_NODES];
    int link_count = build_topology_graph(topology, MAX_NODES * MAX_NODES);
    
    printf("Built topology graph with %d links\n", link_count);
    
    if (link_count > 0) {
        dijkstra_shortest_path(node_ip, topology, link_count);
        printf("Shortest path calculation completed\n");
        print_routing_table();
    } else {
        printf("No topology links available - clearing routing table\n");
        clear_routing_table();
    }
}

/**
 * @brief Add entry to routing table
 */
int add_routing_entry(uint32_t dest_ip, uint32_t next_hop, uint32_t metric, int hops) {
    if (routing_table_size >= MAX_ROUTING_ENTRIES) {
        printf("Error: Routing table full\n");
        return -1;
    }
    
    // Check if entry already exists
    for (int i = 0; i < routing_table_size; i++) {
        if (routing_table[i].dest_ip == dest_ip) {
            // Update existing entry
            routing_table[i].next_hop = next_hop;
            routing_table[i].metric = metric;
            routing_table[i].hops = hops;
            routing_table[i].timestamp = time(NULL);
            printf("Updated routing entry: %s via %s (cost=%d, hops=%d)\n",
                   inet_ntoa(*(struct in_addr*)&dest_ip),
                   inet_ntoa(*(struct in_addr*)&next_hop),
                   metric, hops);
            return 0;
        }
    }
    
    // Add new entry
    routing_table[routing_table_size].dest_ip = dest_ip;
    routing_table[routing_table_size].next_hop = next_hop;
    routing_table[routing_table_size].metric = metric;
    routing_table[routing_table_size].hops = hops;
    routing_table[routing_table_size].timestamp = time(NULL);
    routing_table_size++;
    
    printf("Added routing entry: %s via %s (cost=%d, hops=%d)\n",
           inet_ntoa(*(struct in_addr*)&dest_ip),
           inet_ntoa(*(struct in_addr*)&next_hop),
           metric, hops);
    
    return 0;
}

/**
 * @brief Print current routing table
 */
void print_routing_table(void) {
    printf("\n=== OLSR Routing Table ===\n");
    printf("Destination      Next Hop         Cost  Hops  Age\n");
    printf("------------------------------------------------\n");
    
    if (routing_table_size == 0) {
        printf("(empty)\n");
        return;
    }
    
    time_t now = time(NULL);
    for (int i = 0; i < routing_table_size; i++) {
        int age = (int)(now - routing_table[i].timestamp);
        printf("%-15s  %-15s  %4d  %4d  %3ds\n",
               inet_ntoa(*(struct in_addr*)&routing_table[i].dest_ip),
               inet_ntoa(*(struct in_addr*)&routing_table[i].next_hop),
               routing_table[i].metric,
               routing_table[i].hops,
               age);
    }
    printf("\n");
}

/**
 * @brief Clear and reinitialize routing table
 */
void clear_routing_table(void) {
    routing_table_size = 0;
    memset(routing_table, 0, sizeof(routing_table));
}

/**
 * @brief Update routing table with new topology information
 */
void update_routing_table(void) {
    printf("Updating routing table...\n");
    calculate_routing_table();
}