# OLSR MPR Visual Guide

## MPR Concept Visualization

### What Problem Does MPR Solve?

#### Without MPR (Naive Flooding):
```
Network with 7 nodes, broadcasting a message from Node A:

Step 1: Node A broadcasts
    [A]* ───────> All neighbors receive
     ├─> [B]
     ├─> [C]
     ├─> [D]
     └─> [E]

Step 2: ALL neighbors retransmit
    [B]* ───> Redundant transmissions
    [C]* ───> Redundant transmissions  
    [D]* ───> Redundant transmissions
    [E]* ───> Redundant transmissions

Result: 5 transmissions (1 original + 4 retransmissions)
Problem: Many redundant broadcasts covering the same area!
```

#### With MPR (Optimized):
```
Network with same 7 nodes, using MPR:

Step 1: Node A selects MPRs (B and D)
    [A]* ───────> All neighbors receive
     ├─> [B] (MPR)
     ├─> [C] (not MPR)
     ├─> [D] (MPR)
     └─> [E] (not MPR)

Step 2: Only MPRs retransmit
    [B]* ───> Retransmits
    [C]  ───> Does NOT retransmit
    [D]* ───> Retransmits
    [E]  ───> Does NOT retransmit

Result: 3 transmissions (1 original + 2 retransmissions)
Benefit: 40% fewer transmissions, same coverage!
```

## MPR Selection Algorithm Visualization

### Example Network Topology:

```
                Two-hop neighbors (Goal: Cover all of these)
                ┌────────────────────────────────────┐
                │                                     │
                │    [N6]         [N7]         [N8]  │
                │      │           │            │    │
                │      │           │            │    │
One-hop         │    [N2]────────[N3]────────[N4]   │
neighbors       │      │           │            │    │
                │      │           │            │    │
                │      └───────────┴────────────┘    │
                │                  │                 │
                └──────────────────│─────────────────┘
                                   │
                              [This Node]
                               (10.0.0.1)
```

### Step-by-Step MPR Selection:

#### Initial State:
```
One-hop neighbors:
  • N2 (10.0.0.2) - Willingness: DEFAULT, Link: SYM
  • N3 (10.0.0.3) - Willingness: DEFAULT, Link: SYM
  • N4 (10.0.0.4) - Willingness: DEFAULT, Link: SYM

Two-hop neighbors:
  • N6 (10.0.0.6) via N2
  • N7 (10.0.0.7) via N3
  • N8 (10.0.0.8) via N4

MPR Set: [] (empty)
Covered: [] (none)
```

#### Step 1: Select WILL_ALWAYS nodes
```
Checking willingness values...
  N2: DEFAULT (3) - not ALWAYS
  N3: DEFAULT (3) - not ALWAYS
  N4: DEFAULT (3) - not ALWAYS

MPR Set: [] (still empty)
No WILL_ALWAYS nodes found
```

#### Step 2: Select nodes with unique paths
```
Checking coverage:
  N6: Only reachable via N2 ✓ (unique path!)
  N7: Only reachable via N3 ✓ (unique path!)
  N8: Only reachable via N4 ✓ (unique path!)

Action: Select all three as MPRs

MPR Set: [N2, N3, N4]
Covered: [N6, N7, N8] (all!)
```

#### Step 3: Coverage check
```
Checking if all two-hop neighbors are covered:
  N6: Covered by N2 ✓
  N7: Covered by N3 ✓
  N8: Covered by N4 ✓

All two-hop neighbors covered!
Algorithm complete.
```

#### Final Result:
```
╔════════════════════════════════════╗
║ MPR Selection Complete             ║
╠════════════════════════════════════╣
║ Selected MPRs: 3                   ║
║   • N2 (10.0.0.2) - covers N6      ║
║   • N3 (10.0.0.3) - covers N7      ║
║   • N4 (10.0.0.4) - covers N8      ║
╚════════════════════════════════════╝
```

## Complex Example: Multiple Paths

### Network with Redundant Paths:

```
                Two-hop neighbors
                ┌─────────────────────────┐
                │                          │
                │       [N5]       [N6]    │
                │        │  \      / │     │
One-hop         │        │   \    /  │     │
neighbors       │      [N2]   [N3]  [N4]   │
                │        │   /    \  │     │
                │        │  /      \ │     │
                │        │ /        \│     │
                └────────┴───────────┴─────┘
                         │
                    [This Node]
```

### Topology Details:
```
One-hop neighbors:
  • N2 - Willingness: LOW (1)
  • N3 - Willingness: HIGH (6)  
  • N4 - Willingness: DEFAULT (3)

Two-hop neighbors:
  • N5 via N2, N3 (multiple paths)
  • N6 via N3, N4 (multiple paths)
```

### Selection Process:

#### Step 1: WILL_ALWAYS
```
No WILL_ALWAYS nodes
MPR Set: []
```

#### Step 2: Unique paths
```
N5: Reachable via N2 AND N3 (not unique)
N6: Reachable via N3 AND N4 (not unique)

No unique paths found
MPR Set: [] (still empty)
```

