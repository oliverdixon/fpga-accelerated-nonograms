#include <xparameters.h>
#include <xil_printf.h>

#include <lwip/dhcp.h>
#include <lwip/sockets.h>
#include <netif/xadapter.h>

#include "network.h"

void lwip_init();

#define THREAD_STACKSIZE 1024

//Function prototypes
void print_ip(const char *msg, const ip_addr_t *ip);
void network_thread(void *p);
int network_startup_task();

//Structures and globals
static struct netif server_netif;
struct netif *echo_netif;
unsigned char* mac_addr;
lwip_thread_fn application_task_fn;
TaskHandle_t startuptask, nettask, apptask, rcv_task;

//---------------

//Initialise network task 
//This is called by user code to provide the mac address we should use,
//and the code that we should run once the network is ready.
void network_init(unsigned char* mac_address, lwip_thread_fn app) {
	mac_addr = mac_address;
	application_task_fn = app;
	xTaskCreate((lwip_thread_fn) network_startup_task, "startup_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO, &startuptask);
}

//Created by network_init(). Initialises lwIP, runs DHCP, and once connected, starts the application_task_fn that the user provided
//when they called network_init
int network_startup_task()
{
    lwip_init();

    //Create lwIP's network handling thread (as described by lwIP documentation)
    xTaskCreate(network_thread, "nw_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO, &nettask);

    //This task just waits until we get an IP address via DHCP, then creates our application task
    while (1) {
    	vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS); //wait 500ms
		if (server_netif.ip_addr.addr) { //Do we have an IP address?
			xil_printf("DHCP request success\r\n");
			print_ip("Board IP: ", &server_netif.ip_addr);
			print_ip("Netmask : ", &server_netif.netmask);
			print_ip("Gateway : ", &server_netif.gw);
			xil_printf("\r\n");
			xTaskCreate(application_task_fn, "app_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO, &apptask);
			break;
		}
	}

    //DHCP is connected and we've created the main task so delete ourselves
    vTaskDelete(NULL); //Passing NULL says to delete this task
    return 0;
}

//lwIP's network handling thread (as described by lwIP documentation)
//Started from the network_startup_task
void network_thread(void *p)
{
    struct netif *netif = &server_netif;
    ip_addr_t ipaddr, netmask, gw;
    int mscnt = 0;

	ipaddr.addr = 0;
	gw.addr = 0;
	netmask.addr = 0;

    // Add our network interface to lwIP and set it as default
    if (!xemac_add(netif, &ipaddr, &netmask, &gw, mac_addr, XPAR_XEMACPS_0_BASEADDR)) {
    	xil_printf("Error adding network interface\r\n");
    	return;
    }
    netif_set_default(netif);
    netif_set_up(netif);

    // Start packet receive thread, this is part of lwIP
    xTaskCreate((void(*)(void*))xemacif_input_thread, "xemacif_input_thread", THREAD_STACKSIZE, netif, DEFAULT_THREAD_PRIO, &rcv_task);

    // Start DHCP. This task will now loop forever calling dhcp_fine_tmr and dhcp_coarse_tmr every so often
    xil_printf("\r\nStart DHCP lookup...\r\n");
    dhcp_start(netif);
    while (1) {
		vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
		dhcp_fine_tmr();
		mscnt += DHCP_FINE_TIMER_MSECS;
		if (mscnt >= DHCP_COARSE_TIMER_SECS*1000) {
			dhcp_coarse_tmr();
			mscnt = 0;
		}
	}

    return;
}

void print_ip(const char *msg, const ip_addr_t *ip)
{
	xil_printf(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip), ip4_addr3(ip), ip4_addr4(ip));
}

int network_bind_socket(struct sockaddr_in * const local_addr, const in_port_t local_port)
{
    int sock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        xil_printf("socket failed\r\n");
        vTaskDelete(NULL);
        return -1;
    }

    memset(local_addr, 0, sizeof(struct sockaddr_in));

    local_addr->sin_family = AF_INET;
    local_addr->sin_port = htons(local_port);
    local_addr->sin_addr.s_addr = PP_HTONL(INADDR_ANY);

    if (bind(sock, (struct sockaddr *) local_addr, sizeof(struct sockaddr_in)) < 0) {
        xil_printf("bind failed\r\n");
        lwip_close(sock);
        vTaskDelete(NULL);
        return -1;
    }

    const struct timeval timeout = {
        .tv_sec = 1,
        .tv_usec = 0
    };

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    xil_printf("UDP socket bound to local port %d\r\n", local_port);
    return sock;
}

void network_prepare_dst_addr(struct sockaddr_in * const dst_addr)
{
    memset(dst_addr, 0, sizeof(struct sockaddr_in));

    dst_addr->sin_family = AF_INET;
    dst_addr->sin_port = htons(51050);
    dst_addr->sin_addr.s_addr = inet_addr("192.168.10.1");
}
