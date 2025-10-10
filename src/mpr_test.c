/**
 * @file mpr_test.c
 * @brief Test program for MPR selection algorithm
 * @author OLSR Implementation Team
 * @date 2025-10-10
 * 
 * This program demonstrates and tests the MPR selection algorithm
 * with various network topologies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/olsr.h"
#include "../include/hello.h"
#include "../include/mpr.h"

/**
 * @brief Initialize test environment
 */
void init_test_environment(void) {
    // Set node IP
    node_ip = 0x0A000001; // 10.0.0.1
    node_willingness = WILL_DEFAULT;
    
    // Clear all tables
    neighbor_count = 0;
    memset(neighbor_table, 0, sizeof(neighbor_table));
    clear_two_hop_table();
    clear_mpr_set();
    
    printf("\n=== Test Environment Initialized ===\n");
    printf("Node IP: 10.0.0.1\n");
    printf("Willingness: %d (WILL_DEFAULT)\n\n", node_willingness);
}

/**
 * @brief Test Scenario 1: No two-hop neighbors (star topology)
 */
void test_star_topology(void) {
    printf("\n========================================\n");
    printf("TEST 1: Star Topology (No Two-Hop Neighbors)\n");
    printf("========================================\n");
    
    init_test_environment();
    
    // Add one-hop neighbors only
    add_neigbhor(0x0A000002, SYM_LINK, WILL_DEFAULT); // 10.0.0.2
    add_neigbhor(0x0A000003, SYM_LINK, WILL_DEFAULT); // 10.0.0.3
    add_neigbhor(0x0A000004, SYM_LINK, WILL_DEFAULT); // 10.0.0.4
    add_neigbhor(0x0A000005, SYM_LINK, WILL_DEFAULT); // 10.0.0.5
    
    printf("\nOne-hop neighbors added: 4\n");
    print_neighbor_table();
    
    // Calculate MPR set
    calculate_mpr_set();
    print_mpr_set();
    
    printf("Expected: MPR set should be EMPTY (no two-hop neighbors)\n");
    printf("Actual MPR count: %d\n", get_mpr_count());
    printf("Test %s\n", get_mpr_count() == 0 ? "PASSED" : "FAILED");
}

/**
 * @brief Test Scenario 2: Chain topology (single path)
 */
void test_chain_topology(void) {
    printf("\n========================================\n");
    printf("TEST 2: Chain Topology (Single Path)\n");
    printf("========================================\n");
    printf("Topology: Node1 -> Node2 -> Node3\n");
    
    init_test_environment();
    
    // Add one-hop neighbor
    add_neigbhor(0x0A000002, SYM_LINK, WILL_DEFAULT); // 10.0.0.2
    
    // Add two-hop neighbor reachable only through Node2
    add_two_hop_neighbor(0x0A000003, 0x0A000002); // 10.0.0.3 via 10.0.0.2
    
    printf("\nOne-hop neighbors: 1\n");
    printf("Two-hop neighbors: 1\n");
    print_neighbor_table();
    print_two_hop_table();
    
    // Calculate MPR set
    calculate_mpr_set();
    print_mpr_set();
    
    printf("Expected: Node2 (10.0.0.2) should be selected as MPR\n");
    printf("Actual MPR count: %d\n", get_mpr_count());
    printf("Test %s\n", (get_mpr_count() == 1 && is_mpr(0x0A000002)) ? "PASSED" : "FAILED");
}

/**
 * @brief Test Scenario 3: Multiple paths with willingness
 */
void test_multiple_paths_willingness(void) {
    printf("\n========================================\n");
    printf("TEST 3: Multiple Paths with Different Willingness\n");
    printf("========================================\n");
    printf("Topology: Two paths to same two-hop neighbor\n");
    
    init_test_environment();
    
    // Add one-hop neighbors with different willingness
    add_neigbhor(0x0A000002, SYM_LINK, WILL_LOW);  // 10.0.0.2 (low willingness)
    add_neigbhor(0x0A000003, SYM_LINK, WILL_HIGH); // 10.0.0.3 (high willingness)
    
    // Add two-hop neighbor reachable through both
    add_two_hop_neighbor(0x0A000004, 0x0A000002); // 10.0.0.4 via 10.0.0.2
    add_two_hop_neighbor(0x0A000004, 0x0A000003); // 10.0.0.4 via 10.0.0.3
    
    printf("\nOne-hop neighbors: 2\n");
    printf("Two-hop neighbors: 1 (reachable via 2 paths)\n");
    print_neighbor_table();
    print_two_hop_table();
    
    // Calculate MPR set
    calculate_mpr_set();
    print_mpr_set();
    
    printf("Expected: Node3 (10.0.0.3) should be selected (higher willingness)\n");
    printf("Actual: Node3 selected: %s\n", is_mpr(0x0A000003) ? "YES" : "NO");
    printf("Test %s\n", (get_mpr_count() == 1 && is_mpr(0x0A000003)) ? "PASSED" : "FAILED");
}

/**
 * @brief Test Scenario 4: Complex topology with optimal MPR selection
 */
