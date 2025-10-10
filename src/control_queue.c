#include <string.h>
#include <time.h>
#include "olsr.h"

void init_control_queue(struct control_queue* queue) {
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
}

int push_to_control_queue(struct control_queue* queue,
                          uint8_t msg_type,
                          const uint8_t* msg_data,
                          size_t data_size) {
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
    
    // Fill in the message
    slot->msg_type = msg_type;
    slot->timestamp = time(NULL);
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