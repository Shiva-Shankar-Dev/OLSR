# OLSR MPR Implementation - Build and Usage Guide

## Overview

This directory contains a complete implementation of the MPR (Multipoint Relay) selection algorithm for the OLSR routing protocol, following RFC 3626.

## Files Structure

```
OLSR/
├── include/
│   ├── olsr.h          # Core OLSR definitions
│   ├── hello.h         # HELLO message functions
│   ├── mpr.h           # MPR selection functions (NEW)
│   ├── packet.h        # Packet structures
│   ├── routing.h       # Routing table management
│   └── tc.h            # TC message functions
├── src/
│   ├── main.c          # Main OLSR daemon
│   ├── hello.c         # HELLO message implementation
│   ├── mpr.c           # MPR selection implementation (NEW)
│   ├── mpr_test.c      # MPR test suite (NEW)
│   ├── neighbor.c      # Neighbor management
│   ├── routing.c       # Routing calculations
│   ├── tc.c            # TC message handling
│   └── control_queue.c # Control queue management
├── MPR_DOCUMENTATION.md # Detailed MPR documentation (NEW)
└── MPR_README.md       # This file (NEW)
```

## Building the MPR Module

### Prerequisites

- GCC compiler or compatible C compiler
- Make (optional)
- Standard C libraries

### Compilation Options

#### Option 1: Compile MPR test program

```bash
# Windows (PowerShell)
gcc -o mpr_test.exe src/mpr.c src/mpr_test.c src/neighbor.c src/hello.c src/control_queue.c -I./include

# Linux/Mac
gcc -o mpr_test src/mpr.c src/mpr_test.c src/neighbor.c src/hello.c src/control_queue.c -I./include
```

#### Option 2: Compile as part of full OLSR

```bash
# Windows (PowerShell)
gcc -o olsr.exe src/*.c -I./include

# Linux/Mac
gcc -o olsr src/*.c -I./include
```

#### Option 3: Compile with debugging symbols

```bash
gcc -g -O0 -o mpr_test_debug src/mpr.c src/mpr_test.c src/neighbor.c src/hello.c src/control_queue.c -I./include
```

## Running the MPR Test Suite

### Execute the test program:

```bash
# Windows
.\mpr_test.exe

# Linux/Mac
./mpr_test
```

### Expected Output:

```
╔════════════════════════════════════════════════════╗
║     MPR Selection Algorithm Test Suite            ║
║     OLSR Implementation                            ║
╚════════════════════════════════════════════════════╝

========================================
TEST 1: Star Topology (No Two-Hop Neighbors)
========================================
...
Test PASSED

========================================
TEST 2: Chain Topology (Single Path)
========================================
...
Test PASSED

[Additional tests...]
```

## Using MPR in Your OLSR Implementation

### Basic Integration

```c
#include "mpr.h"
#include "hello.h"

// 1. Process received HELLO messages
void handle_hello(struct olsr_message* msg, uint32_t sender) {
    // Process HELLO to update neighbor table
    process_hello_message(msg, sender);
    
    // Extract two-hop neighbor information
    struct olsr_hello* hello = (struct olsr_hello*)msg->body;
    update_two_hop_neighbors_from_hello(hello, sender);
    
    // Recalculate MPR set
    calculate_mpr_set();
}

// 2. Use MPR set for broadcast forwarding
int should_forward_broadcast(uint32_t from_neighbor) {
    // Only forward if received from an MPR neighbor
    return is_mpr(from_neighbor);
}

// 3. Include MPR status in HELLO messages
struct olsr_hello* create_hello(void) {
    struct olsr_hello* hello = generate_hello_message();
    
    // Mark neighbors selected as MPR
    for (int i = 0; i < hello->neighbor_count; i++) {
        if (is_mpr(hello->neighbors[i].neighbor_addr)) {
            // Set MPR flag in link code
            hello->neighbors[i].link_code |= 0x04; // MPR bit
        }
    }
    
    return hello;
}
```

### Advanced Usage

```c
// Monitor MPR changes
void on_topology_change(void) {
    int old_mpr_count = get_mpr_count();
    
    calculate_mpr_set();
    
    int new_mpr_count = get_mpr_count();
    
    if (old_mpr_count != new_mpr_count) {
        printf("MPR set changed: %d -> %d\n", old_mpr_count, new_mpr_count);
        print_mpr_set();
    }
}

// Periodic maintenance
void periodic_mpr_update(void) {
    // Clean up old two-hop neighbors
    // (You may want to implement aging mechanism)
    
    // Recalculate MPR set
    calculate_mpr_set();
    
    // Log statistics
    printf("MPR Stats: %d MPRs, %d two-hop neighbors\n",
           get_mpr_count(), get_two_hop_count());
}
```

## API Quick Reference

