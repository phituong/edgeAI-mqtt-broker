#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_context.h>
#include "message.h"
#include "subscribe.h"
#include "publish.h"


LOG_MODULE_REGISTER(MESSAGE);

struct mqtt_retained_msg retained_messages[MAX_RETAINED_MESSAGES];
struct mqtt_pending_msg pending_messages[MAX_PENDING_MESSAGES];


uint16_t generate_packet_id() {
    static uint16_t last_packet_id = 1;
    uint16_t reusable_id = 0;

    // Search for a reusable packet ID from invalid entries
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (!pending_messages[i].valid) {
            reusable_id = pending_messages[i].packet_id;
            break;  // Found an invalid entry, reuse its ID
        }
    }

    // If a reusable ID was found, check if it's unique
    if (reusable_id) {
        for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
            if (pending_messages[i].valid && pending_messages[i].packet_id == reusable_id) {
                reusable_id = 0;  // Conflict found, discard reuse
                break;
            }
        }
    }

    // If no valid reusable ID, generate a new unique one
    if (!reusable_id) {
        do {
            last_packet_id++;  // Increment to get a new ID

            // Ensure last_packet_id is unique
            bool conflict = false;
            for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
                if (pending_messages[i].valid && pending_messages[i].packet_id == last_packet_id) {
                    conflict = true;
                    break;
                }
            }
            if (!conflict) {
                break;  // Unique ID found
            }
        } while (1);
    }

    return reusable_id ? reusable_id : last_packet_id;
}

void store_will_message(const char *client_id, const char *topic, const char *message, uint8_t qos, bool retain) {
    LOG_INF("Store Will message: %s on topic: %s\n", message, topic);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(clients[i].client_id, client_id) == 0) {
            strncpy(clients[i].will_topic, topic, sizeof(clients[i].will_topic) - 1);
            strncpy(clients[i].will_message, message, sizeof(clients[i].will_message) - 1);
            clients[i].will_qos = qos;
            clients[i].will_retain = retain;
            clients[i].has_will = true;
            return;
        }
    }
}

void store_retained_message(const char *client_id, const char *topic, const uint8_t *payload, size_t len, uint8_t qos) {
    int empty_slot = -1;
    int i;  // Declare i outside the loop

    for (i = 0; i < MAX_RETAINED_MESSAGES; i++) {
        if (retained_messages[i].valid) {
            // If message exists for the same topic and client, update it
            if (strcmp(retained_messages[i].topic, topic) == 0 && strcmp(retained_messages[i].client_id, client_id) == 0) {
                goto store_message;  // Jump to store logic
            }
        } else if (empty_slot == -1) {
            // Remember the first empty slot
            empty_slot = i;
        }
    }

    // If no existing message, but an empty slot is found, use it
    if (empty_slot != -1) {
        i = empty_slot;
    } else {
        LOG_WRN("MQTT: Retained message storage full!");
        return;
    }

store_message:
    // Store topic
    strncpy(retained_messages[i].topic, topic, sizeof(retained_messages[i].topic) - 1);
    retained_messages[i].topic[sizeof(retained_messages[i].topic) - 1] = '\0';  // Null terminate

    // Store payload safely
    if (len > sizeof(retained_messages[i].payload)) {
        LOG_WRN("Truncating retained message for topic: %s", topic);
        len = sizeof(retained_messages[i].payload);
    }
    memcpy(retained_messages[i].payload, payload, len);
    retained_messages[i].payload_len = len;

    // Store client_id safely
    strncpy(retained_messages[i].client_id, client_id, MAX_CLIENT_ID_LEN - 1);
    retained_messages[i].client_id[MAX_CLIENT_ID_LEN - 1] = '\0';  // Null terminate

    // Store QoS and mark as valid
    retained_messages[i].qos = qos;
    retained_messages[i].valid = true;

    LOG_INF("Stored retained message for topic: %s", topic);
}

