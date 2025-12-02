#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "../include/packet.h"
#include "../include/olsr.h"
#include <stdio.h>
#include <stdint.h>

void init_control_queue(struct control_queue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
}

int push_to_control_queue(struct control_queue* queue, uint8_t msg_type, void* message_ptr) {
    // Check if message pointer is valid
    if (!message_ptr) {
        printf("Error: NULL message pointer\n");
        return -1;
    }
    
    // Allocate new node for linked list
    struct control_message* new_node = (struct control_message*)malloc(sizeof(struct control_message));
    if (!new_node) {
        printf("Error: Failed to allocate memory for control message\n");
        return -1;
    }
    
    // Fill in the message (basic version without retry)
    new_node->msg_type = msg_type;
    new_node->timestamp = time(NULL);
    new_node->next_retry_time = 0;  // No retry for basic push
    new_node->retry_count = 0;
    new_node->destination_id = 0;   // No specific destination
    new_node->message_ptr = message_ptr;  // Store pointer to message structure
    new_node->next = NULL;
    
    // Add to linked list (append at tail)
    if (queue->tail == NULL) {
        // Queue is empty
        queue->head = new_node;
        queue->tail = new_node;
    } else {
        // Queue has elements, append at tail
        queue->tail->next = new_node;
        queue->tail = new_node;
    }
    
    queue->count++;
    
    return 0;  // Success
}

int pop_from_control_queue(struct control_queue* queue,
                           struct control_message* out_msg) {
    // Check if queue is empty
    if (queue->count == 0 || queue->head == NULL) {
        return -1;  // Error: queue empty
    }
    
    // Get the first message node
    struct control_message* node = queue->head;
    
    // COPY the message content to caller's buffer
    memcpy(out_msg, node, sizeof(struct control_message));
    
    // Update head pointer
    queue->head = node->next;
    
    // If queue is now empty, update tail
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    
    queue->count--;
    
    // Free the node (but NOT the message_ptr - caller owns it)
    free(node);
    
    return 0;  // Success
}

/**
 * @brief Add a message to the control queue with retry capability
 * 
 * Enhanced version of push_to_control_queue that includes retry logic
 * with exponential backoff for message retransmission.
 * 
 * @param queue Pointer to the control queue
 * @param msg_type Type of the message
 * @param message_ptr Pointer to the message structure
 * @param destination_id Destination node ID for tracking
 * @return 0 on success, -1 on failure
 */
int add_message_with_retry(struct control_queue* queue, uint8_t msg_type, 
                          void* message_ptr, uint32_t destination_id) {
    // Check if message pointer is valid
    if (!message_ptr) {
        printf("Error: NULL message pointer\n");
        return -1;
    }
    
    // Allocate new node for linked list
    struct control_message* new_node = (struct control_message*)malloc(sizeof(struct control_message));
    if (!new_node) {
        printf("Error: Failed to allocate memory for control message with retry\n");
        return -1;
    }
    
    // Fill in the message with retry information
    new_node->msg_type = msg_type;
    new_node->timestamp = time(NULL);
    new_node->next_retry_time = new_node->timestamp + RETRY_BASE_INTERVAL;  // First retry in 2 seconds
    new_node->retry_count = 0;
    new_node->destination_id = destination_id;
    new_node->message_ptr = message_ptr;  // Store pointer to message structure
    new_node->next = NULL;
    
    // Add to linked list (append at tail)
    if (queue->tail == NULL) {
        // Queue is empty
        queue->head = new_node;
        queue->tail = new_node;
    } else {
        // Queue has elements, append at tail
        queue->tail->next = new_node;
        queue->tail = new_node;
    }
    
    queue->count++;
    
    printf("Message queued with retry capability (dest=%u, retry_time=%ld)\n", 
           destination_id, (long)new_node->next_retry_time);
    
    return 0;  // Success
}

