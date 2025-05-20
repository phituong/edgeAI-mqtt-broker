#include <string.h>
#include <time.h>
#include <zephyr/random/random.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "message.h"
#include "client.h"


LOG_MODULE_REGISTER(CLIENT);

struct mqtt_client_info clients[MAX_CLIENTS];


void generate_random_client_id(char *client_id, size_t min_length) {
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    size_t charset_size = sizeof(charset) - 1;
    size_t length = (min_length < MAX_CLIENT_ID_LEN) ? min_length : MAX_CLIENT_ID_LEN;
    
    bool is_unique;
    do {
        is_unique = true;

        for (size_t i = 0; i < length; i++) {
            uint8_t rand_byte;
            sys_rand_get(&rand_byte, sizeof(rand_byte));  // Use Zephyr's random API
            client_id[i] = charset[rand_byte % charset_size];
        }
        client_id[length] = '\0'; // Null-terminate the string

        // Check for uniqueness in the clients buffer
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].connected && strcmp(clients[i].client_id, client_id) == 0) {
                is_unique = false;
                break; // Duplicate found, regenerate
            }
        }

    } while (!is_unique); // Repeat until a unique ID is generated
}

struct mqtt_client_info* get_client_info(struct net_context *ctx) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].connected && clients[i].ctx == ctx) {
            return &clients[i];
        }
    }
    return NULL;
}

void handle_unexpected_disconnect(struct mqtt_client_info *client) {
    client->connected = false;  // Mark as disconnected

    if (client->has_will) {
        LOG_INF("Publishing Will message: %s on topic: %s\n", client->will_message, client->will_topic);
        client->has_will = false;
        delivery_will_message(client->client_id, client->will_topic, client->will_message, strlen(client->will_message), client->will_qos, client->will_retain);
    }
}

void check_client_connections(void) {
    while (1) {
        uint32_t now = k_uptime_get_32();  // Current time in milliseconds

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].connected) {
                int64_t elapsed_time = (int64_t)now - (int64_t)clients[i].last_seen;

                // Check if this client appears timed out.
                if (elapsed_time > (clients[i].keep_alive * 1500)) {  // 1.5 * Keep-Alive
                    // Before disconnecting, check if another active client on the same IP exists.
                    bool same_ip_active = false;
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (j == i) {
                            continue;
                        }
                        if (clients[j].connected && (strcmp(clients[j].ip_addr, clients[i].ip_addr) == 0)) {
                            int64_t other_elapsed = (int64_t)now - (int64_t)clients[j].last_seen;
                            // If any other client on the same IP has not timed out, consider that as active.
                            if (other_elapsed <= (clients[j].keep_alive * 1500)) {
                                same_ip_active = true;
                                break;
                            }
                        }
                    }
                    if (same_ip_active) {
                        //LOG_INF("Client %s (IP: %s) not disconnected due to another active connection on same IP", 
                        //        clients[i].client_id, clients[i].ip_addr);
                    } else {
                        LOG_WRN("Client %s disconnected due to Keep-Alive timeout!", clients[i].client_id);
                        handle_unexpected_disconnect(&clients[i]);
                    }
                }
            }
        }
        k_sleep(K_MSEC(CHECK_INTERVAL_MS));  // Sleep to avoid busy looping
    }
}

uint8_t track_reconnect(uint8_t client_index) {
    uint32_t now = k_uptime_get() / 1000;  // Convert to seconds

    if (clients[client_index].reconnect_window_start == 0) {
        // First reconnect event, start window
        clients[client_index].reconnect_window_start = now;
        clients[client_index].reconnect_count = 1;
    } else if ((now - clients[client_index].reconnect_window_start) <= TIME_WINDOW_SEC) {
        // Reconnect within the same time window
        clients[client_index].reconnect_count++;
    } else {
        // Window expired, reset
        clients[client_index].reconnect_window_start = now;
        clients[client_index].reconnect_count = 1;
    }

    LOG_INF("Client %s reconnect frequency: %d reconnects/%d sec\n",
            clients[client_index].client_id, clients[client_index].reconnect_count, TIME_WINDOW_SEC);

    return clients[client_index].reconnect_count;
}