void store_pending_message(const char *client_id, uint16_t packet_id, const char *topic, const uint8_t *payload, size_t payload_len, uint8_t qos) {
    int empty_index = -1;

    // Find the first available empty slot
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (!pending_messages[i].valid) {
            empty_index = i;
            break;  // Stop at the first empty slot
        }
    }

    if (empty_index != -1) {
        // Store message in the empty slot
        pending_messages[empty_index].valid = true;
        pending_messages[empty_index].packet_id = packet_id;
        strncpy(pending_messages[empty_index].topic, topic, MAX_TOPIC_LEN - 1);
        pending_messages[empty_index].topic[MAX_TOPIC_LEN - 1] = '\0';  // Ensure null termination

        size_t copy_len = (payload_len < MAX_MESSAGE_LEN) ? payload_len : MAX_MESSAGE_LEN - 1;
        memcpy(pending_messages[empty_index].payload, payload, copy_len);
        pending_messages[empty_index].payload[copy_len] = '\0';  // Ensure null termination

  		strncpy(pending_messages[empty_index].client_id, client_id, MAX_CLIENT_ID_LEN - 1);
        pending_messages[empty_index].client_id[MAX_CLIENT_ID_LEN - 1] = '\0';  // Ensure null termination

        pending_messages[empty_index].payload_len = copy_len;
        pending_messages[empty_index].qos = qos;
        pending_messages[empty_index].timestamp = k_uptime_get_32();

        // Set initial state based on QoS
        if (qos == 1) {
            pending_messages[empty_index].state = PENDING_PUBACK;
        } else if (qos == 2) {
            pending_messages[empty_index].state = PENDING_PUBREC;
        } else {
            pending_messages[empty_index].valid = false; // QoS 0 doesn't need tracking
        }

        LOG_INF("Stored new pending message at index %d with packet ID: %d, topic: %s",
                empty_index, packet_id, topic);
    } else {
        LOG_ERR("No space for new pending messages!");
    }
}

void restore_pending_messages(struct net_context *ctx, const char *client_id) {
    LOG_INF("Restoring pending messages for client: %s", client_id);
    bool restore_pending_msg_found = false;

    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (pending_messages[i].valid && is_persistent_subscription(client_id, pending_messages[i].topic)) {
        	if (strcmp(pending_messages[i].client_id, client_id) == 0) {
				LOG_INF("Restoring message (Packet ID: %u) for topic: %s", 
						pending_messages[i].packet_id, pending_messages[i].topic);
				restore_pending_msg_found = true;
				pending_messages[i].valid = false;
				// Check if the message should be re-sent based on QoS state
				if (pending_messages[i].qos > 0) {
					if (pending_messages[i].state == PENDING_PUBACK || pending_messages[i].state == PENDING_PUBREC ||
						pending_messages[i].state == PENDING_PUBCOMP) {
						// Mark message as duplicate for retransmission
						pending_messages[i].dup = true;
						pending_messages[i].timestamp = k_uptime_get_32();  // Update timestamp
						deliver_pending_message(pending_messages[i].client_id, ctx, pending_messages[i].packet_id, pending_messages[i].qos,
							pending_messages[i].topic, pending_messages[i].payload, pending_messages[i].dup);
					}
				}
			}
        }
    }
    
    if (!restore_pending_msg_found) {
        LOG_INF("No restore pending messages found for client: %s", client_id);
    }
}

void clear_retained_message(const char *client_id, const char *topic) {
    for (int i = 0; i < MAX_RETAINED_MESSAGES; i++) {
        if (retained_messages[i].valid && 
            strcmp(retained_messages[i].topic, topic) == 0 &&
            strcmp(retained_messages[i].client_id, client_id) == 0) {
            
            // Clear stored data
            memset(retained_messages[i].topic, 0, sizeof(retained_messages[i].topic));
            memset(retained_messages[i].client_id, 0, sizeof(retained_messages[i].client_id));
            memset(retained_messages[i].payload, 0, retained_messages[i].payload_len);
            
            // Reset metadata
            retained_messages[i].payload_len = 0;
            retained_messages[i].qos = 0;
            retained_messages[i].valid = false;

            LOG_INF("Cleared retained message for topic: %s", topic);
            return;
        }
    }

    LOG_WRN("MQTT: No retained message found for topic: %s", topic);
}