#### Step 3: Greedy coverage
```
Iteration 1:
  Counting uncovered two-hop neighbors:
    N2 covers: N5 (1 uncovered) - willingness: 1
    N3 covers: N5, N6 (2 uncovered) - willingness: 6 ✓ BEST
    N4 covers: N6 (1 uncovered) - willingness: 3

  Select N3 (most coverage + highest willingness)
  
  MPR Set: [N3]
  Covered: [N5, N6]

Iteration 2:
  All two-hop neighbors covered!
  Algorithm complete.
```

#### Result:
```
╔════════════════════════════════════════╗
║ MPR Selection Complete                 ║
╠════════════════════════════════════════╣
║ Selected MPRs: 1                       ║
║   • N3 (10.0.0.3) - HIGH willingness   ║
║     Covers: N5, N6                     ║
║                                        ║
║ Not selected:                          ║
║   • N2 - Lower willingness             ║
║   • N4 - Redundant coverage            ║
╚════════════════════════════════════════╝

Optimization: 66% reduction (1 MPR instead of 3)
```

## Willingness Impact

### Scenario: Same Coverage, Different Willingness

```
Topology:
    [N5]
     │
   [N2]──[N3]
     │    │
     └────┴─── [This Node]
```

### Case 1: Equal Willingness
```
N2: Willingness DEFAULT (3), covers N5
N3: Willingness DEFAULT (3), covers N5

Selection: N2 (arbitrary choice, same coverage)
MPR Set: [N2]
```

### Case 2: Different Willingness
```
N2: Willingness LOW (1), covers N5
N3: Willingness HIGH (6), covers N5

Selection: N3 (higher willingness preferred)
MPR Set: [N3]
```

### Case 3: WILL_ALWAYS
```
N2: Willingness ALWAYS (7), covers N5
N3: Willingness DEFAULT (3), covers N5

Selection: N2 (WILL_ALWAYS selected first)
MPR Set: [N2]
```

### Case 4: WILL_NEVER
```
N2: Willingness NEVER (0), covers N5
N3: Willingness DEFAULT (3), covers N5

Selection: N3 (N2 cannot be MPR)
MPR Set: [N3]
```

## Message Flow with MPR

### Broadcasting a TC Message:

```
Step 1: Node A generates TC message
┌─────────────────────────────────┐
│ Node A (10.0.0.1)               │
│ Action: Broadcast TC message    │
│ MPR Set: [B, D]                 │
└─────────────────────────────────┘
           │
           │ TC: "My MPR selectors are..."
           ▼
    ┌──────────────┐
    │  Broadcast   │
    └──────────────┘
           │
    ┌──────┼──────┬──────┐
    ▼      ▼      ▼      ▼
   [B]    [C]    [D]    [E]
  (MPR)  (not)  (MPR)  (not)
    │                    │
    │ Forwards          │ Forwards
    ▼                   ▼
  [F,G]                [H,I]

Step 2: Only MPRs (B and D) retransmit
  • C does not forward (not an MPR)
  • E does not forward (not an MPR)

Result: Message reaches all nodes with minimal transmissions
```

## Data Structures

### Two-Hop Neighbor Table:
```
┌────────────────────────────────────────────────┐
│ Index │ Two-Hop Addr │ Via One-Hop │ Last Seen│
├───────┼──────────────┼─────────────┼──────────┤
│   0   │  10.0.0.5    │  10.0.0.2   │ 12:34:56 │
│   1   │  10.0.0.6    │  10.0.0.2   │ 12:34:58 │
│   2   │  10.0.0.6    │  10.0.0.3   │ 12:35:01 │
│   3   │  10.0.0.7    │  10.0.0.3   │ 12:35:01 │
│   4   │  10.0.0.8    │  10.0.0.4   │ 12:35:03 │
└────────────────────────────────────────────────┘

Note: Same two-hop neighbor can appear multiple times
      if reachable through different one-hop neighbors
```

### MPR Set:
```
┌────────────────────────────┐
│ Index │ MPR Address        │
├───────┼────────────────────┤
│   0   │ 10.0.0.2           │
│   1   │ 10.0.0.3           │
│   2   │ 10.0.0.4           │
└────────────────────────────┘

Corresponding neighbor_table entries:
  neighbor_table[i].is_mpr = 1 (for MPRs)
  neighbor_table[i].is_mpr = 0 (for non-MPRs)
```

### Coverage Bitmap (during calculation):
```
Two-hop neighbors:    N5   N6   N7   N8
                      [0]  [0]  [0]  [0]  Initial: none covered
                       ▼
After selecting N2:   [1]  [1]  [0]  [0]  N5, N6 now covered
                       ▼
After selecting N3:   [1]  [1]  [1]  [0]  N7 now covered
                       ▼
After selecting N4:   [1]  [1]  [1]  [1]  All covered!

Legend: 0 = uncovered, 1 = covered
```

## Algorithm Pseudocode