/**
 * @brief Process retry queue for message retransmissions
 * 
 * Scans the control queue for messages that are due for retry attempts.
 * Implements exponential backoff and removes messages that have exceeded
 * maximum retry attempts.
 * 
 * @param queue Pointer to the control queue
 * @return Number of messages processed for retry
 */
int process_retry_queue(struct control_queue* queue) {
    if (!queue || queue->count == 0 || queue->head == NULL) {
        return 0;  // Nothing to process
    }
    
    time_t now = time(NULL);
    int processed_count = 0;
    
    struct control_message* current = queue->head;
    struct control_message* prev = NULL;
    
    // Iterate through linked list
    while (current != NULL) {
        struct control_message* next = current->next;  // Save next pointer
        
        // Check if this message is due for retry
        if (current->retry_count > 0 && now >= current->next_retry_time) {
            
            if (current->retry_count >= MAX_RETRY_ATTEMPTS) {
                // Message has exceeded maximum retry attempts, remove it
                printf("Message to dest %u exceeded max retries (%d), removing from queue\n",
                       current->destination_id, MAX_RETRY_ATTEMPTS);
                
                // Remove node from list
                if (prev == NULL) {
                    // Removing head
                    queue->head = next;
                } else {
                    prev->next = next;
                }
                
                // Update tail if needed
                if (current == queue->tail) {
                    queue->tail = prev;
                }
                
                queue->count--;
                
                // Free the message structure (caller is responsible for managing message_ptr)
                free(current);
                
                current = next;
                continue;
            }
            
            // Calculate next retry time with exponential backoff
            int retry_interval = RETRY_BASE_INTERVAL << current->retry_count;  // 2^retry_count
            if (retry_interval > MAX_RETRY_INTERVAL) {
                retry_interval = MAX_RETRY_INTERVAL;
            }
            
            current->retry_count++;
            current->next_retry_time = now + retry_interval;
            
            printf("Retrying message to dest %u (attempt %d/%d, next retry in %d sec)\n",
                   current->destination_id, current->retry_count, MAX_RETRY_ATTEMPTS, retry_interval);
            
            processed_count++;
        }
        
        prev = current;
        current = next;
    }
    
    return processed_count;
}

/**
 * @brief Cleanup expired messages from retry queue
 * 
 * Removes messages that have been in the queue too long or have
 * exceeded their maximum retry attempts.
 * 
 * @param queue Pointer to the control queue
 * @return Number of messages cleaned up
 */
int cleanup_expired_messages(struct control_queue* queue) {
    if (!queue || queue->count == 0 || queue->head == NULL) {
        return 0;  // Nothing to cleanup
    }
    
    time_t now = time(NULL);
    int cleaned_count = 0;
    
    struct control_message* current = queue->head;
    struct control_message* prev = NULL;
    
    // Iterate through linked list
    while (current != NULL) {
        struct control_message* next = current->next;  // Save next pointer
        
        // Check if message should be removed
        time_t age = now - current->timestamp;
        int should_remove = 0;
        
        // Remove if too old (older than 60 seconds)
        if (age > 60) {
            printf("Removing expired message (age %ld sec)\n", (long)age);
            should_remove = 1;
        }
        
        // Remove if exceeded retry attempts
        if (current->retry_count > MAX_RETRY_ATTEMPTS) {
            printf("Removing message that exceeded retry limit\n");
            should_remove = 1;
        }
        
        if (should_remove) {
            // Remove node from list
            if (prev == NULL) {
                // Removing head
                queue->head = next;
            } else {
                prev->next = next;
            }
            
            // Update tail if needed
            if (current == queue->tail) {
                queue->tail = prev;
            }
            
            queue->count--;
            cleaned_count++;
            
            // Free the message node (caller manages message_ptr)
            free(current);
            
            current = next;
        } else {
            prev = current;
            current = next;
        }
    }
    
    if (cleaned_count > 0) {
        printf("Cleaned up %d expired messages from control queue\n", cleaned_count);
    }
    
    return cleaned_count;
}