#ifndef OLSR_RRC_INTERFACE_H
#define OLSR_RRC_INTERFACE_H

#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#include <stdint.h>

/**
 * @brief Start the OLSR layer thread for RRC integration
 * @param node_id Node identifier in RRC format (8-bit)
 * @return Thread handle, or 0 on failure
 */
pthread_t start_olsr_thread(uint8_t node_id);

#endif // OLSR_RRC_INTERFACE_H