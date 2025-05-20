#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_pkt.h>
#include "broker.h"
#include "connect.h"
#include "publish.h"
#include "subscribe.h"
#include "client.h"


LOG_MODULE_REGISTER(BROKER);
struct k_thread conn_monitor_thread1, conn_monitor_thread2;
K_THREAD_STACK_DEFINE(conn_monitor_stack1, STACK_SIZE);
K_THREAD_STACK_DEFINE(conn_monitor_stack2, STACK_SIZE);


void mqtt_recv_callback(struct net_context *context, 
                        struct net_pkt *pkt, 
                        union net_ip_header *ip_hdr, 
                        union net_proto_header *proto_hdr, 
                        int status, 
                        void *user_data) {
                        
	uint32_t start_cycles = k_cycle_get_32();
                            
    if (!pkt || status < 0) {
        LOG_ERR("Connection closed or error: %d", status);
        
        if (context) {
            LOG_INF("Closing TCP connection");
            net_context_put(context);  // Close the TCP connection
        }
        return;
    }

    size_t len = net_pkt_remaining_data(pkt);
    if (len == 0) {
        LOG_ERR("Received empty packet");
        net_pkt_unref(pkt);
        return;
    }

    uint8_t buffer[BUFFER_SIZE];
    if (len > BUFFER_SIZE) {
        LOG_WRN("Packet too large, truncating to %d bytes", BUFFER_SIZE);
        len = BUFFER_SIZE;
    }

    if (net_pkt_read(pkt, buffer, len) < 0) {
        LOG_ERR("Failed to read packet data");
        net_pkt_unref(pkt);
        return;
    }

	// Extract client IP address
    struct sockaddr_in *client_addr = (struct sockaddr_in *)&context->remote;
    char client_ip[NET_IPV4_ADDR_LEN];

    if (client_addr->sin_family != AF_INET) {
        LOG_ERR("Not an IPv4 address!");
        return;
    }

    if (!net_addr_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip))) {
        LOG_ERR("Failed to extract client IP!");
        return;
    }
    
    
	switch (buffer[0] & 0xF0) { // extract 4-bit control
        case MQTT_CONNECT:
            LOG_INF("MQTT CONNECT received");
            handle_connect(context, buffer, len, client_ip);
            break;

        case MQTT_PUBLISH:
            LOG_INF("MQTT PUBLISH received");
            handle_publish(context, buffer, len);
            break;

        case MQTT_SUBSCRIBE:
            LOG_INF("MQTT SUBSCRIBE received");
            handle_subscribe(context, buffer, len);
            break;

        case MQTT_UNSUBSCRIBE:
            LOG_INF("MQTT MQTT_UNSUBSCRIBE received");
            handle_unsubscribe(context, buffer, len);
            break;
            
        case MQTT_PINGREQ:
            LOG_INF("MQTT PINGREQ received");
            handle_ping(context, buffer, len, client_ip);
            break;

        case MQTT_PUBREC:
            LOG_INF("MQTT MQTT_PUBREC received");
            handle_pubrec(context, buffer, len);
            break;

        case MQTT_PUBACK:        
        	LOG_INF("MQTT MQTT_PUBBACK received");
            handle_puback(context, buffer, len);
            break;
            
        case MQTT_PUBREL:
            LOG_INF("MQTT MQTT_PUBREL received");
            handle_pubrel(context, buffer, len);
            break;
        
        case MQTT_PUBCOMP:
            LOG_INF("MQTT MQTT_PUBCOMP received");
            handle_pubcomp(context, buffer, len);
            break;
            
        case MQTT_DISCONNECT:
            LOG_INF("MQTT MQTT_DISCONNECT received");
            handle_disconnect(context, buffer, len);
            break;
        
        default:
            LOG_WRN("Unexpected MQTT message received: 0x%02X", buffer[0]);
            break;
    }

	// Get CPU cycle count at end
    uint32_t end_cycles = k_cycle_get_32();
    // Convert cycles to time in microseconds (µs)
    float elapsed_us = (float)k_cyc_to_us_floor32(end_cycles - start_cycles);
    // Convert to milliseconds (ms) for readability
    float elapsed_ms = elapsed_us / 1000.0f;
    
    //printf("Total Processing Time Per Msg: %.2f ms (%.0f µs)\r\n", elapsed_ms, elapsed_us);
    printf("Total Processing Time Per Msg: %.2f ms (%.0f µs)\r\n", (double)elapsed_ms, (double)elapsed_us);


    net_pkt_unref(pkt);
}

void mqtt_client_handler(struct net_context *client_ctx,
                         struct sockaddr *client_addr,
                         socklen_t client_addr_len,
                         int status,
                         void *user_data) {
    if (status < 0) {
        LOG_ERR("Connection failed: %d\n", status);
        return;
    }
    
    struct sockaddr_in *addr = (struct sockaddr_in *)client_addr;
	LOG_INF("Client connected from %s:%d\n", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));

    int ret = net_context_recv(client_ctx, mqtt_recv_callback, K_NO_WAIT, NULL);
    
    if (ret < 0) {
        LOG_ERR("Failed to register receive callback: %d\n", ret);
    } else {
        LOG_INF("Waiting for MQTT messages...\n");
    }
}

void mqtt_broker_listen(void) {
    int ret;
   	struct sockaddr_in broker_addr;
   	struct net_context *mqtt_ctx; // Ensure this is declared

	memset(&broker_addr, 0, sizeof(broker_addr));

	broker_addr.sin_family = AF_INET;
	broker_addr.sin_port = htons(BROKER_PORT);
	broker_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on any available IP

    // Create a TCP socket
    ret = net_context_get(AF_INET, SOCK_STREAM, IPPROTO_TCP, &mqtt_ctx);
    if (ret < 0) {
        LOG_ERR("Failed to create network context: %d\n", ret);
        return;
    }

    // Bind the socket to the address
    ret = net_context_bind(mqtt_ctx, (struct sockaddr *)&broker_addr, sizeof(broker_addr));
    if (ret < 0) {
        LOG_ERR("Failed to bind socket: %d\n", ret);
        return;
    }

    // Start listening for incoming connections
    ret = net_context_listen(mqtt_ctx, 5);
    if (ret < 0) {
        LOG_ERR("Failed to listen on socket: %d\n", ret);
        return;
    }

    LOG_INF("MQTT Broker listening on port %d (all interfaces)\n", BROKER_PORT);

    // Set up an asynchronous accept callback
    ret = net_context_accept(mqtt_ctx, mqtt_client_handler, K_FOREVER, NULL);
    if (ret < 0) {
        LOG_ERR("Failed to accept connections: %d\n", ret);
    }

}

void start_connection_monitor() {
    LOG_INF("Starting connection monitor thread...");
    
    k_thread_create(
        &conn_monitor_thread1,
        conn_monitor_stack1,
        K_THREAD_STACK_SIZEOF(conn_monitor_stack1),
        check_client_connections,
        NULL, NULL, NULL,
        7, 0, K_NO_WAIT
    );
    
}

