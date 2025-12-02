/**
 * @file olsr.h
 * @brief Core OLSR protocol definitions and data structures
 * @author OLSR Implementation Team  
 * @date 2025-09-23
 * 
 * This file contains the fundamental definitions, constants, and data structures
 * for the OLSR (Optimized Link State Routing) protocol implementation.
 */

#ifndef OLSR_H
#define OLSR_H

#include<stdint.h>
#include<stddef.h>
#include<time.h>

/**
 * @defgroup MessageTypes OLSR Message Types
 * @brief Defines for different types of OLSR messages
 * @{
 */
#define MSG_HELLO 1  /**< HELLO message type for neighbor discovery */
#define MSG_TC 2     /**< Topology Control message type */
/** @} */

/**
 * @defgroup WillingnessValues Node Willingness Values
 * @brief Defines for node willingness to act as MPR
 * @{
 */
#define MAX_MESSAGE_SIZE 512
#define WILL_NEVER    0  /**< Node will never act as MPR */
#define WILL_LOW      1  /**< Low willingness to act as MPR */
#define WILL_DEFAULT  3  /**< Default willingness level */
#define WILL_HIGH     6  /**< High willingness to act as MPR */
#define WILL_ALWAYS   7  /**< Node will always act as MPR */
/** @} */

/**
 * @defgroup LinkCodes Link Status Codes
 * @brief Defines for link status in HELLO messages
 * @{
 */
#define UNSPEC_LINK   0  /**< Unspecified link */
#define ASYM_LINK     1  /**< Asymmetric link */
#define SYM_LINK      2  /**< Symmetric link */
#define LOST_LINK     3  /**< Lost link */
#define MPR_NEIGH     4  /**< MPR neighbor link */
/** @} */

/**
 * @defgroup Intervals Protocol Timing Intervals
 * @brief Default time intervals for OLSR protocol operations
 * @{
 */
#define HELLO_INTERVAL 2  /**< HELLO message interval in seconds */
#define TC_INTERVAL    5  /**< TC message interval in seconds */
#define HELLO_TIMEOUT  6  /**< HELLO timeout for link failure detection in seconds */
/** @} */

/**
 * @defgroup RetryConstants Message Retry and Failure Recovery Constants
 * @brief Constants for message retransmission and failure detection
 * @{
 */
#define MAX_RETRY_ATTEMPTS 3    /**< Maximum number of retry attempts */
#define RETRY_BASE_INTERVAL 2   /**< Base retry interval in seconds */
#define MAX_RETRY_INTERVAL 16   /**< Maximum retry interval in seconds */
/** @} */

/**
 * @defgroup TDMAConstants TDMA Slot Management Constants
 * @brief Constants for TDMA slot reservation and management
 * @{
 */
#define MAX_TWO_HOP_NEIGHBORS 100    /**< Maximum two-hop neighbors in HELLO */
#define MAX_TDMA_SLOTS 100           /**< Maximum TDMA slots in system */
#define SLOT_RESERVATION_TIMEOUT 30  /**< Seconds before reservation expires */
/** @} */

#define MAX_NEIGHBORS 40  /**< Maximum number of neighbors in table */

/**
 * @brief Neighbor table entry structure
 * 
 * Represents a single neighbor in the OLSR neighbor table.
 * Contains information about link status, willingness, and timestamps.
 */
struct neighbor_entry {
    uint32_t neighbor_id;      /**< IP address of the neighbor */
    uint8_t link_status;         /**< Link status (SYM_LINK, ASYM_LINK, etc.) */
    time_t last_seen;            /**< Timestamp of last received message */
    time_t last_hello_time;      /**< Timestamp of last HELLO message received */
    uint8_t willingness;         /**< Neighbor's willingness to act as MPR */
    int is_mpr;                  /**< Flag: 1 if neighbor is selected as MPR */
    int is_mpr_selector;         /**< Flag: 1 if neighbor selected this node as MPR */
    struct neighbor_entry *next; /**< Pointer to next neighbor (for linked list) */
};

/**
 * @brief OLSR node structure
 * 
 * Contains all the state information for an OLSR node including
 * neighbor tables, MPR sets, and control structures.
 */
