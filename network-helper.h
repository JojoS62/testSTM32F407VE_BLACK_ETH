#ifndef _MBED_HTTP_EXAMPLE_H_
#define _MBED_HTTP_EXAMPLE_H_

#include "mbed.h"
#include "NetworkInterface.h"

/**
 * Connect to the network using the default networking interface,
 * you can also swap this out with a driver for a different networking interface
 * if you use WiFi: see mbed_app.json for the credentials
 */
NetworkInterface *connect_to_default_network_interface() {
    printf("[NWKH] Connecting to network...\n");

    NetworkInterface* network = NetworkInterface::get_default_instance();

    if (!network) {
        printf("[NWKH] No network interface found, select an interface in 'network-helper.h'\n");
        return NULL;
    }

    nsapi_error_t connect_status = network->connect();

    if (connect_status != NSAPI_ERROR_OK) {
        printf("[NWKH] Failed to connect to network (%d)\n", connect_status);
        return NULL;
    }

    printf("[NWKH] Connected to the network\n");
    printf("[NWKH] IP address: %s\n", network->get_ip_address());
    return network;
}

#endif // _MBED_HTTP_EXAMPLE_H_
