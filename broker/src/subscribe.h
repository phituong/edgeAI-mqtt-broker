#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_context.h>
#include "message.h"
#include "client.h"


#define MAX_SUBSCRIBERS 10
#define MAX_PERSISTENT_SUBSCRIPTIONS 10
#ifndef TIME_WINDOW_SEC
#define TIME_WINDOW_SEC 10
#endif

struct mqtt_subscriber {
    bool valid;
    char client_id[MAX_CLIENT_ID_LEN];
    struct net_context *ctx;
    uint8_t index;
    uint32_t resub_window_start;
    uint8_t resubscription_count;
    uint8_t subscription_count;  // Add this field to store the subscription count
    uint8_t wildcard_subscription_count;
    
    struct {
        bool valid;
        char topic[MAX_TOPIC_LEN];
        uint8_t qos;
    } persistent_subscriptions[MAX_PERSISTENT_SUBSCRIPTIONS];
};

extern struct mqtt_subscriber subscribers[MAX_SUBSCRIBERS];


uint8_t get_resubscription_frequency(uint8_t subscriber_index);
uint8_t get_subscriber_index(const char *client_id);
uint8_t get_subscription_count(uint8_t subscriber_index);
uint8_t get_wildcard_subscription_count(uint8_t subscriber_index);
uint8_t track_resubscription(uint8_t subscriber_index);
bool is_persistent_subscription(const char *client_id, const char *topic);
void add_subscription(struct net_context *ctx, const char *client_id, const char *topic, uint8_t qos);
void clear_subscriber(const char *client_id);
void clear_subscription_topic(const char *client_id, const char *topic);
void send_unsuback(struct net_context *ctx, uint16_t packet_id);
void send_suback(struct net_context *ctx, uint16_t packet_id, uint8_t sub_count);
void send_pubrel(struct net_context *ctx, uint16_t packet_id);
void handle_pubcomp(struct net_context *ctx, uint8_t *buffer, size_t length);
void handle_pubrec(struct net_context *ctx, uint8_t *buffer, size_t length);
void handle_puback(struct net_context *ctx, uint8_t *buffer, size_t length);
int handle_unsubscribe(struct net_context *ctx, uint8_t *buffer, size_t length);
int handle_subscribe(struct net_context *ctx, uint8_t *buffer, size_t length);