void clear_pending_message(uint16_t packet_id) {
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (pending_messages[i].valid && pending_messages[i].packet_id == packet_id
        	&& (pending_messages[i].state == PENDING_PUBCOMP || pending_messages[i].state == PENDING_PUBACK)) {
            pending_messages[i].valid = false;
            pending_messages[i].state = NONE_PENDING;
            pending_messages[i].packet_id = 0;
            LOG_INF("Cleared pending message for packet ID: %d", packet_id);
            return;
        }
    }
}

void clear_pending_messages(const char *client_id) {
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (pending_messages[i].valid &&
            strcmp(pending_messages[i].client_id, client_id) == 0) {
            pending_messages[i].valid = false;  // Mark as invalid
            LOG_INF("Cleared pending message: client=%s, packet_id=%d, topic=%s", 
                    client_id, pending_messages[i].packet_id, pending_messages[i].topic);
        }
    }
}

void clear_will_message(const char *client_id) {
    LOG_INF("Clearing Will message for client: %s", client_id);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(clients[i].client_id, client_id) == 0) {
            memset(clients[i].will_topic, 0, sizeof(clients[i].will_topic));
            memset(clients[i].will_message, 0, sizeof(clients[i].will_message));
            clients[i].will_qos = 0;
            clients[i].will_retain = false;
            clients[i].has_will = false;
            return;
        }
    }
}

void deliver_message(const char *topic, const uint8_t *payload, uint8_t pub_qos, bool dup) {
    LOG_INF("Delivering message to subscribers: Topic=%s, Payload=%s", topic, payload);

    bool subscriber_found = false;

    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (subscribers[i].valid) {
            for (int j = 0; j < subscribers[i].subscription_count; j++) {
                if (subscribers[i].persistent_subscriptions[j].valid &&
                    strcmp(subscribers[i].persistent_subscriptions[j].topic, topic) == 0) {
                    subscriber_found = true;
                    
                    uint8_t sub_qos = subscribers[i].persistent_subscriptions[j].qos;
                    uint8_t final_qos = (pub_qos < sub_qos) ? pub_qos : sub_qos;  // Use the lower QoS
                    uint16_t packet_id = generate_packet_id();
                    
                    // Construct MQTT PUBLISH packet for this subscriber
                    uint8_t publish_packet[MAX_PACKET_SIZE];
                    bool retain = false;  // Retain is usually false when delivering live messages
                    size_t packet_len = construct_publish_packet(publish_packet, topic, payload,
                                                                 strlen((const char *)payload),
                                                                 final_qos, dup, retain, packet_id);
                    
                    if (final_qos != 0) {
                        // Store message for QoS 1 & 2 to enable retransmissions
                        store_pending_message(subscribers[i].client_id, packet_id, topic,
                                              payload, strlen((const char *)payload), final_qos);
                    }

                    if (packet_len > 0) {
                        net_context_send(subscribers[i].ctx, publish_packet, packet_len,
                                         NULL, K_NO_WAIT, NULL);
                        LOG_INF("Message sent to subscriber: client_id=%s, topic=%s, QoS=%d, Packet ID=%d",
                                subscribers[i].client_id, topic, final_qos, packet_id);
                    } else {
                        LOG_ERR("Failed to construct MQTT PUBLISH packet!");
                    }
                }
            }
        }
    }

    if (!subscriber_found) {
        LOG_INF("No subscribers found for topic: %s", topic);
    }
}

