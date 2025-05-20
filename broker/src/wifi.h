#include <zephyr/net/wifi_mgmt.h>


#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"

#define NET_EVENT_WIFI_MASK                                                                        \
	(NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT |                        \
	 NET_EVENT_WIFI_AP_ENABLE_RESULT | NET_EVENT_WIFI_AP_DISABLE_RESULT |                      \
	 NET_EVENT_WIFI_AP_STA_CONNECTED | NET_EVENT_WIFI_AP_STA_DISCONNECTED)

/* AP Mode Configuration */
#define WIFI_AP_SSID       "HUB_BROKER"
#define WIFI_AP_PSK        ""
#define WIFI_AP_IP_ADDRESS "192.168.4.1"
#define WIFI_AP_NETMASK    "255.255.255.0"

/* STA Mode Configuration */
#define WIFI_SSID "WIFIA"     /* Replace `SSID` with WiFi ssid. */
#define WIFI_PSK  "13149697" /* Replace `PASSWORD` with Router password. */
#define WIFI_STATIC_IP  "192.168.0.100" /* Replace `PASSWORD` with Router password. */
#define WIFI_GATEWAY_IP  "192.168.0.1" /* Replace `PASSWORD` with Router password. */


void set_static_ip(void);
int setup_wifi(void);
int connect_to_wifi(void);
int enable_ap_mode(void);
void enable_dhcpv4_server(void);
void wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface);