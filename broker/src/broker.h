#include <zephyr/net/socket.h>



#define BROKER_PORT 1883
#define BUFFER_SIZE 256  // Define a reasonable buffer size


#define MQTT_PROTOCOL_LEVEL_311 4
#define MQTT_PROTOCOL_LEVEL_5   5
// MQTT packet types
#define MQTT_CONNECT     0x10  // Client request to connect to Server
#define MQTT_CONNACK     0x20  // Connect acknowledgment
#define MQTT_PUBLISH     0x30  // Publish message
#define MQTT_PUBACK      0x40  // Publish acknowledgment (QoS 1)
#define MQTT_PUBREC      0x50  // Publish received (QoS 2 part 1)
#define MQTT_PUBREL      0x60  // Publish release (QoS 2 part 2)
#define MQTT_PUBCOMP     0x70  // Publish complete (QoS 2 part 3)
#define MQTT_SUBSCRIBE   0x80  // Subscribe request
#define MQTT_SUBACK      0x90  // Subscribe acknowledgment
#define MQTT_UNSUBSCRIBE 0xA0  // Unsubscribe request
#define MQTT_UNSUBACK    0xB0  // Unsubscribe acknowledgment
#define MQTT_PINGREQ     0xC0  // Ping request
#define MQTT_PINGRESP    0xD0  // Ping response
#define MQTT_DISCONNECT  0xE0  // Client is disconnecting
#define MQTT_AUTH        0xF0  // Authentication (MQTT v5.0)
#define MQTT_CONNACK_ACCEPTED 0x00  // Connection accepted response
#define MQTT_CONNACK_REFUSED 0x05  // Refused connection


#define STACK_SIZE 1024


// MQTT CONNECT message structure
struct mqtt_connect_packet {
    uint8_t fixed_header;
    uint8_t remaining_length;
    char protocol_name[4]; // "MQTT"
    uint8_t protocol_level;
    uint8_t connect_flags;
    uint16_t keep_alive;
    char client_id[128]; // client ID sent in CONNECT
};

void mqtt_client_handler(struct net_context *client_ctx,
                         struct sockaddr *client_addr,
                         socklen_t client_addr_len,
                         int status,
                         void *user_data);
void mqtt_broker_listen(void);
void start_connection_monitor();


