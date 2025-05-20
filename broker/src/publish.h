#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_context.h>
#include "client.h"


struct publisher_stats {
    char client_id[MAX_CLIENT_ID_LEN];  // Store Client ID
    uint8_t wildcard_republish_count;  // Wildcard topic message count
    uint8_t normal_republish_count;
    uint64_t last_reset_time;  // Last time reset
    bool valid;  // Flag to mark if the publisher is active
    uint8_t index;  // Position in the array
};

struct publisher_manager {
    struct publisher_stats publishers[MAX_CLIENTS];  // List of publishers
    uint8_t publisher_count;  // Total registered publishers
};


int find_publisher_index(const char *client_id);
int add_publisher(const char *client_id);
void remove_publisher(const char *client_id);
uint8_t get_normal_republish_frequency(const char *client_id);
uint8_t get_wildcard_republish_frequency(const char *client_id);
void track_republish_frequency(const char *client_id, bool is_wildcard);
size_t encode_remaining_length(uint8_t *buf, size_t length);
size_t decode_remaining_length(uint8_t *buffer, size_t *remaining_length);
size_t construct_publish_packet(uint8_t *buffer, const char *topic, const uint8_t *payload, 
					size_t payload_len, uint8_t qos, bool dup, bool retain, uint16_t packet_id);
void send_puback(struct net_context *ctx, uint16_t packet_id);
void send_pubcomp(struct net_context *ctx, uint16_t packet_id);
void send_pubrec(struct net_context *ctx, uint16_t packet_id);
void handle_publish(struct net_context *ctx, uint8_t *buffer, size_t length);
void handle_pubrel(struct net_context *ctx, uint8_t *buffer, size_t length);




