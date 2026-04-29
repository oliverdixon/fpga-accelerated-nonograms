#include <assert.h>
#include <lwip/dhcp.h>
#include <lwip/init.h>
#include <lwip/sockets.h>
#include <netif/xadapter.h>
#include <xil_printf.h>
#include <xparameters.h>

#include "logging.h"
#include "network.h"

#define THREAD_STACKSIZE 1024

static unsigned char mac_addr[] = {0x00, 0x11, 0x22, 0x33, 0x00, 0x19};

static struct netif server_netif;
struct netif * echo_netif;
lwip_thread_fn application_task_fn;

static void print_ip(
    const char * msg,
    const ip_addr_t * ip
) {
    logging_printf(
        "%s: %d.%d.%d.%d", msg, ip4_addr1(ip), ip4_addr2(ip), ip4_addr3(ip), ip4_addr4(ip)
    );
}

static void persist_dhcp_task(
    void * const data
) {
    (void)data;

    struct netif * netif = &server_netif;
    ip_addr_t ipaddr, netmask, gw;
    int mscnt = 0;

    ipaddr.addr = 0;
    gw.addr = 0;
    netmask.addr = 0;

    // Add our network interface to lwIP and set it as default
    if (!xemac_add(netif, &ipaddr, &netmask, &gw, mac_addr, XPAR_XEMACPS_0_BASEADDR)) {
        logging_puts("Error adding network interface.");
        return;
    }
    netif_set_default(netif);
    netif_set_up(netif);

    // Start packet receive thread, this is part of lwIP
    xTaskCreate(
        (void (*)(void *))xemacif_input_thread, "xemacif_input_thread", THREAD_STACKSIZE, netif,
        DEFAULT_THREAD_PRIO, NULL
    );

    logging_puts("Start DHCP lookup...");
    dhcp_start(netif);
    while (1) {
        vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
        dhcp_fine_tmr();
        mscnt += DHCP_FINE_TIMER_MSECS;
        if (mscnt >= DHCP_COARSE_TIMER_SECS * 1000) {
            dhcp_coarse_tmr();
            mscnt = 0;
        }
    }

    vTaskDelete(NULL);
    return;
}

static void startup_task(
    void * const network_state
) {
    lwip_init();

    struct NetworkState * const network = network_state;
    assert(network != NULL);
    network->sock = network_bind_socket(&network->local_addr, 51050);

    if (network->sock < 0) {
        vTaskDelete(NULL);
        return;
    }

    network_prepare_dst_addr(&network->dst_addr);
    network->mutex = xSemaphoreCreateMutex();

    xTaskCreate(
        persist_dhcp_task, "persist_dhcp_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO, NULL
    );

    while (1) {
        vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
        if (server_netif.ip_addr.addr) {
            print_ip("Board IP", &server_netif.ip_addr);
            print_ip("Netmask", &server_netif.netmask);
            print_ip("Gateway", &server_netif.gw);
            xTaskCreate(
                application_task_fn, "app_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO, NULL
            );
            break;
        }
    }

    // DHCP is connected and we've created the main task.
    vTaskDelete(NULL);
}

void network_initialise(
    struct NetworkState * const network,
    lwip_thread_fn app
) {
    application_task_fn = app;
    xTaskCreate(
        &startup_task, "startup_task", THREAD_STACKSIZE, network, DEFAULT_THREAD_PRIO, NULL
    );
}

int network_bind_socket(
    struct sockaddr_in * const local_addr,
    const in_port_t local_port
) {
    int sock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        logging_puts("socket failed");
        vTaskDelete(NULL);
        return -1;
    }

    memset(local_addr, 0, sizeof(struct sockaddr_in));

    local_addr->sin_family = AF_INET;
    local_addr->sin_port = htons(local_port);
    local_addr->sin_addr.s_addr = PP_HTONL(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)local_addr, sizeof(struct sockaddr_in)) < 0) {
        logging_puts("bind failed");
        lwip_close(sock);
        vTaskDelete(NULL);
        return -1;
    }

    const struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    logging_printf("UDP socket bound to local port %d.", local_port);
    return sock;
}

void network_prepare_dst_addr(
    struct sockaddr_in * const dst_addr
) {
    memset(dst_addr, 0, sizeof(struct sockaddr_in));

    dst_addr->sin_family = AF_INET;
    dst_addr->sin_port = htons(51050);
    dst_addr->sin_addr.s_addr = inet_addr("192.168.10.1");
}
