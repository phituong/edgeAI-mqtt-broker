#include "message.h"
#include "client.h"
#include <stdint.h>
#include <zephyr/types.h>  // Zephyr-specific types
#include "features.h"
#include "publish.h"
#include "subscribe.h"

static uint32_t max_pending_queue = MAX_PENDING_MESSAGES;
static uint32_t max_retained_queue = MAX_RETAINED_MESSAGES;
static uint32_t max_payload_size = MAX_PACKET_SIZE;
static uint32_t max_subscription_queue = MAX_PERSISTENT_SUBSCRIPTIONS;
static uint32_t max_reconnect_freq = MAX_RECONNECT_FREQ;
static uint32_t max_resubscription_freq = MAX_RESUBSCRIPTION_FREQ;
static uint32_t max_republish_freq = MAX_REPUBLISH_FREQ;


uint8_t categorize_ratio(uint32_t value, uint32_t *max_value) {
    float ratio = (float)value / *max_value;
    
    return (ratio <= LOW_RATIO) ? 0 : (ratio <= HIGH_RATIO) ? 1 : 2;
}

bool check_topic_wildcard(const char *topic) {
    return (topic && (strchr(topic, '#') || strchr(topic, '+')));
}

struct connect_features get_connect_features(uint8_t *buffer, size_t length, uint8_t client_index, uint8_t client_id) {
    struct connect_features features = {0};

    if (length < 14) {
        return features; // Invalid packet
    }

    features.event_type = 1;
    features.keep_alive = (buffer[10] << 8) | buffer[11];
    
    uint8_t reconnect_freq = track_reconnect(client_index);
    features.reconn_freq = categorize_ratio(reconnect_freq, &max_reconnect_freq);;
    features.clean_session = (buffer[9] & 0x02) ? true : false;
    features.pending_queue_status = categorize_ratio(get_pending_queue_status(client_id), &max_pending_queue);

    features.keep_alive = (features.keep_alive <= 30) ? 0 :
                          (features.keep_alive <= 90) ? 1 : 2;

	uint8_t subscriber_index = get_subscriber_index(client_id);
    features.subscription_count = categorize_ratio(get_subscription_count(subscriber_index), &max_subscription_queue);


    if (buffer[9] & 0x04) {  // Will Flag
        features.will_flag = true;
        features.will_qos = (buffer[9] & 0x18) >> 3;
        features.will_retain = (buffer[9] & 0x20) >> 5;

        int index = 12;
        uint16_t client_id_length = (buffer[index] << 8) | buffer[index + 1];
        index += 2 + client_id_length;

        uint16_t will_topic_length = (buffer[index] << 8) | buffer[index + 1];
        index += 2;
        char will_topic[MAX_TOPIC_LEN + 1] = {0};
        memcpy(will_topic, &buffer[index], will_topic_length);
        will_topic[will_topic_length] = '\0';

        if (will_topic_length > 0) {
            features.retained_queue_status = categorize_ratio(
                get_retained_queue_status(will_topic), &max_retained_queue);
        }

        features.will_topic_contain_wildcard = check_topic_wildcard(will_topic);
        index += will_topic_length;

        uint16_t payload_size = (buffer[index] << 8) | buffer[index + 1];
        features.will_payload_size = categorize_ratio(payload_size, &max_payload_size);
    }

    return features;
}

struct publish_features get_publish_features(uint8_t *buffer, size_t length, struct mqtt_client_info *client) {
    struct publish_features features = {0};

    if (length < 2) {
        return features;
    }

    features.event_type = 2;
    uint8_t fixed_header = buffer[0];
    features.qos = (fixed_header & 0x06) >> 1;
    features.retain = (fixed_header & 0x01);
    features.clean_session = client->clean_session;
    features.pending_queue_status = categorize_ratio(get_pending_queue_status(client->client_id), &max_pending_queue);

    size_t remaining_length;
    size_t rem_len_bytes = decode_remaining_length(&buffer[1], &remaining_length);

    int index = 1 + rem_len_bytes;
    uint16_t topic_length = (buffer[index] << 8) | buffer[index + 1];
    index += 2;

    if (topic_length > 0 && index + topic_length <= length) {
        char topic[MAX_TOPIC_LEN + 1] = {0};
        memcpy(topic, &buffer[index], topic_length);
        topic[topic_length] = '\0';
        features.topic_contain_wildcard = check_topic_wildcard(topic);
        index += topic_length;

        features.retained_queue_status = categorize_ratio(get_retained_queue_status(topic), &max_retained_queue);
		features.republish_freq = categorize_ratio(get_normal_republish_frequency(client->client_id), &max_republish_freq);
    }

    int payload_len = length - index;
    features.payload_size = categorize_ratio(payload_len, &max_payload_size);

    return features;
}

// --- Extract SUBSCRIBE features ---
struct subscribe_features get_subscribe_features(uint8_t *buffer, size_t length, struct mqtt_client_info *client) {
    struct subscribe_features features = {0};

    if (length < 8 || !client) {
        return features; // Invalid packet or client missing
    }

    features.event_type = 3;
    features.clean_session = client->clean_session;

    uint8_t subscriber_index = get_subscriber_index(client->client_id);
    features.pending_queue_status = categorize_ratio(get_pending_queue_status(client->client_id), &max_pending_queue);
    features.subscription_count = categorize_ratio(get_subscription_count(subscriber_index), &max_subscription_queue);
    features.wildcard_subscription_count = categorize_ratio(get_wildcard_subscription_count(subscriber_index), &max_subscription_queue);
    features.resubscription_freq = categorize_ratio(get_resubscription_frequency(subscriber_index), &max_resubscription_freq);

    int index = 4; // Start after fixed header and packet ID
    bool wildcard_found = false;
    uint8_t max_qos = 0;

    while (index < length) {
        if (index + 3 > length) {
            break;
        }

        // Read topic length (2 bytes)
        uint16_t topic_length = (buffer[index] << 8) | buffer[index + 1];
        index += 2;

        if (topic_length == 0 || index + topic_length + 1 > length) {
            break;  // Invalid topic length
        }

        char topic[MAX_TOPIC_LEN] = {0};
        memcpy(topic, &buffer[index], topic_length);
        topic[topic_length] = '\0';
        index += topic_length;

        // Categorize retained queue status
        features.retained_queue_status = categorize_ratio(get_retained_queue_status(topic), &max_retained_queue);

        // Read QoS (1 byte)
        uint8_t req_qos = buffer[index++];
        if (req_qos > max_qos) {
            max_qos = req_qos;  // Store highest QoS requested
        }

        if (check_topic_wildcard(topic)) {
            wildcard_found = true;
        }
    }

    features.qos = max_qos;
    features.topic_contain_wildcard = wildcard_found;
    return features;
}

