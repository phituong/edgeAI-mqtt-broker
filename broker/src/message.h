#ifndef MESSAGE_H
#define MESSAGE_H

#include <zephyr/net/net_context.h>


#define MAX_PACKET_SIZE 256
#define MAX_TOPIC_LEN 100
#define MAX_MESSAGE_LEN 500
#define MAX_PENDING_MESSAGES 20
#define MAX_RETAINED_MESSAGES 20  // Adjust based on memory limits
#define QOS_TIMEOUT 5000 // 5 seconds

#ifndef MAX_CLIENT_ID_LEN  // 23 bytes
#define MAX_CLIENT_ID_LEN 23
#endif // MAX_CLIENT_ID_LEN


typedef enum {
    LOW,
    MEDIUM,
    HIGH
} queue_status_t;

// Struct to store retained messages
struct mqtt_retained_msg {
    char topic[MAX_TOPIC_LEN];
    uint8_t payload[MAX_MESSAGE_LEN];
    size_t payload_len;
    uint8_t qos;
    bool valid;
    char client_id[MAX_CLIENT_ID_LEN];  
};

typedef enum {
    PENDING_PUBACK,  // QoS 1: Waiting for PUBACK
    PENDING_PUBREC,  // QoS 2: Waiting for PUBREC
    PENDING_PUBREL,  // QoS 2: Waiting for PUBREL
    PENDING_PUBCOMP, // QoS 2: Waiting for PUBCOMP
    NONE_PENDING     // No pending message
} mqtt_qos_state_t;

struct mqtt_pending_msg {
    uint16_t packet_id;
    char topic[MAX_TOPIC_LEN];
    uint8_t payload[MAX_MESSAGE_LEN];
    size_t payload_len;
    uint8_t qos;
    bool dup;  // For retransmission flag
    bool valid;
    mqtt_qos_state_t state;  // New field to track QoS state
    uint32_t timestamp;  // For timeout handling
    char client_id[MAX_CLIENT_ID_LEN];  
};


extern struct mqtt_retained_msg retained_messages[MAX_RETAINED_MESSAGES];
extern struct mqtt_pending_msg pending_messages[MAX_PENDING_MESSAGES];


uint16_t generate_packet_id(void);
void store_will_message(const char *client_id, const char *topic, const char *message, uint8_t qos, bool retain);
void store_retained_message(const char *client_id, const char *topic, const uint8_t *payload, size_t len, uint8_t qos);
void store_pending_message(const char *client_id, uint16_t packet_id, const char *topic, const uint8_t *payload, size_t payload_len, uint8_t qos);
void restore_pending_messages(struct net_context *ctx, const char *client_id);
void clear_retained_message(const char *client_id, const char *topic);
void clear_pending_message(uint16_t packet_id);
void clear_pending_messages(const char *client_id);
void clear_will_message(const char *client_id);
void deliver_message(const char *topic, const uint8_t *payload, uint8_t pub_qos, bool dup);
void deliver_pending_message(const char *client_id, struct net_context *ctx,  uint16_t packet_id, uint8_t qos, const char *topic, const uint8_t *payload, bool dup);
void deliver_retained_message(const char *client_id, struct net_context *ctx, const char *topic);
void delivery_will_message(const char *client_id, const char *topic, const uint8_t *payload, size_t payload_len, uint8_t qos, bool retain);
uint8_t get_retained_queue_status(const char *topic);
uint8_t get_pending_queue_status(const char *client_id);

#endif // MESSAGE_HANDLER_H