### MPR Calculation
- `int calculate_mpr_set(void)` - Calculate MPR set based on current topology
- `int get_mpr_count(void)` - Get number of selected MPRs
- `int is_mpr(uint32_t neighbor_addr)` - Check if neighbor is MPR

### Two-Hop Neighbor Management
- `int add_two_hop_neighbor(uint32_t two_hop, uint32_t one_hop)` - Add two-hop neighbor
- `int remove_two_hop_neighbor(uint32_t two_hop, uint32_t one_hop)` - Remove two-hop neighbor
- `int get_two_hop_count(void)` - Get two-hop neighbor count
- `void update_two_hop_neighbors_from_hello(struct olsr_hello*, uint32_t)` - Update from HELLO

### Utility Functions
- `void print_mpr_set(void)` - Display current MPR set
- `void print_two_hop_table(void)` - Display two-hop neighbor table
- `void clear_mpr_set(void)` - Clear MPR set
- `void clear_two_hop_table(void)` - Clear two-hop table

## Configuration

### Tuning Parameters

Edit `src/mpr.c` to adjust:

```c
#define MAX_TWO_HOP_NEIGHBORS 100  // Maximum two-hop neighbors
```

Edit `include/olsr.h` to adjust:

```c
#define MAX_NEIGHBORS 40           // Maximum one-hop neighbors
#define WILL_DEFAULT 3             // Default willingness value
```

### Willingness Settings

Set node willingness based on device capability:

```c
// Low-power devices
node_willingness = WILL_LOW;      // Value: 1

// Normal devices
node_willingness = WILL_DEFAULT;  // Value: 3

// High-capacity routers
node_willingness = WILL_HIGH;     // Value: 6

// Always forward
node_willingness = WILL_ALWAYS;   // Value: 7
```

## Troubleshooting

### Problem: MPR set is always empty

**Solution**: Check that two-hop neighbors are being added correctly.

```c
// Verify two-hop neighbor count
printf("Two-hop count: %d\n", get_two_hop_count());
print_two_hop_table();
```

### Problem: Too many MPRs selected

**Solution**: This is normal in dense networks. Each two-hop neighbor must be covered.

```c
// Check coverage
print_two_hop_table();  // See which two-hop neighbors exist
print_mpr_set();        // See which MPRs are selected
```

### Problem: Compilation errors

**Solution**: Ensure all dependencies are compiled:

```bash
# Compile in order:
gcc -c src/neighbor.c -I./include
gcc -c src/hello.c -I./include
gcc -c src/control_queue.c -I./include
gcc -c src/mpr.c -I./include
gcc -c src/mpr_test.c -I./include

# Link all together:
gcc -o mpr_test *.o
```

## Performance Characteristics

### Time Complexity
- MPR calculation: O(N × M) where N = neighbors, M = two-hop neighbors
- Typical: < 1ms for 50 neighbors

### Memory Usage
- Two-hop table: ~20 bytes × MAX_TWO_HOP_NEIGHBORS (default: 2KB)
- MPR set: ~4 bytes × MAX_NEIGHBORS (default: 160 bytes)
- Per-neighbor overhead: 1 byte (is_mpr flag)

### Optimization Tips
1. Only recalculate on topology changes (not periodically)
2. Set appropriate MAX values based on network size
3. Use higher willingness on capable nodes

## Testing Scenarios

The test suite includes:

1. **Star Topology**: No two-hop neighbors
2. **Chain Topology**: Single path selection
3. **Multiple Paths**: Willingness-based selection
4. **Complex Topology**: Optimal coverage
5. **Willingness Extremes**: ALWAYS/NEVER handling
6. **Dynamic Updates**: Topology changes

Run specific test:
```c
// In mpr_test.c, comment out other tests and run only desired one
test_complex_topology();
```

## Integration Checklist

- [ ] Include `mpr.h` in your OLSR code
- [ ] Call `update_two_hop_neighbors_from_hello()` when processing HELLO
- [ ] Call `calculate_mpr_set()` after topology changes
- [ ] Use `is_mpr()` to check if neighbor should forward
- [ ] Include MPR status in your HELLO messages
- [ ] Set appropriate `node_willingness` value
- [ ] Implement periodic table cleanup (optional)

## Further Reading

- **MPR_DOCUMENTATION.md**: Detailed algorithm explanation
- **RFC 3626**: OLSR specification (Section 8.3)
- **include/mpr.h**: Full API documentation
- **src/mpr.c**: Implementation with inline comments

## Support

For questions or issues:
1. Check MPR_DOCUMENTATION.md for algorithm details
2. Review test cases in mpr_test.c
3. Enable debug output in calculate_mpr_set()

## License

This implementation follows the OLSR RFC 3626 specification.

## Authors

OLSR Implementation Team  
Date: October 10, 2025

---

**Note**: This is an educational implementation. For production use, additional features like:
- Two-hop neighbor aging
- Link quality metrics
- Advanced willingness strategies
- Performance optimizations

may be required.
