#ifndef CLIENT_H
#define CLIENT_H


#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_context.h>
#include "message.h"


#define MAX_CLIENTS 20
#define MAX_CLIENT_ID_LEN 23  // 23 bytes
#define MIN_CLIENT_ID_LEN 5  // 5 bytes
#define ADDR_LEN NET_IPV4_ADDR_LEN
#ifndef TIME_WINDOW_SEC
#define TIME_WINDOW_SEC 10
#endif
#define CHECK_INTERVAL_MS 5000  // Check every 5 seconds



struct mqtt_client_info {
    bool connected;  
    char client_id[MAX_CLIENT_ID_LEN];  
    uint16_t keep_alive;    
    bool clean_session;  // Store Clean Session flag
	char ip_addr[ADDR_LEN];
	struct net_context *ctx;

    // Last Will & Testament (LWT)  
    bool has_will;  // This field tracks if a client has a Will message
    char will_topic[MAX_TOPIC_LEN];  
    char will_message[MAX_MESSAGE_LEN];  
    uint8_t will_qos;
    bool will_retain;

    uint32_t last_seen;
    uint32_t reconnect_window_start;
    uint8_t reconnect_count;   
};

extern struct mqtt_client_info clients[MAX_CLIENTS];



struct mqtt_client_info* get_client_info(struct net_context *ctx);
void generate_random_client_id(char *client_id, size_t min_length);
void handle_unexpected_disconnect(struct mqtt_client_info *client);
void check_client_connections();
uint8_t track_reconnect(uint8_t client_index);


#endif // CLIENT_H

