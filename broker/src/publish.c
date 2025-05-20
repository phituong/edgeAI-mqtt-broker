#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_context.h>
#include "broker.h"
#include "message.h"
#include "client.h"
#include "publish.h"
#include "features.h"


LOG_MODULE_REGISTER(PUBLISH);

uint16_t temp_packet_id;
uint8_t temp_qos;
char temp_topic[MAX_TOPIC_LEN];
char temp_payload[MAX_MESSAGE_LEN];
bool temp_dup;
static struct publisher_manager publisher = { .publisher_count = 0 };


// Find publisher index by client_id
int find_publisher_index(const char *client_id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (publisher.publishers[i].valid &&
            strcmp(publisher.publishers[i].client_id, client_id) == 0) {
            return i;
        }
    }
    return -1; // Not found
}

// Add a new publisher or reuse an empty slot
int add_publisher(const char *client_id) {
    int index = find_publisher_index(client_id);
    if (index != -1) {
        return index; // Publisher already exists
    }

    // Find an empty slot
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!publisher.publishers[i].valid) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        LOG_ERR("Max publisher limit reached");
        return -1;
    }

    // Initialize publisher entry
    strncpy(publisher.publishers[index].client_id, client_id, MAX_CLIENT_ID_LEN - 1);
    publisher.publishers[index].client_id[MAX_CLIENT_ID_LEN - 1] = '\0'; // Ensure null-termination
    publisher.publishers[index].wildcard_republish_count = 0;
    publisher.publishers[index].normal_republish_count = 0;
    publisher.publishers[index].last_reset_time = k_uptime_get(); // Set last reset time
    publisher.publishers[index].valid = true;
    publisher.publishers[index].index = index;  // Store index in struct

    publisher.publisher_count++;

    return index;
}

// Remove a publisher (when client disconnects)
void remove_publisher(const char *client_id) {
    int index = find_publisher_index(client_id);
    if (index == -1) {
        return;
    }

    publisher.publishers[index].valid = false;  // Mark slot as reusable
    publisher.publisher_count--;

    LOG_INF("Publisher %s disconnected and removed", client_id);
}

// Get wildcard republish frequency
uint8_t get_wildcard_republish_frequency(const char *client_id) {
    int index = find_publisher_index(client_id);
    if (index == -1) {
        return 0;
    }
    return publisher.publishers[index].wildcard_republish_count;
}

// Get normal republish frequency
uint8_t get_normal_republish_frequency(const char *client_id) {
    int index = find_publisher_index(client_id);
    if (index == -1) {
        return 0;
    }
    return publisher.publishers[index].normal_republish_count;
}

// Track both normal and wildcard republish frequencies
void track_republish_frequency(const char *client_id, bool is_wildcard) {
    int index = find_publisher_index(client_id);
    if (index == -1) {
        index = add_publisher(client_id);
        if (index == -1) {
            return;
        }
    }

    uint32_t now = k_uptime_get() / 1000;  // Convert to seconds

    if (publisher.publishers[index].last_reset_time == 0 ||
        (now - publisher.publishers[index].last_reset_time) > TIME_WINDOW_SEC) {
        // First publish event or window expired -> reset both counters
        publisher.publishers[index].last_reset_time = now;
        publisher.publishers[index].wildcard_republish_count = 0;
        publisher.publishers[index].normal_republish_count = 0;
    }

    // Update both counters
    publisher.publishers[index].normal_republish_count++;
    if (is_wildcard) {
        publisher.publishers[index].wildcard_republish_count++;
    }

    LOG_INF("Publisher %s - Republish frequency: %d, Wildcard Republish frequency: %d (per 10 sec)",
            client_id, publisher.publishers[index].normal_republish_count, 
            publisher.publishers[index].wildcard_republish_count);
}

size_t encode_remaining_length(uint8_t *buf, size_t length) {
    size_t pos = 0;
    do {
        uint8_t encodedByte = length % 128;
        length /= 128;
        // If there are more digits to encode, set the top bit of this digit
        if (length > 0) {
            encodedByte |= 0x80;
        }
        buf[pos++] = encodedByte;
    } while (length > 0);
    return pos;
}

