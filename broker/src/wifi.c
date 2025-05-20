#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/dhcpv4_server.h>
#include "wifi.h"
#include "broker.h"


LOG_MODULE_REGISTER(WIFI);

static struct net_if *ap_iface;
static struct net_if *sta_iface;

static struct wifi_connect_req_params ap_config;
static struct wifi_connect_req_params sta_config;
static struct net_mgmt_event_callback cb;

void wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
			       struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT: {
		LOG_INF("Connected to %s", WIFI_SSID);
		set_static_ip(); // Set static IP here
		mqtt_broker_listen();
		break;
	}
	case NET_EVENT_WIFI_DISCONNECT_RESULT: {
		LOG_INF("Disconnected from %s", WIFI_SSID);
		break;
	}
	case NET_EVENT_WIFI_AP_ENABLE_RESULT: {
		LOG_INF("AP Mode is enabled. Waiting for station to connect");
		// Start listening for incoming MQTT connections
		mqtt_broker_listen();
		break;
	}
	case NET_EVENT_WIFI_AP_DISABLE_RESULT: {
		LOG_INF("AP Mode is disabled.");
		break;
	}
	case NET_EVENT_WIFI_AP_STA_CONNECTED: {
		struct wifi_ap_sta_info *sta_info = (struct wifi_ap_sta_info *)cb->info;

		LOG_INF("station: " MACSTR " joined ", sta_info->mac[0], sta_info->mac[1],
			sta_info->mac[2], sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		break;
	}
	case NET_EVENT_WIFI_AP_STA_DISCONNECTED: {
		struct wifi_ap_sta_info *sta_info = (struct wifi_ap_sta_info *)cb->info;

		LOG_INF("station: " MACSTR " leave ", sta_info->mac[0], sta_info->mac[1],
			sta_info->mac[2], sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		break;
	}
	default:
		break;
	}
}

void enable_dhcpv4_server(void)
{
	static struct in_addr addr;
	static struct in_addr netmaskAddr;

	if (net_addr_pton(AF_INET, WIFI_AP_IP_ADDRESS, &addr)) {
		LOG_ERR("Invalid address: %s", WIFI_AP_IP_ADDRESS);
		return;
	}

	if (net_addr_pton(AF_INET, WIFI_AP_NETMASK, &netmaskAddr)) {
		LOG_ERR("Invalid netmask: %s", WIFI_AP_NETMASK);
		return;
	}

	net_if_ipv4_set_gw(ap_iface, &addr);

	if (net_if_ipv4_addr_add(ap_iface, &addr, NET_ADDR_MANUAL, 0) == NULL) {
		LOG_ERR("unable to set IP address for AP interface");
	}

	if (!net_if_ipv4_set_netmask_by_addr(ap_iface, &addr, &netmaskAddr)) {
		LOG_ERR("Unable to set netmask for AP interface: %s", WIFI_AP_NETMASK);
	}

	addr.s4_addr[3] += 10; /* Starting IPv4 address for DHCPv4 address pool. */

	if (net_dhcpv4_server_start(ap_iface, &addr) != 0) {
		LOG_ERR("DHCP server is not started for desired IP");
		return;
	}

	LOG_INF("DHCPv4 server started...\n");
}

int enable_ap_mode(void) {
    ap_iface = net_if_get_default();  // Get the default network interface

	if (!ap_iface) {
		LOG_INF("AP: is not initialized");
		return -EIO;
	}

	LOG_INF("Turning on AP Mode");
	ap_config.ssid = (const uint8_t *)WIFI_AP_SSID;
	ap_config.ssid_length = strlen(WIFI_AP_SSID);
	ap_config.psk = (const uint8_t *)WIFI_AP_PSK;
	ap_config.psk_length = strlen(WIFI_AP_PSK);
	ap_config.channel = WIFI_CHANNEL_ANY;
	ap_config.band = WIFI_FREQ_BAND_2_4_GHZ;

	if (strlen(WIFI_AP_PSK) == 0) {
		ap_config.security = WIFI_SECURITY_TYPE_NONE;
	} else {

		ap_config.security = WIFI_SECURITY_TYPE_PSK;
	}

	enable_dhcpv4_server();

	int ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, ap_iface, &ap_config,
			   sizeof(struct wifi_connect_req_params));
	if (ret) {
		LOG_ERR("NET_REQUEST_WIFI_AP_ENABLE failed, err: %d", ret);
	}

	return ret;
}

void set_static_ip(void) {
    struct net_if *iface = net_if_get_default(); // Get the default network interface
    struct in_addr ip, netmask, gateway;

    if (!iface) {
        LOG_ERR("No network interface found");
        return;
    }

    // Disable DHCP
    net_dhcpv4_stop(iface);
    LOG_INF("DHCP disabled");

    // Convert string IPs to binary format
    if (net_addr_pton(AF_INET, "192.168.0.100", &ip) < 0 ||
        net_addr_pton(AF_INET, "255.255.255.0", &netmask) < 0 ||
        net_addr_pton(AF_INET, "192.168.0.1", &gateway) < 0) {
        LOG_ERR("Invalid IP configuration");
        return;
    }

    // Assign static IP with netmask
    if (net_if_ipv4_addr_add(iface, &ip, NET_ADDR_MANUAL, 24) < 0) {
        LOG_ERR("Failed to set static IP");
        return;
    }

 	// Add the gateway (router) with proper parameters
    struct net_if_router *router = net_if_ipv4_router_add(iface, &gateway, true, 0);
    if (!router) {
        LOG_ERR("Failed to set the default gateway");
        return;
    }
    LOG_INF("Static IP set: %s", WIFI_STATIC_IP);
}

int connect_to_wifi(void) {
	sta_iface = net_if_get_wifi_sta();
	
    if (!sta_iface) {
        LOG_INF("STA: interface not initialized");
        return -EIO;
    }

    sta_config.ssid = (const uint8_t *)WIFI_SSID;
    sta_config.ssid_length = strlen(WIFI_SSID);
    sta_config.psk = (const uint8_t *)WIFI_PSK;
    sta_config.psk_length = strlen(WIFI_PSK);
    sta_config.security = WIFI_SECURITY_TYPE_PSK;
    sta_config.channel = WIFI_CHANNEL_ANY;
    sta_config.band = WIFI_FREQ_BAND_2_4_GHZ;

    LOG_INF("Connecting to SSID: %s", WIFI_SSID);

    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, sta_iface, &sta_config,
                       sizeof(struct wifi_connect_req_params));
                       
    if (ret) {
        LOG_ERR("Unable to connect to (%s)", WIFI_SSID);
        return ret;
    }

    // Wait a bit for the connection to establish before setting a static IP
    k_sleep(K_SECONDS(3));
    
    return ret;
}

int setup_wifi(void) {
	net_mgmt_init_event_callback(&cb, wifi_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT |
                                 NET_EVENT_WIFI_DISCONNECT_RESULT |
                                 NET_EVENT_WIFI_AP_ENABLE_RESULT |
                                 NET_EVENT_WIFI_AP_DISABLE_RESULT |
                                 NET_EVENT_WIFI_AP_STA_CONNECTED |
                                 NET_EVENT_WIFI_AP_STA_DISCONNECTED);
    net_mgmt_add_event_callback(&cb);

	//enable_ap_mode();
	connect_to_wifi();	
	
	return 0;
}