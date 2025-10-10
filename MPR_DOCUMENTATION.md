# MPR (Multipoint Relay) Implementation for OLSR

## Overview

This document explains the MPR (Multipoint Relay) selection algorithm implementation for the OLSR (Optimized Link State Routing) protocol. The MPR mechanism is a key optimization in OLSR that reduces flooding overhead in broadcast operations.

## What is MPR?

MPR (Multipoint Relay) is a technique used in OLSR to reduce redundant retransmissions in flooding operations. Instead of having every node retransmit broadcast messages, only selected MPR nodes retransmit, ensuring all two-hop neighbors still receive the message.

### Key Benefits:
- **Reduces network flooding**: Only MPR nodes forward broadcast messages
- **Maintains coverage**: All two-hop neighbors are still reachable
- **Optimizes bandwidth**: Fewer redundant transmissions
- **Scalable**: Works efficiently in dense networks

## Architecture

### Data Structures

#### 1. Two-Hop Neighbor Structure
```c
struct two_hop_neighbor {
    uint32_t neighbor_addr;      // IP address of the two-hop neighbor
    uint32_t one_hop_addr;       // One-hop neighbor providing reach
    time_t last_seen;            // Timestamp of last update
    struct two_hop_neighbor *next; // Linked list pointer
};
```

#### 2. MPR Set
- Array of selected MPR addresses
- Boolean flag in neighbor_entry indicating MPR status

### Key Functions

#### MPR Calculation: `calculate_mpr_set()`

Implements the RFC 3626 MPR selection algorithm:

**Algorithm Steps:**

1. **Initialize**: Clear current MPR set, mark all neighbors as non-MPR

2. **Select WILL_ALWAYS nodes**: 
   - Add all symmetric neighbors with willingness = WILL_ALWAYS
   - These nodes always want to be MPRs

3. **Select unique paths**:
   - For each two-hop neighbor
   - If only one one-hop neighbor provides reach
   - Select that one-hop neighbor as MPR

4. **Greedy coverage**:
   - While not all two-hop neighbors are covered:
     - Find neighbor covering most uncovered two-hop neighbors
     - Break ties using willingness value (higher is better)
     - Add to MPR set and mark covered neighbors
     - Repeat until all are covered

**Time Complexity**: O(N² + M²) where N = one-hop neighbors, M = two-hop neighbors

#### Two-Hop Neighbor Management

**`add_two_hop_neighbor(two_hop_addr, one_hop_addr)`**
- Adds or updates a two-hop neighbor entry
- Maintains the relationship between two-hop and one-hop neighbors
- Updates timestamp for freshness tracking

**`update_two_hop_neighbors_from_hello(hello_msg, sender_addr)`**
- Called when processing HELLO messages
- Extracts two-hop neighbor information from HELLO
- Only processes symmetric neighbors
- Filters out self and existing one-hop neighbors

## MPR Selection Example

### Network Topology:
```
        N4      N5
         |       |
    N2---A---N3--+
    |    |       |
    N1   |      N6
         |
        This Node
```

### MPR Selection Process:

**One-hop neighbors**: N1, N2, N3, A  
**Two-hop neighbors**: N4 (via A), N5 (via N3, A), N6 (via N3)

**Selection Steps:**

1. **WILL_ALWAYS**: None in this example

2. **Unique paths**:
   - N4 is only reachable via A → Select A as MPR
   - N6 is only reachable via N3 → Select N3 as MPR

3. **Coverage check**:
   - N4: Covered by A ✓
   - N5: Covered by N3 and A ✓
   - N6: Covered by N3 ✓
   - All covered, algorithm complete

**Result**: MPR set = {A, N3}

## Integration with OLSR

### HELLO Message Processing

```c
// In process_hello_message():
process_hello_message(msg, sender_addr);

// Update two-hop neighbor information
struct olsr_hello* hello = (struct olsr_hello*)msg->body;
update_two_hop_neighbors_from_hello(hello, sender_addr);

// Recalculate MPR set
calculate_mpr_set();
```

### TC Message Generation

```c
// MPR selectors are used in TC messages
// When a node receives HELLO listing it as MPR:
add_mpr_selector(neighbor_who_selected_us);

// TC message includes MPR selectors
generate_tc_message(); // Contains MPR selector list
```

## Willingness Values

OLSR defines willingness to forward traffic:

| Value | Constant | Meaning |
|-------|----------|---------|
| 0 | WILL_NEVER | Never selected as MPR |
| 1 | WILL_LOW | Low willingness |
| 3 | WILL_DEFAULT | Default willingness |
| 6 | WILL_HIGH | High willingness |
| 7 | WILL_ALWAYS | Always selected as MPR |

**Usage**:
- Battery-powered devices: Use WILL_LOW or WILL_NEVER
- High-capacity nodes: Use WILL_HIGH or WILL_ALWAYS
- Default nodes: Use WILL_DEFAULT

## API Reference

### Core Functions

**`int calculate_mpr_set(void)`**
- Calculates the MPR set based on current neighbor information
- Returns: 0 on success, -1 on failure
- Should be called after neighbor table updates

**`int get_mpr_set(uint32_t* mpr_array, int max_size)`**
- Retrieves the current MPR addresses
- Parameters: output array and maximum size
- Returns: Number of MPRs in set

**`int is_mpr(uint32_t neighbor_addr)`**
- Checks if a neighbor is selected as MPR
- Returns: 1 if MPR, 0 otherwise

**`void print_mpr_set(void)`**
- Displays the current MPR set in human-readable format
- Useful for debugging and monitoring

### Two-Hop Management

**`int add_two_hop_neighbor(uint32_t two_hop, uint32_t one_hop)`**
- Adds a two-hop neighbor relationship
- Returns: 0 on success, -1 if table full

**`int get_two_hop_count(void)`**
- Returns the current number of two-hop neighbors

**`void print_two_hop_table(void)`**
- Displays all two-hop neighbor relationships

### Maintenance Functions

**`void clear_mpr_set(void)`**
- Clears the MPR set
- Marks all neighbors as non-MPR

**`void clear_two_hop_table(void)`**
- Removes all two-hop neighbor entries

## Usage Example

```c
#include "mpr.h"
#include "hello.h"

// After receiving HELLO messages and updating neighbors
void process_received_hello(struct olsr_message* msg, uint32_t sender) {
    // Process the HELLO message
    process_hello_message(msg, sender);
    
    // Extract two-hop neighbor information
    struct olsr_hello* hello = (struct olsr_hello*)msg->body;
    update_two_hop_neighbors_from_hello(hello, sender);
    
    // Recalculate MPR set
    calculate_mpr_set();
    
    // Display results
    print_two_hop_table();
    print_mpr_set();
}

// Check if a neighbor should forward our broadcasts
int should_neighbor_forward(uint32_t neighbor_addr) {
    return is_mpr(neighbor_addr);
}

// Get MPR list for TC message
void prepare_tc_message(void) {
    uint32_t mpr_list[MAX_NEIGHBORS];
    int mpr_count = get_mpr_set(mpr_list, MAX_NEIGHBORS);
    
    // Use mpr_list in TC message generation
    // ...
}
```

## Testing

### Test Scenario 1: Simple Star Topology
```c
// Central node with 4 neighbors
node_ip = 0x01010101;
add_neighbor(0x01010102, SYM_LINK, WILL_DEFAULT); // N1
add_neighbor(0x01010103, SYM_LINK, WILL_DEFAULT); // N2
add_neighbor(0x01010104, SYM_LINK, WILL_DEFAULT); // N3
add_neighbor(0x01010105, SYM_LINK, WILL_DEFAULT); // N4

// No two-hop neighbors
calculate_mpr_set();
// Expected: MPR set is empty (no two-hop neighbors to cover)
```

### Test Scenario 2: Chain Topology
```c
// This node -> N1 -> N2
node_ip = 0x01010101;
add_neighbor(0x01010102, SYM_LINK, WILL_DEFAULT); // N1
add_two_hop_neighbor(0x01010103, 0x01010102);     // N2 via N1

calculate_mpr_set();
// Expected: N1 is selected as MPR (only path to N2)
```

### Test Scenario 3: Multiple Paths
```c
// Complex topology with redundant paths
node_ip = 0x01010101;
add_neighbor(0x01010102, SYM_LINK, WILL_HIGH);    // N1
add_neighbor(0x01010103, SYM_LINK, WILL_DEFAULT); // N2
add_two_hop_neighbor(0x01010104, 0x01010102);     // N3 via N1
add_two_hop_neighbor(0x01010104, 0x01010103);     // N3 via N2

calculate_mpr_set();
// Expected: N1 selected (higher willingness)
```

## Performance Considerations

### Memory Usage
- Two-hop table: MAX_TWO_HOP_NEIGHBORS entries (default: 100)
- MPR set: MAX_NEIGHBORS entries (default: 40)
- Per-neighbor overhead: 1 byte (is_mpr flag)

### Computational Complexity
- MPR calculation: O(N × M) where N = neighbors, M = two-hop neighbors
- Typical networks: < 1ms for 50 neighbors, 100 two-hop neighbors

### Optimization Tips
1. **Periodic recalculation**: Only recalculate on topology changes
2. **Willingness strategy**: Set appropriate willingness based on node capability
3. **Table cleanup**: Periodically remove stale two-hop neighbors

## RFC 3626 Compliance

This implementation follows RFC 3626 Section 8.3 (MPR Computation):

✓ Step 1: Start with empty MPR set  
✓ Step 2: Select WILL_ALWAYS neighbors  
✓ Step 3: Select neighbors with unique paths  
✓ Step 4: Greedy coverage of remaining two-hop neighbors  
✓ Willingness-based tie-breaking  
✓ Coverage verification  

## Troubleshooting

### Issue: MPR set is empty
**Cause**: No two-hop neighbors detected  
**Solution**: Verify HELLO messages are being processed correctly

### Issue: Too many MPRs selected
**Cause**: Dense network with many two-hop neighbors  
**Solution**: This is expected behavior; each two-hop neighbor must be covered

### Issue: Node not receiving broadcasts
**Cause**: Not selected as MPR by any neighbor  
**Solution**: Check willingness value, verify symmetric links

## References

- RFC 3626: Optimized Link State Routing Protocol (OLSR)
- Section 8.3: MPR Computation
- Section 18: Neighbor Information Base

## Authors

OLSR Implementation Team  
Date: October 10, 2025