// Returns number of bytes used to encode remaining length, and sets *remaining_length.
size_t decode_remaining_length(uint8_t *buffer, size_t *remaining_length) {
    size_t multiplier = 1;
    size_t value = 0;
    size_t pos = 0;
    uint8_t encodedByte;

    do {
        encodedByte = buffer[pos++];
        value += (encodedByte & 127) * multiplier;
        multiplier *= 128;
        // MQTT allows a maximum of 4 bytes for remaining length
        if (pos > 4) {
            break;
        }
    } while ((encodedByte & 128) != 0);

    *remaining_length = value;
    return pos;  // Number of bytes used for remaining length
}


size_t construct_publish_packet(uint8_t *buffer, const char *topic, const uint8_t *payload, size_t payload_len, uint8_t qos, bool dup, bool retain, uint16_t packet_id) {
    size_t pos = 0;
    // Fixed Header: start with PUBLISH (0x30)
    uint8_t header = 0x30;
    if (dup) {
        header |= 0x08;  // Set DUP flag (bit 3)
    }
    header |= (qos << 1);  // Set QoS bits (bits 1-2)
    if (retain) {
        header |= 0x01;  // Set Retain flag (bit 0)
    }
    buffer[pos++] = header;
    
    // Calculate Remaining Length:
    // Remaining Length = 2 bytes for Topic Length + topic string length + payload length
    //                 + (if qos > 0, 2 bytes for Packet Identifier)
    size_t topic_len = strlen(topic);
    size_t rem_len = 2 + topic_len + payload_len;
    if (qos > 0) {
        rem_len += 2;
    }
    
    // Encode Remaining Length
    pos += encode_remaining_length(&buffer[pos], rem_len);
    
    // Write Topic Length (big endian)
    uint16_t topic_len_be = htons(topic_len);
    memcpy(&buffer[pos], &topic_len_be, 2);
    pos += 2;
    
    // Write Topic string
    memcpy(&buffer[pos], topic, topic_len);
    pos += topic_len;
    
    // Write Packet Identifier if QoS > 0
    if (qos > 0) {
        uint16_t pid_be = htons(packet_id);
        memcpy(&buffer[pos], &pid_be, 2);
        pos += 2;
    }
    
    // Write Payload
    memcpy(&buffer[pos], payload, payload_len);
    pos += payload_len;
    
    return pos;  // Total packet length
}

void send_puback(struct net_context *ctx, uint16_t packet_id) {
    uint8_t puback_packet[4] = {MQTT_PUBACK, 0x02, (packet_id >> 8), (packet_id & 0xFF)};
    net_context_send(ctx, puback_packet, sizeof(puback_packet), NULL, K_NO_WAIT, NULL);
    LOG_INF("MQTT PUBACK sent (Packet ID: %d)", packet_id);
}

void send_pubrec(struct net_context *ctx, uint16_t packet_id) {
    uint8_t pubrec_packet[4] = {MQTT_PUBREC, 0x02, (packet_id >> 8), (packet_id & 0xFF)};
    net_context_send(ctx, pubrec_packet, sizeof(pubrec_packet), NULL, K_NO_WAIT, NULL);
    LOG_INF("MQTT PUBREC sent (Packet ID: %d)", packet_id);
}

void send_pubcomp(struct net_context *ctx, uint16_t packet_id) {
    uint8_t pubcomp_packet[4] = {MQTT_PUBCOMP, 0x02, (packet_id >> 8) & 0xFF, packet_id & 0xFF};
    net_context_send(ctx, pubcomp_packet, sizeof(pubcomp_packet), NULL, K_NO_WAIT, NULL);
    LOG_INF("MQTT PUBCOMP sent (Packet ID: %d)", packet_id);
}

