#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_context.h>



bool session_exists(const char *client_id);
void initialize_client_session(int index, struct net_context *ctx, const char *client_id, 
							uint16_t keep_alive, const char *ip_addr, bool clean_session);
int send_connack(struct net_context *ctx, uint8_t return_code);
void send_pingresp(struct net_context *ctx);
void handle_ping(struct net_context *ctx, uint8_t *buffer, ssize_t length, const char *ip_addr);
int handle_connect(struct net_context *ctx, uint8_t *buffer, ssize_t length, const char *ip_addr);
void handle_disconnect(struct net_context *ctx, uint8_t *buffer, ssize_t length);
