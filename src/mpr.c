#include "../include/olsr.h"
#include "../include/hello.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

/** @brief Maximum number of nodes in the topology */
#define MAX_NODES 50
/** @brief Maximum entries in the routing table */
#define MAX_ROUTING_ENTRIES 100
/** @brief Infinite cost for unreachable nodes */
#define INFINITE_COST INT_MAX
/** @brief Routing table entry structure */
