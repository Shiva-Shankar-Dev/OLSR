# TDMA Slot Reservation Implementation for OLSR

## Overview

This implementation adds TDMA (Time Division Multiple Access) slot reservation functionality to the OLSR protocol. It enables collision-free medium access by allowing nodes to share their slot reservations and coordinate transmission schedules across 1-hop and 2-hop neighborhoods.

## Key Features

✅ **Two-hop TDMA coordination**: Nodes share slot reservations up to 2 hops  
✅ **Collision avoidance**: Check slot availability before transmission  
✅ **Dynamic updates**: Slot reservations update with each HELLO exchange  
✅ **Automatic cleanup**: Expired reservations are automatically removed  
✅ **Integration**: Works seamlessly with existing OLSR neighbor management  

## API Reference

### Core TDMA Functions

```c
// Set this node's TDMA slot reservation
void set_my_slot_reservation(int slot_number);

// Update neighbor's TDMA slot reservation
void update_neighbor_slot_reservation(uint32_t neighbor_id, int slot_number, int hop_distance);

// Get neighbor's TDMA slot reservation  
int get_neighbor_slot_reservation(uint32_t neighbor_id);

// Check if a TDMA slot is available for use
int is_slot_available(int slot_number);

// Get list of occupied slots in neighborhood
int get_occupied_slots(int* occupied_slots, int max_slots);

// Print current TDMA slot reservations
void print_tdma_reservations(void);

// Clean up expired slot reservations
void cleanup_expired_reservations(int max_age);
```

### Usage Examples

#### 1. Basic Slot Reservation

```c
// Reserve slot 10 for this node
set_my_slot_reservation(10);

// Check if slot 15 is available
if (is_slot_available(15)) {
    printf("Slot 15 is available for use\n");
    set_my_slot_reservation(15);
} else {
    printf("Slot 15 is occupied by another node\n");
}
```

#### 2. TDMA Program Integration

```c
int find_available_slot(void) {
    // Get list of all occupied slots
    int occupied[MAX_TDMA_SLOTS];
    int count = get_occupied_slots(occupied, MAX_TDMA_SLOTS);
    
    // Find first available slot
    for (int slot = 0; slot < MAX_TDMA_SLOTS; slot++) {
        if (is_slot_available(slot)) {
            return slot;
        }
    }
    return -1; // No available slots
}

void tdma_transmission_check(int desired_slot) {
    if (is_slot_available(desired_slot)) {
        // Safe to transmit
        set_my_slot_reservation(desired_slot);
        printf("Transmitting on slot %d\n", desired_slot);
        // ... perform transmission ...
    } else {
        printf("Slot %d occupied, waiting for next opportunity\n", desired_slot);
        // Choose alternative slot or wait
        int alt_slot = find_available_slot();
        if (alt_slot >= 0) {
            set_my_slot_reservation(alt_slot);
            printf("Using alternative slot %d\n", alt_slot);
        }
    }
}
```

#### 3. Network Monitoring

```c
void monitor_network_tdma(void) {
    printf("=== Network TDMA Status ===\n");
    
    // Print all reservations
    print_tdma_reservations();
    
    // Check specific slots
    int critical_slots[] = {0, 1, 10, 20, 50};
    int num_slots = sizeof(critical_slots) / sizeof(critical_slots[0]);
    
    printf("\nCritical Slot Status:\n");
    for (int i = 0; i < num_slots; i++) {
        int slot = critical_slots[i];
        printf("Slot %d: %s\n", slot, 
               is_slot_available(slot) ? "FREE" : "OCCUPIED");
    }
    
    // Cleanup old reservations
    cleanup_expired_reservations(30);
}
```

## HELLO Message Extensions

The HELLO message structure has been extended to include TDMA information:

```c
struct olsr_hello {
    uint16_t hello_interval;
    uint8_t willingness; 
    uint8_t neighbor_count;
    int reserved_slot;           // NEW: This node's TDMA slot reservation
    struct hello_neighbor* neighbors;
    
    // NEW: Two-hop neighbor TDMA information
    uint8_t two_hop_count;
    struct two_hop_hello_neighbor* two_hop_neighbors;
};

struct two_hop_hello_neighbor {
    uint32_t two_hop_id;        // Two-hop neighbor node ID
    uint32_t via_neighbor_id;   // One-hop neighbor providing the path  
    int reserved_slot;          // TDMA slot reserved by two-hop neighbor
};
```

## Constants and Configuration

```c
#define MAX_TWO_HOP_NEIGHBORS 100    // Maximum two-hop neighbors in HELLO
#define MAX_TDMA_SLOTS 100           // Maximum TDMA slots in system
#define SLOT_RESERVATION_TIMEOUT 30  // Seconds before reservation expires
```

## Implementation Details

### Slot Reservation Table

The implementation maintains a table of neighbor slot reservations:

```c
static struct {
    uint32_t node_id;      // Neighbor's node ID
    int reserved_slot;     // Reserved slot number (-1 = no reservation)  
    time_t last_updated;   // Timestamp of last update
    int hop_distance;      // 1 for direct neighbors, 2 for two-hop
} neighbor_slots[MAX_NEIGHBORS + MAX_TWO_HOP_NEIGHBORS];
```

### Message Processing Flow

1. **HELLO Generation**: Include our slot reservation and two-hop neighbor TDMA info
2. **HELLO Reception**: Update sender's slot reservation and process two-hop neighbor info  
3. **Slot Tracking**: Maintain reservations for 1-hop and 2-hop neighbors
4. **Conflict Detection**: Check slot availability against all known reservations
5. **Cleanup**: Remove expired reservations periodically

### Serialization

The HELLO message serialization has been extended to handle TDMA fields:
- Basic HELLO fields (existing)
- One-hop neighbors (existing) 
- Two-hop neighbor count (new)
- Two-hop neighbor TDMA information (new)

## Integration with Existing Code

The TDMA functionality integrates seamlessly with existing OLSR components:

- **MPR Module**: Uses existing `get_two_hop_count()` and `get_two_hop_table()` functions
- **Neighbor Management**: Leverages existing neighbor table and update mechanisms  
- **Message Processing**: Extends existing HELLO processing without breaking compatibility
- **Routing**: No changes required - TDMA is transparent to routing calculations

## Testing

Run the included test program:

```bash
gcc -Wall -Wextra -g -Iinclude src/hello.c src/mpr.c src/routing.c src/tc.c src/neighbor.c src/control_queue.c tdma_test.c -o tdma_test
./tdma_test
```

## Benefits

1. **Collision Avoidance**: Prevents transmission collisions in TDMA networks
2. **Network Efficiency**: Optimizes channel utilization through coordination  
3. **Scalability**: Works with networks up to 2-hop diameter for slot coordination
4. **Compatibility**: Maintains full OLSR protocol compliance
5. **Real-time**: Provides immediate slot availability feedback

## Future Enhancements

- Extend coordination to 3+ hops for larger networks
- Add priority-based slot allocation
- Implement slot reservation requests and negotiations
- Add QoS-aware slot scheduling
- Support for dynamic slot timing adjustments

---

**Author**: OLSR Implementation Team  
**Date**: October 14, 2025  
**Version**: 1.0.0