// ...existing includes...
#ifdef RRC_INTEGRATION
#include "../include/olsr_rrc_interface.h"
#endif

int main(int argc, char* argv[]) {
    
#ifdef RRC_INTEGRATION
    // RRC Integration mode - run as thread
    uint8_t node_id_rrc = 1;  // Get from command line or config
    
    if (argc > 1) {
        node_id_rrc = (uint8_t)atoi(argv[1]);
    }
    
    printf("Starting OLSR in RRC integration mode (node_id=%u)\n", node_id_rrc);
    
    pthread_t olsr_thread = start_olsr_thread(node_id_rrc);
    
    if (olsr_thread == 0) {
        fprintf(stderr, "Failed to start OLSR thread\n");
        return 1;
    }
    
    // Wait for thread (in real system, this would be managed by RRC)
    pthread_join(olsr_thread, NULL);
    
#else
    // Standalone mode - existing test/simulation code
    printf("Starting OLSR in standalone test mode\n");
    init_olsr();
    simulate();
#endif
    
    return 0;
}