void deliver_pending_message(const char *client_id, struct net_context *ctx, uint16_t packet_id, uint8_t qos, const char *topic, const uint8_t *payload, bool dup) {
    // Construct MQTT PUBLISH packet for this subscriber
	uint8_t publish_packet[MAX_PACKET_SIZE];
	bool retain = false;  // For delivery, retain is usually false (the retain flag is already set in stored retained message)
	size_t packet_len = construct_publish_packet(publish_packet, topic, payload, strlen((const char *)payload), qos, dup, retain, packet_id);

	// For QoS 1 & 2, store the message to maintain tracking for retransmissions.
	store_pending_message(client_id, packet_id, topic, payload, strlen((const char *)payload), qos);
	
	if (packet_len > 0) {
		net_context_send(ctx, publish_packet, packet_len, NULL, K_NO_WAIT, NULL);
		LOG_INF("Pending message sent to subscriber on topic: %s with QoS=%d, Packet ID=%d", topic, qos, packet_id);
	} else {
		LOG_ERR("Failed to construct MQTT PUBLISH packet!");
	}
}

void deliver_retained_message(const char *client_id, struct net_context *ctx, const char *topic) {
    // Construct MQTT PUBLISH packet for this subscriber
    uint8_t publish_packet[MAX_PACKET_SIZE];

    for (int i = 0; i < MAX_RETAINED_MESSAGES; i++) {
        if (retained_messages[i].valid && strcmp(retained_messages[i].topic, topic) == 0) {
            uint8_t qos = retained_messages[i].qos;
            bool dup = false;
            bool retain = false;
            uint16_t packet_id = generate_packet_id();
            size_t payload_len = retained_messages[i].payload_len;

            // Ensure the publish packet size is within the allowed limit
            size_t packet_len = construct_publish_packet(publish_packet, topic, retained_messages[i].payload, 
                                                         payload_len, qos, dup, retain, packet_id);
            
            if (packet_len == 0) {
                LOG_ERR("Failed to construct MQTT PUBLISH packet!");
                return;
            }

            // For QoS 1 & 2, store the message for tracking retransmissions
            if (qos > 0) {
                store_pending_message(client_id, packet_id, topic, retained_messages[i].payload, 
                                      payload_len, qos);
            }

            // Send the packet
            int ret = net_context_send(ctx, publish_packet, packet_len, NULL, K_NO_WAIT, NULL);
            if (ret < 0) {
                LOG_ERR("Failed to send retained message on topic: %s (error: %d)", topic, ret);
                return;
            }

            LOG_INF("Retained message sent to subscriber on topic: %s with QoS=%d, Packet ID=%d", 
                    topic, qos, packet_id);
            return;
        }
    }

    LOG_WRN("No retained message found for topic: %s", topic);
}

void delivery_will_message(const char *client_id, const char *topic, const uint8_t *payload,
							size_t payload_len, uint8_t qos, bool retain) {
    LOG_INF("Processing Will Message: Topic=%s, QoS=%d, Retain=%d", topic, qos, retain);
    
    if (retain) {
        if (payload_len == 0) {
            LOG_INF("Clearing retained Will message for topic: %s, client_id: %s", topic, client_id);
            clear_retained_message(client_id, topic);
        } else {
            LOG_INF("Storing retained Will message for topic: %s, client_id: %s", topic, client_id);
            store_retained_message(client_id, topic, payload, payload_len, qos);
        }
    }
    else {
     	LOG_INF("Clearing retained Will message for topic: %s, client_id: %s", topic, client_id);
    	clear_retained_message(client_id, topic);
	}
	
	bool dup = false;
	deliver_message(topic, payload, qos, dup);
}

uint8_t get_retained_queue_status(const char *topic) {
    uint8_t retained_count = 0;

    for (int i = 0; i < MAX_RETAINED_MESSAGES; i++) {
        if (retained_messages[i].valid && strcmp(retained_messages[i].topic, topic) == 0) {
            retained_count++;
        }
    }
	
	return retained_count;
}

uint8_t get_pending_queue_status(const char *client_id) {
    uint8_t pending_count = 0;

    for (int j = 0; j < MAX_PENDING_MESSAGES; j++) {
		if (pending_messages[j].valid && strcmp(pending_messages[j].client_id, client_id) == 0) {
			pending_count++;
		}
    }
	return pending_count;
}