```
ALGORITHM: calculate_mpr_set()

INPUT: 
  - neighbor_table[] (one-hop neighbors with link status)
  - two_hop_table[] (two-hop neighbors and paths)

OUTPUT:
  - mpr_set[] (selected MPRs)

BEGIN
  1. Initialize
     mpr_set = []
     covered_set = [false, false, ..., false]
     
  2. Handle WILL_ALWAYS
     FOR EACH neighbor IN neighbor_table:
       IF neighbor.link_status == SYM_LINK AND 
          neighbor.willingness == WILL_ALWAYS:
         mpr_set.add(neighbor)
         mark_covered(neighbor, covered_set)
       END IF
     END FOR
     
  3. Handle unique paths
     FOR EACH neighbor IN neighbor_table:
       IF neighbor.link_status == SYM_LINK AND
          neighbor.willingness != WILL_NEVER AND
          is_only_path(neighbor):
         mpr_set.add(neighbor)
         mark_covered(neighbor, covered_set)
       END IF
     END FOR
     
  4. Greedy coverage
     WHILE NOT all_covered(covered_set):
       best_neighbor = NULL
       max_coverage = 0
       best_willingness = -1
       
       FOR EACH neighbor IN neighbor_table:
         IF neighbor.link_status == SYM_LINK AND
            neighbor NOT IN mpr_set AND
            neighbor.willingness != WILL_NEVER:
           
           coverage = count_uncovered(neighbor, covered_set)
           
           IF coverage > max_coverage OR
              (coverage == max_coverage AND 
               neighbor.willingness > best_willingness):
             best_neighbor = neighbor
             max_coverage = coverage
             best_willingness = neighbor.willingness
           END IF
         END IF
       END FOR
       
       IF best_neighbor == NULL:
         BREAK  // Cannot cover all (shouldn't happen)
       END IF
       
       mpr_set.add(best_neighbor)
       mark_covered(best_neighbor, covered_set)
     END WHILE
     
  5. Return mpr_set
END
```

## Performance Visualization

### Time Complexity Analysis:

```
Step 1: WILL_ALWAYS
  FOR each neighbor (N iterations)
    Check willingness: O(1)
    Mark covered: O(M) where M = two-hop count
  Total: O(N × M)

Step 2: Unique paths
  FOR each neighbor (N iterations)
    Check if only path: O(M) where M = two-hop count
    Mark covered: O(M)
  Total: O(N × M)

Step 3: Greedy coverage
  WHILE not all covered (at most N iterations)
    FOR each neighbor (N iterations)
      Count uncovered: O(M)
    Mark covered: O(M)
  Total: O(N² × M)

Overall: O(N² × M)

Typical case: N=20, M=40
  Operations: 20² × 40 = 16,000
  Modern CPU: < 1ms
```

### Memory Usage:

```
Structure Sizes:
┌───────────────────────────────────────────────┐
│ Component              │ Size    │ Count      │
├────────────────────────┼─────────┼────────────┤
│ two_hop_neighbor       │ 20 bytes│ × 100      │
│ MPR set array          │  4 bytes│ × 40       │
│ Neighbor is_mpr flags  │  1 byte │ × 40       │
│ Temp coverage bitmap   │  1 byte │ × 100      │
│                        │         │            │
│ Total (static)         │ ~2.3 KB │            │
│ Total (temp/stack)     │ ~0.1 KB │            │
└───────────────────────────────────────────────┘
```

## Integration Flow

```
┌─────────────────────────────────────────────────┐
│                OLSR Main Loop                   │
└─────────────────────────────────────────────────┘
                     │
         ┌───────────┴───────────┐
         │                       │
         ▼                       ▼
  ┌─────────────┐         ┌──────────────┐
  │ Receive     │         │ Timer        │
  │ HELLO       │         │ Expired      │
  └─────────────┘         └──────────────┘
         │                       │
         │                       │
         ▼                       │
  ┌─────────────────────────┐   │
  │ process_hello_message() │   │
  │ - Update neighbor table │   │
  └─────────────────────────┘   │
         │                       │
         ▼                       │
  ┌──────────────────────────────┴──────┐
  │ update_two_hop_neighbors_from_hello()│
  │ - Extract two-hop info               │
  └──────────────────────────────────────┘
                  │
                  ▼
         ┌─────────────────┐
         │ calculate_mpr_set()│
         │ - Select MPRs    │
         └─────────────────┘
                  │
                  ▼
         ┌─────────────────┐
         │ generate_hello() │
         │ - Include MPR    │
         │   status         │
         └─────────────────┘
```

## Summary

MPR optimization reduces broadcast overhead by:
- Selecting minimal set of relay nodes
- Ensuring all two-hop neighbors are covered
- Respecting node willingness preferences
- Maintaining network connectivity

Key metrics:
- Typical reduction: 40-60% fewer broadcasts
- Coverage guarantee: 100% of two-hop neighbors
- Calculation time: < 1ms for typical networks
- Memory overhead: ~2KB

---

Visual diagrams created for MPR algorithm understanding  
Date: October 10, 2025
