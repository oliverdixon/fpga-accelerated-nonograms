#include <lwip/def.h>
#include <stdio.h>
#include <string.h>

#include "metadata.h"
#include "xparameters.h"
#include "netif/xadapter.h"
#include "xuartps_hw.h"
#include "xil_printf.h"
#include "FreeRTOS.h"
#include "task.h"

#include "lwip/sockets.h"
#include "lwipopts.h"
#include <lwip/ip_addr.h>
#include <lwip/tcp.h>
#include <lwip/udp.h>

#include "chunks.h"
#include "puzzle.h"
#include "video.h"

#define THREAD_STACKSIZE 1024
#define LOCAL_PORT 50000

unsigned char mac_ethernet_address[] = { 0x00, 0x11, 0x22, 0x33, 0x00, 0x19 };

void network_init(unsigned char* mac_address, lwip_thread_fn app);
void application_task(void *);
void udp_get_handler(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                     const ip_addr_t *addr, u16_t port);

ip_addr_t dest_ip;
const u16_t dest_port = 51050;

static struct udp_pcb *udp_pcb_global = NULL;

int main(void)
{
    IP4_ADDR(&dest_ip, 192, 168, 10, 1);
    video_test();
    network_init(mac_ethernet_address, application_task);
    vTaskStartScheduler();
    return 0;
}

void application_task(void *p)
{
    err_t err;

    xil_printf("application_task started\n\r");

    udp_pcb_global = udp_new();
    if (!udp_pcb_global) {
        xil_printf("udp_new failed\r\n");
        vTaskDelete(NULL);
        return;
    }

    err = udp_bind(udp_pcb_global, IP_ADDR_ANY, LOCAL_PORT);
    if (err != ERR_OK) {
        xil_printf("udp_bind failed: %d\r\n", err);
        udp_remove(udp_pcb_global);
        udp_pcb_global = NULL;
        vTaskDelete(NULL);
        return;
    }

    udp_recv(udp_pcb_global, udp_get_handler, NULL);

	const struct MessageRequestInfo request_info = {
		.metadata = {
			.valid = true,
			.seed = 13,
			.difficulty = {
				.size_index = SIZE_INDEX_5X5,
				.tier = DIFFICULTY_EASY
			}
		}
	};

    const struct MessageRequestChunk request_chunk = {
        .chunk_id = 0,
        .metadata = request_info.metadata
    };

    for (;;) {
		vTaskDelay(pdMS_TO_TICKS(1000));
        xil_printf("Sending data...\r\n");
		// puzzle_request(&request_info, udp_pcb_global, &dest_ip, 51050);
        chunk_request(&request_chunk, udp_pcb_global, &dest_ip, 51050);
	}
}

void udp_get_handler(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                     const ip_addr_t *addr, u16_t port)
{
    if (p) {
        char msg[256];
        u16_t copy_len = (p->len < sizeof(msg) - 1) ? p->len : sizeof(msg) - 1;

        memcpy(msg, p->payload, copy_len);
        const uint8_t magic_number = *(const uint8_t *)p->payload;

        switch (magic_number) {
        case MSG_PUZZLE_INFO:
            const struct MessagePuzzleInfo puzzle_info = puzzle_parse(p->payload);
            puzzle_print(&puzzle_info);
            break;
        case MSG_CHUNK_DATA:
            const struct MessageChunkData chunk_data = chunk_parse(p->payload);
            chunk_print(&chunk_data);
            break;
        default:
            xil_printf("udp_get_handler: received message of unknown type %02x.\r\n", magic_number);
        }

        pbuf_free(p);
    }
}
