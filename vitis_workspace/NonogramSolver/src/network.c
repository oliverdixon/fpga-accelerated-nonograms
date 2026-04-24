#include <stdio.h>
#include "xparameters.h"
#include "netif/xadapter.h"
#include "xil_printf.h"

#include "lwip/dhcp.h"
void lwip_init();

#define THREAD_STACKSIZE 1024

static void network_thread(void * data);
int network_startup_task();

//Structures and globals
static struct netif server_netif;
struct netif *echo_netif;
static unsigned char * const mac_addr = { 0x00, 0x11, 0x22, 0x33, 0x00, 0x19 };
lwip_thread_fn application_task_fn;
TaskHandle_t startuptask, nettask, apptask, rcv_task;

void print_ip(const char *msg, const ip_addr_t *ip)
{
	xil_printf(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip), ip4_addr3(ip), ip4_addr4(ip));
}

void network_init(lwip_thread_fn app)
{
	lwip_init();

    xTaskCreate(network_thread, "nw_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO, NULL);

    while (1) {
    	vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS); //wait 500ms
		if (server_netif.ip_addr.addr) { //Do we have an IP address?
			xil_printf("DHCP request success\r\n");
			print_ip("Board IP: ", &server_netif.ip_addr);
			print_ip("Netmask : ", &server_netif.netmask);
			print_ip("Gateway : ", &server_netif.gw);
			xil_printf("\r\n");
			xTaskCreate(app, "app_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO, &apptask);
			break;
		}
	}

	xTaskCreate((lwip_thread_fn) network_startup_task, "startup_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO, NULL);
}

static void network_thread(void * const data)
{
	(void) data;

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