void handle_publish(struct net_context *ctx, uint8_t *buffer, size_t length) {
    if (length < 2) {  // At least fixed header and one byte for remaining length
        LOG_ERR("PUBLISH packet too short");
        return;
    }
    
    // Retrieve client session from the network context
    struct mqtt_client_info *client = get_client_info(ctx);
    
    if (!client) {
		LOG_ERR("Client ID not found, rejecting subscription");
		return;
	}
	
	struct publish_features features = get_publish_features(buffer, length, client);
    LOG_WRN("Check features for PUBLISH: QoS=%d, Retain=%d, Payload Size=%d, Topic Wildcard=%d, Pending Queue=%d, "
    		"Retained Queue=%d, Republish Frequency=%d",
            features.qos, features.retain, features.payload_size, features.topic_contain_wildcard,
            features.pending_queue_status, features.retained_queue_status, features.republish_freq);

    uint8_t fixed_header = buffer[0];
    uint8_t qos = (fixed_header & 0x06) >> 1;
    bool dup = (fixed_header & 0x08) ? true : false;
    bool retain = (fixed_header & 0x01) ? true : false;

    // Decode remaining length
    size_t remaining_length;
    size_t rem_len_bytes = decode_remaining_length(&buffer[1], &remaining_length);

    // Calculate starting index of variable header
    int index = 1 + rem_len_bytes;
    if (index + 2 > length) {
        LOG_ERR("Packet too short for topic length field");
        return;
    }

    // Now read the topic length (2 bytes)
    uint16_t topic_length = (buffer[index] << 8) | buffer[index + 1];
    index += 2;

    if (index + topic_length > length) {
        LOG_ERR("Invalid topic length: topic_length=%d, index=%d, total_length=%d", topic_length, index, length);
        return;
    }

    char topic[MAX_TOPIC_LEN];
    memcpy(topic, &buffer[index], topic_length);
    topic[topic_length] = '\0';
    index += topic_length;

    // If QoS > 0, Packet Identifier follows
    uint16_t packet_id = 0;
    if (qos > 0) {
        if (index + 2 > length) {
            LOG_ERR("Packet too short for Packet ID");
            return;
        }
        packet_id = (buffer[index] << 8) | buffer[index + 1];
        index += 2;
    }

    // Remaining bytes are payload
    int payload_len = length - index;
    char payload[MAX_MESSAGE_LEN];
    memcpy(payload, &buffer[index], payload_len);
    payload[payload_len] = '\0';

    LOG_INF("Received PUBLISH: Topic=%s, QoS=%d, DUP=%d, Retain=%d", topic, qos, dup, retain);

    if (retain) {
        if (payload_len == 0) {
            LOG_INF("Clearing retained message for topic: %s, client id: %s", topic, client->client_id);
            clear_retained_message(client->client_id, topic);
        } else {
            LOG_INF("Storing retained message for topic: %s, client id: %s", topic, client->client_id);
            store_retained_message(client->client_id, topic, payload, payload_len, qos);
        }
    }
    else {
    	LOG_INF("Clearing retained message for topic: %s, client id: %s", topic, client->client_id);
        clear_retained_message(client->client_id, topic);
    }
    
    add_publisher(client->client_id);
	track_republish_frequency(client->client_id, check_topic_wildcard(topic));
	
    if (qos == 0) {
        deliver_message(topic, payload, qos, dup);
    } else if (qos == 1) {
        deliver_message(topic, payload, qos, dup);
        send_puback(ctx, packet_id);
    } else if (qos == 2) {
    	// store pending msg
        temp_packet_id = packet_id;
		temp_qos = qos;

		// Correctly copy topic and payload with length checking
		if (topic_length < MAX_TOPIC_LEN) {
			memcpy(temp_topic, topic, topic_length);
			temp_topic[topic_length] = '\0';  // Null-terminate properly
		} else {
			LOG_ERR("Topic length exceeds limit, truncating");
			memcpy(temp_topic, topic, MAX_TOPIC_LEN - 1);
			temp_topic[MAX_TOPIC_LEN - 1] = '\0';
		}
		
		if (payload_len < MAX_MESSAGE_LEN) {
			memcpy(temp_payload, payload, payload_len);
			temp_payload[payload_len] = '\0';  // Null-terminate properly
		} else {
			LOG_ERR("Payload length exceeds limit, truncating");
			memcpy(temp_payload, payload, MAX_MESSAGE_LEN - 1);
			temp_payload[MAX_MESSAGE_LEN - 1] = '\0';
		}
		temp_dup = dup;
        send_pubrec(ctx, packet_id); // send pubrec and waiting for pubrel
    }
}

void handle_pubrel(struct net_context *ctx, uint8_t *buffer, size_t length) {
    if (length < 4) {  // PUBREL must have at least 4 bytes
        LOG_ERR("PUBREL packet too short");
        return;
    }

    uint16_t packet_id = (buffer[2] << 8) | buffer[3];  // Extract packet ID
    LOG_INF("Received PUBREL for packet ID: %d", packet_id);

	deliver_message(temp_topic, temp_payload, temp_qos, temp_dup);
    send_pubcomp(ctx, packet_id);
}


