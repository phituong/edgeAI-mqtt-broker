

#define MAX_RECONNECT_FREQ 10 // per 10 seconds
#define MAX_RESUBSCRIPTION_FREQ 10 // per 10 seconds
#define MAX_REPUBLISH_FREQ 10 // per 10 secs
#define LOW_RATIO  0.3f  
#define HIGH_RATIO 0.7f  

struct connect_features {
    uint8_t event_type;              // Always 1
    uint8_t will_qos;                     // Extracted from Will QoS
    bool will_retain;                  // Will Retain flag
    uint8_t will_payload_size;            // Will message size
    bool will_flag;               // Will message exists
    bool will_topic_contain_wildcard; // Will topic name contain wildcard (#,+)
    uint8_t keep_alive;              // Keep-alive interval
    uint8_t reconn_freq;               // Connection/reconnect frequency per second    
    bool clean_session;           // Clean Session flag
    uint8_t pending_queue_status;           // Pending messages count
    uint8_t retained_queue_status;          // Retained messages count
    uint8_t subscription_count;          
};

struct publish_features {
    uint8_t event_type;              // Always 2
    uint8_t qos;                     // Extracted from QoS
    bool retain;                  // Will Retain flag
    bool clean_session;           // Clean Session flag
    uint8_t payload_size;            // Will message size
    bool topic_contain_wildcard; // Will topic name contain wildcard (#,+)
    uint8_t pending_queue_status;           // Pending messages count
    uint8_t retained_queue_status;          // Retained messages count
    uint8_t republish_freq;          // 

};

struct subscribe_features {
    uint8_t event_type;              // Always 3
    uint8_t qos;                     // Extracted from QoS
    bool clean_session;           // Clean Session flag
    bool topic_contain_wildcard; // topic name contain wildcard (#,+)
    uint8_t pending_queue_status;           // Pending messages count
    uint8_t retained_queue_status;          // Retained messages count
    uint8_t resubscription_freq;          //
    uint8_t subscription_count;          //
    uint8_t wildcard_subscription_count;
};



bool check_topic_wildcard(const char *topic);
struct subscribe_features get_subscribe_features(uint8_t *buffer, size_t length, struct mqtt_client_info *client);
struct publish_features get_publish_features(uint8_t *buffer, size_t length, struct mqtt_client_info *client);
struct connect_features get_connect_features(uint8_t *buffer, size_t length, uint8_t client_index, uint8_t client_id);