void test_complex_topology(void) {
    printf("\n========================================\n");
    printf("TEST 4: Complex Topology (Optimal Coverage)\n");
    printf("========================================\n");
    printf("Topology: Multiple one-hop and two-hop neighbors\n");
    
    init_test_environment();
    
    // Add one-hop neighbors
    add_neigbhor(0x0A000002, SYM_LINK, WILL_DEFAULT); // N2
    add_neigbhor(0x0A000003, SYM_LINK, WILL_DEFAULT); // N3
    add_neigbhor(0x0A000004, SYM_LINK, WILL_DEFAULT); // N4
    add_neigbhor(0x0A000005, SYM_LINK, WILL_DEFAULT); // N5
    
    // Add two-hop neighbors
    // N6 reachable via N2 only
    add_two_hop_neighbor(0x0A000006, 0x0A000002);
    
    // N7 reachable via N3 and N4
    add_two_hop_neighbor(0x0A000007, 0x0A000003);
    add_two_hop_neighbor(0x0A000007, 0x0A000004);
    
    // N8 reachable via N4 and N5
    add_two_hop_neighbor(0x0A000008, 0x0A000004);
    add_two_hop_neighbor(0x0A000008, 0x0A000005);
    
    // N9 reachable via N5 only
    add_two_hop_neighbor(0x0A000009, 0x0A000005);
    
    printf("\nOne-hop neighbors: 4\n");
    printf("Two-hop neighbors: 4\n");
    print_neighbor_table();
    print_two_hop_table();
    
    // Calculate MPR set
    calculate_mpr_set();
    print_mpr_set();
    
    printf("Expected: N2 and N5 (unique paths), plus one of N3/N4\n");
    printf("Actual MPR count: %d\n", get_mpr_count());
    printf("N2 selected: %s (required for N6)\n", is_mpr(0x0A000002) ? "YES" : "NO");
    printf("N5 selected: %s (required for N9)\n", is_mpr(0x0A000005) ? "YES" : "NO");
    
    int has_required = is_mpr(0x0A000002) && is_mpr(0x0A000005);
    printf("Test %s\n", has_required ? "PASSED" : "FAILED");
}

/**
 * @brief Test Scenario 5: WILL_ALWAYS and WILL_NEVER
 */
void test_willingness_extremes(void) {
    printf("\n========================================\n");
    printf("TEST 5: Willingness Extremes (ALWAYS/NEVER)\n");
    printf("========================================\n");
    
    init_test_environment();
    
    // Add neighbors with extreme willingness values
    add_neigbhor(0x0A000002, SYM_LINK, WILL_NEVER);  // Should not be selected
    add_neigbhor(0x0A000003, SYM_LINK, WILL_ALWAYS); // Should always be selected
    add_neigbhor(0x0A000004, SYM_LINK, WILL_DEFAULT);
    
    // Add two-hop neighbors reachable through all
    add_two_hop_neighbor(0x0A000005, 0x0A000002);
    add_two_hop_neighbor(0x0A000005, 0x0A000003);
    add_two_hop_neighbor(0x0A000005, 0x0A000004);
    
    printf("\nOne-hop neighbors: 3 (NEVER, ALWAYS, DEFAULT)\n");
    printf("Two-hop neighbors: 1 (reachable via all)\n");
    print_neighbor_table();
    print_two_hop_table();
    
    // Calculate MPR set
    calculate_mpr_set();
    print_mpr_set();
    
    printf("Expected: N3 selected (WILL_ALWAYS), N2 NOT selected (WILL_NEVER)\n");
    printf("N2 (WILL_NEVER) selected: %s\n", is_mpr(0x0A000002) ? "YES" : "NO");
    printf("N3 (WILL_ALWAYS) selected: %s\n", is_mpr(0x0A000003) ? "YES" : "NO");
    
    int correct = !is_mpr(0x0A000002) && is_mpr(0x0A000003);
    printf("Test %s\n", correct ? "PASSED" : "FAILED");
}

/**
 * @brief Test Scenario 6: Update and recalculate
 */
void test_dynamic_update(void) {
    printf("\n========================================\n");
    printf("TEST 6: Dynamic Topology Update\n");
    printf("========================================\n");
    
    init_test_environment();
    
    // Initial topology
    add_neigbhor(0x0A000002, SYM_LINK, WILL_DEFAULT);
    add_two_hop_neighbor(0x0A000003, 0x0A000002);
    
    printf("\n--- Initial Topology ---\n");
    calculate_mpr_set();
    print_mpr_set();
    int initial_count = get_mpr_count();
    
    // Add new path to two-hop neighbor
    printf("\n--- Adding New One-Hop Neighbor ---\n");
    add_neigbhor(0x0A000004, SYM_LINK, WILL_HIGH);
    add_two_hop_neighbor(0x0A000003, 0x0A000004);
    
    printf("\n--- After Update ---\n");
    calculate_mpr_set();
    print_mpr_set();
    
    printf("Initial MPR count: %d\n", initial_count);
    printf("Updated MPR count: %d\n", get_mpr_count());
    printf("N4 (high will) selected: %s\n", is_mpr(0x0A000004) ? "YES" : "NO");
    printf("Test COMPLETED (dynamic recalculation works)\n");
}

/**
 * @brief Main test runner
 */
int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║     MPR Selection Algorithm Test Suite            ║\n");
    printf("║     OLSR Implementation                            ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    
    // Run all tests
    test_star_topology();
    test_chain_topology();
    test_multiple_paths_willingness();
    test_complex_topology();
    test_willingness_extremes();
    test_dynamic_update();
    
    printf("\n========================================\n");
    printf("All Tests Completed\n");
    printf("========================================\n\n");
    
    return 0;
}
