#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_context.h>
#include "broker.h"
#include "message.h"
#include "client.h"
#include "connect.h"
#include "subscribe.h"
#include "features.h"
#include "model/main_functions.h"
#include <stdint.h>


LOG_MODULE_REGISTER(CONNECT);


int send_connack(struct net_context *ctx, uint8_t return_code) {
    uint8_t connack_packet[] = {MQTT_CONNACK, 0x02, 0x00, return_code};
    LOG_INF("MQTT CONNACK sent");
    return net_context_send(ctx, connack_packet, sizeof(connack_packet), NULL, K_NO_WAIT, NULL);
}

void send_pingresp(struct net_context *ctx) {
    uint8_t pingresp_packet[] = {MQTT_PINGRESP, 0x00};
    net_context_send(ctx, pingresp_packet, sizeof(pingresp_packet), NULL, K_NO_WAIT, NULL);
    LOG_INF("MQTT PINGRESP sent");
}

bool session_exists(const char *client_id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(clients[i].client_id, client_id) == 0) {
            return clients[i].connected;  // Return true if client is marked as connected
        }
    }
    return false;
}

void initialize_client_session(int index, struct net_context *ctx, const char *client_id, 
							uint16_t keep_alive, const char *ip_addr, bool clean_session) {
    clients[index].connected = true;
    clients[index].clean_session = clean_session;
    clients[index].keep_alive = keep_alive;
    clients[index].last_seen = k_uptime_get_32();
   	
   	// Handle context assignment properly
    if (clients[index].ctx) {
        net_context_unref(clients[index].ctx);  // Release the old context if it exists
    }
    clients[index].ctx = ctx ? ctx : NULL;

    strncpy(clients[index].client_id, client_id, sizeof(clients[index].client_id) - 1);
    clients[index].client_id[sizeof(clients[index].client_id) - 1] = '\0';

    strncpy(clients[index].ip_addr, ip_addr, sizeof(clients[index].ip_addr) - 1);
    clients[index].ip_addr[sizeof(clients[index].ip_addr) - 1] = '\0';

    LOG_INF("Initialized session for client: %s", client_id);
}