struct olsr_node {
    uint32_t node_id;            /**< Unique node identifier (IP address) */
    uint8_t willingness;         /**< This node's willingness to act as MPR */
    uint16_t hello_seq_num;      /**< Sequence number for HELLO messages */
    uint16_t packet_seq_num;     /**< Sequence number for packets */
    time_t last_hello_time;      /**< Timestamp of last HELLO message sent */
    
    struct neighbor_entry *one_hop_neighbors;  /**< List of one-hop neighbors */
    struct two_hop_neighbor *two_hop_neighbors; /**< List of two-hop neighbors */

    uint32_t mpr_set[MAX_NEIGHBORS]; /**< Array of selected MPR addresses */
    int mpr_count;               /**< Number of MPRs in the set */
    
    struct control_queue *ctrl_queue; /**< Pointer to control message queue */
};

/** @brief Global neighbor table array */
extern struct neighbor_entry neighbor_table[MAX_NEIGHBORS];
/** @brief Current number of neighbors in table */
extern int neighbor_count;

/** @brief This node's willingness value */
extern uint8_t node_willingness;
/** @brief This node's IP address */
extern uint32_t node_id;
/** @brief Global message sequence number */
extern uint16_t message_seq_num;

#define MAX_QUEUE_SIZE 100  /**< Maximum size of control message queue */

/**
 * @brief Control message structure
 * 
 * Represents a single message in the control queue with metadata and retry logic.
 * Uses a linked list structure where each node stores a pointer to the actual message.
 */
struct control_message {
    uint8_t msg_type;        /**< Type of message (MSG_HELLO, MSG_TC, etc.) */
    time_t timestamp;        /**< Timestamp when message was created */
    time_t next_retry_time;  /**< Timestamp for next retry attempt */
    int retry_count;         /**< Number of retry attempts made */
    uint32_t destination_id; /**< Destination node ID (for tracking failed links) */
    void* message_ptr;       /**< Pointer to actual message structure (olsr_hello*, olsr_tc*, etc.) */
    struct control_message* next; /**< Pointer to next message in linked list */
};

/**
 * @brief Control queue structure
 * 
 * Linked list queue for managing OLSR control messages.
 * Used for queuing HELLO and TC messages before transmission to RRC/TDMA layer.
 */
struct control_queue {
    struct control_message* head;  /**< Pointer to first message in queue */
    struct control_message* tail;  /**< Pointer to last message in queue */
    int count;  /**< Current number of messages in queue */
};

/**
 * @brief Initialize the control queue
 * @param queue Pointer to the control queue to initialize
 */
void init_control_queue(struct control_queue* queue);

/**
 * @brief Push a message to the control queue
 * @param queue Pointer to the control queue
 * @param msg_type Type of the message
 * @param message_ptr Pointer to the message structure (olsr_hello*, olsr_tc*, etc.)
 * @return 0 on success, -1 on failure
 */
int push_to_control_queue(struct control_queue* queue, uint8_t msg_type, void* message_ptr);
/**
 * @brief Pop a message from the control queue
 * @param queue Pointer to the control queue
 * @return Pointer to control message, or NULL if queue is empty
 */
int pop_from_control_queue(struct control_queue* queue,struct control_message* out_msg);

/**
 * @brief Add a message to the control queue with retry capability
 * @param queue Pointer to the control queue
 * @param msg_type Type of the message
 * @param message_ptr Pointer to the message structure
 * @param destination_id Destination node ID for tracking
 * @return 0 on success, -1 on failure
 */
int add_message_with_retry(struct control_queue* queue, uint8_t msg_type, 
                          void* message_ptr, uint32_t destination_id);

/**
 * @brief Process retry queue for message retransmissions
 * @param queue Pointer to the control queue
 * @return Number of messages processed
 */
int process_retry_queue(struct control_queue* queue);

/**
 * @brief Cleanup expired messages from retry queue
 * @param queue Pointer to the control queue
 * @return Number of messages cleaned up
 */
int cleanup_expired_messages(struct control_queue* queue);

#endif

