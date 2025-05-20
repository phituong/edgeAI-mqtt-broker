#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_context.h>
#include "broker.h"
#include "client.h"
#include "subscribe.h"
#include "features.h"


LOG_MODULE_REGISTER(SUBSCRIBE);
struct mqtt_subscriber subscribers[MAX_SUBSCRIBERS];


uint8_t get_subscription_count(uint8_t subscriber_index) {
    return subscribers[subscriber_index].subscription_count;  // Use the persistent subscription count
}

uint8_t get_wildcard_subscription_count(uint8_t subscriber_index) {
    return subscribers[subscriber_index].wildcard_subscription_count;  // Use the persistent subscription count
}

uint8_t get_resubscription_frequency(uint8_t subscriber_index) {
    LOG_INF("Subscriber %s resubscription frequency: %d resubscriptions/10 sec",
            subscribers[subscriber_index].client_id, subscribers[subscriber_index].resubscription_count);

    return subscribers[subscriber_index].resubscription_count;
}

uint8_t track_resubscription(uint8_t subscriber_index) {
    uint32_t now = k_uptime_get() / 1000;  // Convert to seconds

    if (subscribers[subscriber_index].resub_window_start == 0) {
        // First resubscription event, start window
        subscribers[subscriber_index].resub_window_start = now;
        subscribers[subscriber_index].resubscription_count = 1;
    } else if ((now - subscribers[subscriber_index].resub_window_start) <= TIME_WINDOW_SEC) {
        // Resubscription within the same 10-second window
        subscribers[subscriber_index].resubscription_count++;
    } else {
        // Window expired, reset
        subscribers[subscriber_index].resub_window_start = now;
        subscribers[subscriber_index].resubscription_count = 1;
    }

    LOG_INF("Subscriber %s resubscription frequency: %d resubscriptions/10 sec",
            subscribers[subscriber_index].client_id, subscribers[subscriber_index].resubscription_count);

    return subscribers[subscriber_index].resubscription_count;
}

void add_subscription(struct net_context *ctx, const char *client_id, const char *topic, uint8_t qos) {
    if (!ctx || !client_id || !topic) {
        LOG_ERR("Invalid subscription parameters");
        return;
    }

    int existing_index = -1;
    int empty_index = -1;
    bool is_wildcard = check_topic_wildcard(topic);

    // Search for existing client and topic
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (subscribers[i].valid && strcmp(subscribers[i].client_id, client_id) == 0) {
            existing_index = i; // Found existing client

            for (int j = 0; j < MAX_PERSISTENT_SUBSCRIPTIONS; j++) {
                if (subscribers[i].persistent_subscriptions[j].valid) {
                    if (strcmp(subscribers[i].persistent_subscriptions[j].topic, topic) == 0) {
                        subscribers[existing_index].persistent_subscriptions[j].qos = qos;
        				subscribers[existing_index].ctx = ctx;
                		track_resubscription(existing_index);
                		
                        LOG_WRN("Client %s already subscribed to topic %s", client_id, topic);
                        return; // **EXIT EARLY TO PREVENT DUPLICATES**
                    }
                }
            }
        } else if (empty_index == -1) {
            empty_index = i; // Track first empty slot
        }
    }

    if (existing_index != -1) {
        // Add new topic for existing client
        for (int j = 0; j < MAX_PERSISTENT_SUBSCRIPTIONS; j++) {
            if (!subscribers[existing_index].persistent_subscriptions[j].valid) {
                subscribers[existing_index].persistent_subscriptions[j].valid = true;
                strncpy(subscribers[existing_index].persistent_subscriptions[j].topic, topic, MAX_TOPIC_LEN - 1);
                subscribers[existing_index].persistent_subscriptions[j].topic[MAX_TOPIC_LEN - 1] = '\0';
                subscribers[existing_index].persistent_subscriptions[j].qos = qos;

                subscribers[existing_index].subscription_count++;
                if (is_wildcard) {
                    subscribers[existing_index].wildcard_subscription_count++;
                }

                track_resubscription(existing_index);
                LOG_INF("Client %s subscribed to new topic: %s (QoS=%d)", client_id, topic, qos);
                return;
            }
        }
        
        track_resubscription(existing_index);
        LOG_ERR("Client %s reached max subscriptions", client_id);
        return;
    }

    if (empty_index != -1) {
        // New subscriber
        subscribers[empty_index].valid = true;
        subscribers[empty_index].index = empty_index;
        subscribers[empty_index].ctx = ctx;
        strncpy(subscribers[empty_index].client_id, client_id, MAX_CLIENT_ID_LEN - 1);
        subscribers[empty_index].client_id[MAX_CLIENT_ID_LEN - 1] = '\0';
        subscribers[empty_index].subscription_count = 0;
        subscribers[empty_index].wildcard_subscription_count = 0;

        // Add first subscription
        subscribers[empty_index].persistent_subscriptions[0].valid = true;
        strncpy(subscribers[empty_index].persistent_subscriptions[0].topic, topic, MAX_TOPIC_LEN - 1);
        subscribers[empty_index].persistent_subscriptions[0].topic[MAX_TOPIC_LEN - 1] = '\0';
        subscribers[empty_index].persistent_subscriptions[0].qos = qos;

        subscribers[empty_index].subscription_count++;
        if (is_wildcard) {
            subscribers[empty_index].wildcard_subscription_count++;
        }

        track_resubscription(empty_index);
        LOG_INF("Added new client: %s subscribed to topic: %s (QoS=%d)", client_id, topic, qos);
    } else {
        LOG_ERR("No available slot for new subscription!");
    }
}