int handle_connect(struct net_context *ctx, uint8_t *buffer, ssize_t length, const char *ip_addr) {
    if (length < 14) {  // Minimum size for a valid CONNECT packet
        LOG_ERR("CONNECT packet too short");
        return send_connack(ctx, MQTT_CONNACK_REFUSED);
    }

    struct mqtt_connect_packet packet = {0};
    packet.fixed_header = buffer[0];

    if (packet.fixed_header != MQTT_CONNECT) {
        LOG_ERR("Not a CONNECT message");
        return send_connack(ctx, MQTT_CONNACK_REFUSED);
    }

    uint16_t protocol_name_length = (buffer[2] << 8) | buffer[3];
    if (protocol_name_length != 4 || length < 10) {
        LOG_ERR("Invalid protocol name length");
        return send_connack(ctx, MQTT_CONNACK_REFUSED);
    }

    memcpy(packet.protocol_name, &buffer[4], 4);
    packet.protocol_name[4] = '\0';

    if (strncmp(packet.protocol_name, "MQTT", 4) != 0) {
        LOG_ERR("Unsupported protocol name: %s", packet.protocol_name);
        return send_connack(ctx, MQTT_CONNACK_REFUSED);
    }

    packet.protocol_level = buffer[8];
    if (packet.protocol_level != MQTT_PROTOCOL_LEVEL_311 && packet.protocol_level != MQTT_PROTOCOL_LEVEL_5) {
        LOG_ERR("Unsupported protocol level: %d", packet.protocol_level);
        return send_connack(ctx, MQTT_CONNACK_REFUSED);
    }

    packet.connect_flags = buffer[9];
    packet.keep_alive = (buffer[10] << 8) | buffer[11];

    bool clean_session = (packet.connect_flags & 0x02) ? true : false; // Bit 1
    LOG_INF("Connect Flags: %02X, Keep Alive: %d, Clean Session: %d", packet.connect_flags, packet.keep_alive, clean_session);

    int index = 12;  // Start parsing from Client ID length

    // --- Parse Client ID ---
    uint16_t client_id_length = (buffer[index] << 8) | buffer[index + 1];
    index += 2;

    char client_id[MAX_CLIENT_ID_LEN + 1] = {0};

    if (client_id_length > 0 && (index + client_id_length) <= length) {
        size_t copy_length = (client_id_length < MAX_CLIENT_ID_LEN) ? client_id_length : MAX_CLIENT_ID_LEN;
        memcpy(client_id, &buffer[index], copy_length);
        client_id[copy_length] = '\0';
        index += client_id_length;
    } else {
        if (clean_session) {
            LOG_WRN("Generating random client ID");
            generate_random_client_id(client_id, MIN_CLIENT_ID_LEN);
        } else {
            LOG_ERR("Client ID missing and clean session is 0");
            return send_connack(ctx, MQTT_CONNACK_REFUSED);
        }
    }

    LOG_INF("Client ID: %s connected", client_id);

    // --- Restore Previous Session If Needed ---
    int client_index = -1;
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].connected && strcmp(clients[i].client_id, client_id) == 0) {
            client_index = i;
            break;
        }
    }
    
    if (client_index != -1 && !clean_session) {		
		if (!session_exists(client_id)) {
			LOG_INF("No existing session found, initializing new session");
			initialize_client_session(client_index, ctx, client_id, packet.keep_alive, ip_addr, clean_session);
		} else {
			LOG_INF("Restoring session for client: %s", client_id);
			
			// Update context properly only if it has changed
			if (clients[client_index].ctx != ctx) {
				if (clients[client_index].ctx) {
					net_context_unref(clients[client_index].ctx);  // Release the old context if it exists
				}
				clients[client_index].ctx = ctx;
				LOG_INF("Updated client context for %s", client_id);
			}
			restore_pending_messages(ctx, client_id);
		}
	} else {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (!clients[i].connected || (clean_session && strcmp(clients[i].client_id, client_id) == 0)) {
				if (clean_session) {
					LOG_INF("Cleaning session for client: %s", clients[i].client_id);
					clear_subscriber(clients[i].client_id);
        			clear_pending_messages(clients[client_index].client_id);

					uint32_t prev_reconnect_count = clients[i].reconnect_count;
					uint32_t prev_reconnect_window_start = clients[i].reconnect_window_start;
					memset(&clients[i], 0, sizeof(clients[i]));  // Clear other fields					
					// preserve reconnect frequency
					clients[i].reconnect_count = prev_reconnect_count;
					clients[i].reconnect_window_start = prev_reconnect_window_start;
				}
				initialize_client_session(i, ctx, client_id, packet.keep_alive, ip_addr, clean_session);
				client_index = i;
				break;
			}
		}
	}

    if (client_index == -1) {
        LOG_ERR("No available session slots");
        return send_connack(ctx, MQTT_CONNACK_REFUSED);
    }

    // --- Handle Will Message ---
    if (packet.connect_flags & 0x04) {  // Will Flag (bit 2)
        uint16_t will_topic_length = (buffer[index] << 8) | buffer[index + 1];
        index += 2;
        char will_topic[MAX_TOPIC_LEN + 1] = {0};
        memcpy(will_topic, &buffer[index], will_topic_length);
        will_topic[will_topic_length] = '\0';
        index += will_topic_length;

        uint16_t will_message_length = (buffer[index] << 8) | buffer[index + 1];
        index += 2;
        char will_message[MAX_MESSAGE_LEN + 1] = {0};
        memcpy(will_message, &buffer[index], will_message_length);
        will_message[will_message_length] = '\0';
        index += will_message_length;

        clients[client_index].has_will = true;
        strncpy(clients[client_index].will_topic, will_topic, sizeof(clients[client_index].will_topic) - 1);
        strncpy(clients[client_index].will_message, will_message, sizeof(clients[client_index].will_message) - 1);
        clients[client_index].will_qos = (packet.connect_flags & 0x18) >> 3;
        clients[client_index].will_retain = (packet.connect_flags & 0x20) >> 5;
		store_will_message(client_id, will_topic, will_message, clients[client_index].will_qos, clients[client_index].will_retain);
    }

    send_connack(ctx, MQTT_CONNACK_ACCEPTED);

    struct connect_features features = get_connect_features(buffer, length, client_index, client_id);
    
	LOG_WRN("CONNECT Features:"
        " Event Type: %d,"
        " Clean Session: %d,"
        " Will Flag: %d,"
        " Will QoS: %d,"
        " Will Retain: %d,"
        " Will Payload Size: %d,"
        " Keep Alive: %d,"
        " Reconnect Freq: %d,"
        " Pending Queue: %d,"
        " Retained Queue: %d,"
        " Subscription Count: %d,"
        " Resub Freq: %d,"
        " Republish Freq: %d\n",
        features.event_type,
        features.clean_session,
        features.will_flag,
        features.will_flag ? features.will_qos : -1,
        features.will_flag ? features.will_retain : -1,
        features.will_flag ? features.will_payload_size : -1,
        features.keep_alive,
        features.reconn_freq,
        features.pending_queue_status,
        features.retained_queue_status,
        features.subscription_count,
        -1,  // Resubscription Frequency
        -1   // Republish Frequency
	);

	
	int8_t input_data[13] = {
		(int8_t)features.event_type,
		(int8_t)features.clean_session,
		(int8_t)features.will_flag,
		features.will_flag ? (int8_t)features.will_qos : -1,
		features.will_flag ? (int8_t)features.will_retain : -1,
		features.will_flag ? (int8_t)features.will_payload_size : -1,
		(int8_t)features.keep_alive,
		(int8_t)features.reconn_freq,
		(int8_t)features.pending_queue_status,
		(int8_t)features.retained_queue_status,
		(int8_t)features.subscription_count,
		-1,  // Resubscription Frequency
		-1   // Republish Frequency
	};

	analyze_message(input_data);


	return 0;
}

