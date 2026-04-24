#include <assert.h>
#include <lwip/def.h>
#include <lwip/inet.h>
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

#include <queue.h>

#include "chunks.h"
#include "puzzle.h"
#include "video.h"
#include "error.h"

#define THREAD_STACKSIZE 1024

void network_init(unsigned char* mac_address, lwip_thread_fn app);
void application_task(void *);

void udp_recv_task(void * const data);
void request_protocol_task(void * const data);

struct VideoState video_state;

int main(void)
{
    video_initialise(&video_state);
    network_init(request_protocol_task);

    vTaskStartScheduler();

    return 0;
}

static int udp_receive_puzzle_info(int sock, void * buffer, size_t buffer_size,
        struct sockaddr_in * src, struct MessagePuzzleInfo * const message)
{
    socklen_t src_len = sizeof(*src);
    const ssize_t data_len = recvfrom(sock, buffer, buffer_size, 0,
        (struct sockaddr *) src, &src_len);

    if (data_len >= 0 && *(const uint8_t *) buffer == MSG_PUZZLE_INFO)
        return puzzle_parse(message, buffer);

    return -1;
}

// TODO replace with generic function taking msg type (switch to cast) and func ptr.
static int udp_receive_chunk_data(int sock, void * buffer, size_t buffer_size,
        struct sockaddr_in * src, struct MessageChunkData * const message)
{
    socklen_t src_len = sizeof(*src);
    const ssize_t data_len = recvfrom(sock, buffer, buffer_size, 0,
        (struct sockaddr *) src, &src_len);

    if (data_len >= 0 && *(const uint8_t *) buffer == MSG_ERROR) {
        struct MessageError error = error_parse(buffer);
        error_print(&error);
        error_free(&error);
    }

    return -1;
}

static int bind_socket(struct sockaddr_in * const local_addr)
{
    xil_printf("application_task started\r\n");

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        xil_printf("socket failed\r\n");
        vTaskDelete(NULL);
        return -1;
    }

    memset(local_addr, 0, sizeof(struct sockaddr_in));

    local_addr->sin_family = AF_INET;
    local_addr->sin_port = htons(LOCAL_PORT);
    local_addr->sin_addr.s_addr = PP_HTONL(INADDR_ANY);

    if (bind(sock, (struct sockaddr *) local_addr, sizeof(struct sockaddr_in)) < 0) {
        xil_printf("bind failed\r\n");
        closesocket(sock);
        vTaskDelete(NULL);
        return -1;
    }

    const struct timeval timeout = {
        .tv_sec = 1,
        .tv_usec = 0
    };

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    xil_printf("UDP socket bound to local port %d\r\n", LOCAL_PORT);
    return sock;
}

static void prepare_dst_addr(struct sockaddr_in * const dst_addr)
{
    memset(dst_addr, 0, sizeof(struct sockaddr_in));

    dst_addr->sin_family = AF_INET;
    dst_addr->sin_port = htons(51050);
    dst_addr->sin_addr.s_addr = inet_addr("192.168.10.1");
}

void request_protocol_task(void * const data)
{
    (void)data;

    struct sockaddr_in local_addr;
    struct sockaddr_in dst_addr;
    struct sockaddr_in src_addr;

    const int sock = bind_socket(&local_addr);

    if (sock < 0) {
        vTaskDelete(NULL);
        return;
    }

    prepare_dst_addr(&dst_addr);

    char buffer[256];

    struct MessagePuzzleInfo puzzle_info;
    struct MessageChunkData chunk_data;
    struct MessageRequestChunk request_chunk;

    const struct MessageRequestInfo request_info = {
        .metadata = {
            .valid = true,
            .seed = 13,
            .difficulty = {
                .size_index = SIZE_INDEX_5X5,
                .tier = DIFFICULTY_EASY,
            }
        }
    };

    for (;;) {
        puzzle_request(&request_info, sock, &dst_addr);

        if (udp_receive_puzzle_info(sock, buffer, sizeof(buffer) / sizeof(*buffer), &src_addr, &puzzle_info) == 0) {
            puzzle_print(&puzzle_info);
            request_chunk.metadata = puzzle_info.metadata;

            assert(puzzle_info.num_chunks == 1); // TODO remove constraint

            for (unsigned int chunk_id = 0; chunk_id < puzzle_info.num_chunks; ++chunk_id) {
                request_chunk.chunk_id = chunk_id;
                chunk_request(&request_chunk, sock, &dst_addr);
                if (udp_receive_chunk_data(sock, buffer, sizeof(buffer) / sizeof(*buffer), &src_addr, &chunk_data) == 0) {
                    assert(chunk_data.chunk_id == chunk_id);
                    assert(metadata_equal(&chunk_data.metadata, &request_chunk.metadata));
                    chunk_print(&chunk_data);

                    break; // TODO

                    // TODO maximise clue count over chunks
                }
            }
        } else
            xil_printf("request_task: did not receive expected MSG_PUZZLE_INFO.\r\n");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    closesocket(sock);
    vTaskDelete(NULL);
}
