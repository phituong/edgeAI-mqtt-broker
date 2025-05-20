#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "broker.h"
#include "wifi.h"


int main(void) {

    setup_wifi();
    start_connection_monitor();  // Start the monitoring thread

    return 0;
}