void handle_ping(struct net_context *ctx, uint8_t *buffer, ssize_t length, const char *ip_addr) {
    if (ctx == NULL || buffer == NULL || length < 2 || ip_addr == NULL) {
        LOG_ERR("MQTT PINGREQ: Invalid parameters!");
        return;
    }

    // Ensure packet is a valid PINGREQ (0xC0, 0x00)
    if (buffer[0] != 0xC0 || buffer[1] != 0x00) {
        LOG_WRN("MQTT PINGREQ: Invalid packet received!");
        return;
    }

    int client_index = -1;

    // Identify the client based on the IP address
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].ip_addr[0] != '\0' && strcmp(clients[i].ip_addr, ip_addr) == 0) {
            client_index = i;
            break;
        }
    }

    if (client_index == -1) {
        LOG_WRN("MQTT PINGREQ: Client with IP %s not found!", ip_addr);
        return;
    }

    // Update last_seen timestamp
    clients[client_index].last_seen = k_uptime_get_32();
    LOG_INF("MQTT PINGREQ: Client %s updated last_seen to %u", 
            clients[client_index].client_id, clients[client_index].last_seen);
    
    send_pingresp(ctx);
}

void handle_disconnect(struct net_context *ctx, uint8_t *buffer, ssize_t length) {
    if (length < 2) {  // MQTT DISCONNECT packet should be at least 2 bytes (Fixed Header)
        LOG_ERR("DISCONNECT packet too short");
        return;
    }

    struct mqtt_client_info *client = get_client_info(ctx);
    // Find the client session based on the context
    int client_index = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].connected && clients[i].client_id == client->client_id) {
            client_index = i;
            break;
        }
    }

    if (client_index == -1) {
        LOG_WRN("Client not found for DISCONNECT");
        return;
    }

    LOG_INF("Client %s sent DISCONNECT", clients[client_index].client_id);

    // Since the client is disconnecting normally, remove the Will message
    if (clients[client_index].has_will) {
        LOG_INF("Removing Will message for client: %s", clients[client_index].client_id);
        clear_will_message(clients[client_index].client_id);
        clients[client_index].has_will = false;
    }

    // Handle session cleanup if Clean Session was set
    if (clients[client_index].clean_session) {
        LOG_INF("Cleaning session for client: %s", clients[client_index].client_id);
        clear_subscriber(clients[client_index].client_id);
        clear_pending_messages(clients[client_index].client_id);
        
		uint32_t prev_reconnect_count = clients[client_index].reconnect_count;
		uint32_t prev_reconnect_window_start = clients[client_index].reconnect_window_start;
        memset(&clients[client_index], 0, sizeof(clients[client_index]));  // Reset client session
		// preserve reconnect frequency
		clients[client_index].reconnect_count = prev_reconnect_count;
		clients[client_index].reconnect_window_start = prev_reconnect_window_start;   
    }
    
    net_context_put(ctx);
}