uint8_t get_subscriber_index(const char *client_id) {
    for (uint8_t i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (subscribers[i].valid && strcmp(subscribers[i].client_id, client_id) == 0) {
            return subscribers[i].index;
        }
    }
    return -1;  // Return an invalid index if not found
}

void clear_subscriber(const char *client_id) {
    if (!client_id) return;

    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (subscribers[i].valid && strcmp(subscribers[i].client_id, client_id) == 0) {
            LOG_INF("Removing subscriber: client_id=%s", client_id);

            // Store resubscription count before clearing
            uint8_t resub_count = subscribers[i].resubscription_count;

            // Clear all persistent subscriptions
            for (int j = 0; j < MAX_PERSISTENT_SUBSCRIPTIONS; j++) {
                if (subscribers[i].persistent_subscriptions[j].valid) {
                    LOG_INF("Removed persistent subscription: topic=%s", 
                            subscribers[i].persistent_subscriptions[j].topic);
                    subscribers[i].persistent_subscriptions[j].valid = false;
                }
            }

            // Securely erase subscriber data but keep resubscription count
            memset(&subscribers[i], 0, sizeof(subscribers[i]));
            subscribers[i].valid = false;  // Mark invalid
            subscribers[i].resubscription_count = resub_count;  // Restore count

            break;  // Stop after removing the subscriber
        }
    }
}

void clear_subscription_topic(const char *client_id, const char *topic) {
    if (!client_id || *client_id == '\0') {
        LOG_ERR("Invalid client ID for unsubscription");
        return;
    }

    bool found = false;

    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (subscribers[i].valid && strcmp(subscribers[i].client_id, client_id) == 0) {
            for (int j = 0; j < MAX_PERSISTENT_SUBSCRIPTIONS; j++) {
                if (subscribers[i].persistent_subscriptions[j].valid &&
                    (topic == NULL || strcmp(subscribers[i].persistent_subscriptions[j].topic, topic) == 0)) {

                    LOG_INF("Removing subscription: client_id=%s, topic=%s", 
                            client_id, subscribers[i].persistent_subscriptions[j].topic);

                    // Clear the subscription entry
                    subscribers[i].persistent_subscriptions[j].valid = false;
                    memset(subscribers[i].persistent_subscriptions[j].topic, 0, MAX_TOPIC_LEN);
                    subscribers[i].persistent_subscriptions[j].qos = 0;

                    if (subscribers[i].subscription_count > 0) {
                        subscribers[i].subscription_count--;  // Decrement only if non-zero
                    }

                    found = true;

                    // If removing a single topic, break after the first match
                    if (topic != NULL) {
                        break;
                    }
                }
            }
        }
    }

    if (!found) {
        if (topic) {
            LOG_WRN("No subscription found for client_id=%s on topic=%s", client_id, topic);
        } else {
            LOG_WRN("No subscriptions found for client_id=%s", client_id);
        }
    } else {
        if (topic) {
            LOG_INF("Subscription removed for client_id=%s, topic=%s", client_id, topic);
        } else {
            LOG_INF("All subscriptions removed for client_id=%s", client_id);
        }
    }
}

