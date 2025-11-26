#include <string.h>
#include <time.h>
#include "../include/packet.h"
#include "../include/olsr.h"
#include <stdio.h>
#include <stdint.h>

void init_control_queue(struct control_queue* queue) {
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
}

int push_to_control_queue(struct control_queue* queue,uint8_t msg_type,const uint8_t* msg_data,size_t data_size) {
    // Check if queue is full   
    if (queue->count >= MAX_QUEUE_SIZE) {
        return -1;  // Error: queue full
    }
    
    // Check if data fits in buffer
    if (data_size > MAX_MESSAGE_SIZE) {
       return -1;  // Error: message too large
    }
    
    // Get the slot where we'll store this message
    struct control_message* slot = &queue->messages[queue->rear];
    
    // Fill in the message (basic version without retry)
    slot->msg_type = msg_type;
    slot->timestamp = time(NULL);
    slot->next_retry_time = 0;  // No retry for basic push
    slot->retry_count = 0;
    slot->destination_id = 0;   // No specific destination
    slot->data_size = data_size;
    
    // COPY the data into our buffer
    memcpy(slot->msg_data, msg_data, data_size);
    
    // Update queue pointers
    queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;  // Circular
    queue->count++;
    
    return 0;  // Success
}

int pop_from_control_queue(struct control_queue* queue,
                           struct control_message* out_msg) {
    // Check if queue is empty
    if (queue->count == 0) {
        return -1;  // Error: queue empty
    }
    
    // COPY the entire message to caller's buffer
    memcpy(out_msg, &queue->messages[queue->front], 
           sizeof(struct control_message));
    
    // Update queue pointers
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;  // Circular
    queue->count--;
    
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
 * @param msg_data Pointer to message data
 * @param data_size Size of the message data
 * @param destination_id Destination node ID for tracking
 * @return 0 on success, -1 on failure
 */
int add_message_with_retry(struct control_queue* queue, uint8_t msg_type, 
                          const uint8_t* msg_data, size_t data_size, uint32_t destination_id) {
    // Check if queue is full
    if (queue->count >= MAX_QUEUE_SIZE) {
        printf("Error: Control queue is full, cannot add message with retry\n");
        return -1;  // Error: queue full
    }
    
    // Check if data fits in buffer
    if (data_size > MAX_MESSAGE_SIZE) {
        printf("Error: Message too large for retry queue\n");
        return -1;  // Error: message too large
    }
    
    // Get the slot where we'll store this message
    struct control_message* slot = &queue->messages[queue->rear];
    
    // Fill in the message with retry information
    slot->msg_type = msg_type;
    slot->timestamp = time(NULL);
    slot->next_retry_time = slot->timestamp + RETRY_BASE_INTERVAL;  // First retry in 2 seconds
    slot->retry_count = 0;
    slot->destination_id = destination_id;
    slot->data_size = data_size;
    
    // COPY the data into our buffer
    memcpy(slot->msg_data, msg_data, data_size);
    
    // Update queue pointers
    queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;  // Circular
    queue->count++;
    
    printf("Message queued with retry capability (dest=%u, retry_time=%ld)\n", 
           destination_id, (long)slot->next_retry_time);
    
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
    if (!queue || queue->count == 0) {
        return 0;  // Nothing to process
    }
    
    time_t now = time(NULL);
    int processed_count = 0;
    int write_pos = queue->front;
    int messages_to_check = queue->count;
    
    // Process messages in circular queue
    for (int i = 0; i < messages_to_check; i++) {
        int current_pos = (queue->front + i) % MAX_QUEUE_SIZE;
        struct control_message* msg = &queue->messages[current_pos];
        
        // Check if this message is due for retry
        if (msg->retry_count > 0 && now >= msg->next_retry_time) {
            
            if (msg->retry_count >= MAX_RETRY_ATTEMPTS) {
                // Message has exceeded maximum retry attempts, remove it
                printf("Message to dest %u exceeded max retries (%d), removing from queue\n",
                       msg->destination_id, MAX_RETRY_ATTEMPTS);
                // Don't copy this message (effectively removing it)
                queue->count--;
                continue;
            }
            
            // Calculate next retry time with exponential backoff
            int retry_interval = RETRY_BASE_INTERVAL << msg->retry_count;  // 2^retry_count
            if (retry_interval > MAX_RETRY_INTERVAL) {
                retry_interval = MAX_RETRY_INTERVAL;
            }
            
            msg->retry_count++;
            msg->next_retry_time = now + retry_interval;
            
            printf("Retrying message to dest %u (attempt %d/%d, next retry in %d sec)\n",
                   msg->destination_id, msg->retry_count, MAX_RETRY_ATTEMPTS, retry_interval);
            
            // Message will be retransmitted by MAC layer when popped
            processed_count++;
        }
        
        // Keep the message in queue for now (retry logic manages removal)
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
    if (!queue || queue->count == 0) {
        return 0;  // Nothing to cleanup
    }
    
    time_t now = time(NULL);
    int cleaned_count = 0;
    int write_pos = 0;
    
    // Create temporary array to hold valid messages
    struct control_message temp_messages[MAX_QUEUE_SIZE];
    int temp_count = 0;
    
    // Check all messages in queue
    for (int i = 0; i < queue->count; i++) {
        int current_pos = (queue->front + i) % MAX_QUEUE_SIZE;
        struct control_message* msg = &queue->messages[current_pos];
        
        // Check if message should be kept
        time_t age = now - msg->timestamp;
        int should_keep = 1;
        
        // Remove if too old (older than 60 seconds)
        if (age > 60) {
            printf("Removing expired message (age %ld sec)\n", (long)age);
            should_keep = 0;
            cleaned_count++;
        }
        
        // Remove if exceeded retry attempts
        if (msg->retry_count > MAX_RETRY_ATTEMPTS) {
            printf("Removing message that exceeded retry limit\n");
            should_keep = 0;
            cleaned_count++;
        }
        
        // Keep valid messages
        if (should_keep) {
            temp_messages[temp_count] = *msg;
            temp_count++;
        }
    }
    
    // Rebuild queue with valid messages
    queue->front = 0;
    queue->rear = temp_count;
    queue->count = temp_count;
    
    for (int i = 0; i < temp_count; i++) {
        queue->messages[i] = temp_messages[i];
    }
    
    if (cleaned_count > 0) {
        printf("Cleaned up %d expired messages from control queue\n", cleaned_count);
    }
    
    return cleaned_count;
}