bool is_persistent_subscription(const char *client_id, const char *topic) {
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (subscribers[i].valid && strcmp(subscribers[i].client_id, client_id) == 0) {
            for (int j = 0; j < MAX_PERSISTENT_SUBSCRIPTIONS; j++) {
                if (subscribers[i].persistent_subscriptions[j].valid &&
                    strcmp(subscribers[i].persistent_subscriptions[j].topic, topic) == 0) {
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

void send_pubrel(struct net_context *ctx, uint16_t packet_id) {
    uint8_t pubrel_packet[4] = {MQTT_PUBREL, 0x02, (packet_id >> 8) & 0xFF, packet_id & 0xFF};
    net_context_send(ctx, pubrel_packet, sizeof(pubrel_packet), NULL, K_NO_WAIT, NULL);
    LOG_INF("MQTT PUBREL sent (Packet ID: %d)", packet_id);
}

void send_unsuback(struct net_context *ctx, uint16_t packet_id) {
    uint8_t unsuback_packet[4];
    unsuback_packet[0] = 0xB0;         // UNSUBACK packet type
    unsuback_packet[1] = 0x02;         // Remaining length is 2
    uint16_t pid_net = htons(packet_id);
    memcpy(&unsuback_packet[2], &pid_net, 2);
    
    net_context_send(ctx, unsuback_packet, sizeof(unsuback_packet), NULL, K_NO_WAIT, NULL);
    LOG_INF("Sent UNSUBACK for packet ID: %d", packet_id);
}

void send_suback(struct net_context *ctx, uint16_t packet_id, uint8_t sub_count) {
    uint8_t suback_packet[256];
    int pos = 0;
    
    // Fixed header: SUBACK packet type is 9 (0x90)
    suback_packet[pos++] = MQTT_SUBACK;
    
    // Remaining Length: 2 (for packet ID) + sub_count (1 byte per subscription)
    uint8_t rem_len = 2 + sub_count;
    suback_packet[pos++] = rem_len;
    
    // Packet Identifier (in network byte order)
    uint16_t pid_net = htons(packet_id);
    memcpy(&suback_packet[pos], &pid_net, 2);
    pos += 2;
    
    // For each subscription, return the granted QoS.
    // Here, we assume success and simply grant the requested QoS.
    for (int i = 0; i < sub_count; i++) {
        // If there's any error or restriction, you could return 0x80 (failure).
        suback_packet[pos++] = 0x00;  // Granted QoS 0 (adjust as needed for your implementation)
    }
    
    net_context_send(ctx, suback_packet, pos, NULL, K_NO_WAIT, NULL);
    LOG_INF("Sent SUBACK for packet ID: %d", packet_id);
}

int handle_subscribe(struct net_context *ctx, uint8_t *buffer, size_t length) {
    if (length < 8) {  // Minimum SUBSCRIBE packet: fixed header, remaining length, packet ID, and one subscription entry.
        LOG_ERR("SUBSCRIBE packet too short");
        return -1;
    }

    // Packet Identifier (next 2 bytes)
    uint16_t packet_id = (buffer[2] << 8) | buffer[3];
    int index = 4;

    // Temporary array to store subscribed topics for later delivery of retained messages
    char subscribed_topics[MAX_SUBSCRIBERS][MAX_TOPIC_LEN] = {0};
    int topic_count = 0;
    uint8_t sub_count = 0;

    // Retrieve client session from the network context
    struct mqtt_client_info *client = get_client_info(ctx);
    
    if (!client) {
		LOG_ERR("Client ID not found, rejecting subscription");
		return -1;
	}

    // Process the payload: one or more topic filter and QoS pairs
    while (index < length) {
        if (index + 3 > length) {
            LOG_ERR("Malformed SUBSCRIBE packet");
            return -1;
        }

        // Topic length (2 bytes)
        uint16_t topic_length = (buffer[index] << 8) | buffer[index + 1];
        index += 2;

        if (index + topic_length + 1 > length) {
            LOG_ERR("Malformed SUBSCRIBE packet: topic length exceeds packet length");
            return -1;
        }

        char topic[MAX_TOPIC_LEN] = {0};
        memcpy(topic, &buffer[index], topic_length);
        topic[topic_length] = '\0';
        index += topic_length;

        // Requested QoS (1 byte)
        uint8_t req_qos = buffer[index++];
        LOG_INF("Subscriber requested topic: %s with QoS: %d", topic, req_qos);

        // Add subscription for this client (active subscription)
        add_subscription(ctx, client->client_id, topic, req_qos);
        sub_count++;
        
        // Store the topic in a temporary array for delivering retained messages
        strncpy(subscribed_topics[topic_count], topic, MAX_TOPIC_LEN - 1);
        subscribed_topics[topic_count][MAX_TOPIC_LEN - 1] = '\0';
        topic_count++;
    }

    // Send SUBACK packet
    send_suback(ctx, packet_id, sub_count);

    // Deliver retained messages for each subscribed topic
    for (int j = 0; j < topic_count; j++) {
        deliver_retained_message(client->client_id, ctx, subscribed_topics[j]);
    }
    
    struct subscribe_features features = get_subscribe_features(buffer, length, client);
    
	LOG_WRN("Check features for SUBSCRIBE: QoS: %d, Clean Session: %d, Topic Wildcard: %d, "
        "Pending Queue: %d, Retained Queue: %d, Subscription Count: %d, Resubscription Frequency=%d, Wildcard Subscription Count=%d",
        features.qos, features.clean_session, features.topic_contain_wildcard,
        features.pending_queue_status, features.retained_queue_status, 
        features.subscription_count, features.resubscription_freq, features.wildcard_subscription_count);

    return 0;
}

int handle_unsubscribe(struct net_context *ctx, uint8_t *buffer, size_t length) {
    if (length < 4) {  // Minimum length: Fixed header, Remaining Length, Packet Identifier
        LOG_ERR("UNSUBSCRIBE packet too short");
        return -1;
    }
    
    // Fixed header for UNSUBSCRIBE should be 0xA2 (i.e. type 10, flags 0x02)
    if ((buffer[0] & 0xF0) != 0xA0) {
        LOG_ERR("Invalid UNSUBSCRIBE packet header: 0x%02X", buffer[0]);
        return -1;
    }
    
    // For simplicity, assume remaining length is encoded in one byte (buffer[1])
    uint8_t rem_len = buffer[1];
    if (length < 2 + rem_len) {
        LOG_ERR("Incomplete UNSUBSCRIBE packet");
        return -1;
    }
    
    // Packet Identifier (next 2 bytes)
    uint16_t packet_id = (buffer[2] << 8) | buffer[3];
    int index = 4;
    int unsub_count = 0;
    
    // Retrieve client session from the network context
    struct mqtt_client_info *client = get_client_info(ctx);
    
    // Process the payload: one or more topic filters
    while (index < length) {
        // Check if at least 3 bytes remain (2 for topic length, 1 for topic content at minimum)
        if (index + 3 > length) {
            LOG_ERR("Malformed UNSUBSCRIBE packet");
            return -1;
        }
        
        // Topic Length (2 bytes)
        uint16_t topic_length = (buffer[index] << 8) | buffer[index + 1];
        index += 2;
        
        if (index + topic_length > length) {
            LOG_ERR("Malformed UNSUBSCRIBE packet: topic length exceeds packet length");
            return -1;
        }
        
        char topic[MAX_TOPIC_LEN];
        memset(topic, 0, sizeof(topic));
        memcpy(topic, &buffer[index], topic_length);
        topic[topic_length] = '\0';
        index += topic_length;
        
        LOG_INF("Unsubscribing from topic: %s", topic);
        clear_subscription_topic(client->client_id, topic);
        unsub_count++;
    }
    
    // Send UNSUBACK packet
    send_unsuback(ctx, packet_id);
    LOG_INF("Handled UNSUBSCRIBE for packet ID: %d, unsubscribed %d topics", packet_id, unsub_count);
    
    return 0;
}

void handle_puback(struct net_context *ctx, uint8_t *buffer, size_t length) {
 	if (length < 4) {  // PUBACK must have at least 4 bytes
        LOG_ERR("PUBACK packet too short");
        return;
    }

    uint16_t packet_id = (buffer[2] << 8) | buffer[3];
    LOG_INF("Received PUBACK for packet ID: %d", packet_id);
    clear_pending_message(packet_id);  // Clear from pending list
}

void handle_pubrec(struct net_context *ctx, uint8_t *buffer, size_t length) {
 	if (length < 4) {  // PUBREC must have at least 4 bytes
        LOG_ERR("PUBREC packet too short");
        return;
    }
    
    uint16_t packet_id = (buffer[2] << 8) | buffer[3];
    LOG_INF("Received PUBREC for packet ID: %d", packet_id);

    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (pending_messages[i].valid && pending_messages[i].packet_id == packet_id) {
            if (pending_messages[i].state == PENDING_PUBREC) {
                pending_messages[i].state = PENDING_PUBCOMP;
                send_pubrel(ctx, packet_id);
                pending_messages[i].timestamp = k_uptime_get_32();
            }
            return;
        }
    }
}

void handle_pubcomp(struct net_context *ctx, uint8_t *buffer, size_t length) {
    if (length < 4) {
        LOG_ERR("Invalid PUBCOMP packet received.");
        return;
    }

    uint16_t packet_id = (buffer[2] << 8) | buffer[3];
    LOG_INF("Handling PUBCOMP for packet ID: %d", packet_id);
    clear_pending_message(packet_id);  // Clear from pending list
}
