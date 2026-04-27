#include <assert.h>
#include <lwip/def.h>
#include <lwip/inet.h>
#include <projdefs.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>

#include "lwipopts.h"
#include <lwip/ip_addr.h>
#include <lwip/udp.h>

#include <queue.h>

#include "chunks.h"
#include "puzzle.h"
#include "video.h"
#include "error.h"
#include "network.h"

#include <xil_printf.h>
#include <xil_cache.h>

#include <xsolver_toplevel.h>

#define THREAD_STACKSIZE 1024

static void accept_input_task(void * data);
static void request_protocol_task(void * data);
static void draw_puzzles_task(void * data);
static void solve_puzzles_task(void * data);

static char buffer[16];

QueueHandle_t requests_queue; // REQUEST_INFO messages for puzzle procurement.
QueueHandle_t graphics_queue; // PUZZLE_INFO/CHUNK_DATA messages for drawing.
QueueHandle_t challenge_queue; // PUZZLE_INFO/CHUNK_DATA messages for solving.

static unsigned int readline(char * const dst, const unsigned int max_length)
{
    if (max_length == 0)
        return 0;

    unsigned int idx = 0;

    while (1) {
        const char ch = inbyte();

        switch (ch) {
        case '\r':
        case '\n':
            print("\r\n");
            dst[idx] = '\0';
            return idx;
        case '\b':
        case 0x7f:
            if (idx > 0) {
                --idx;
                print("\b \b");
            }
            continue;
        default: ;
        }

        if (ch < 0x20)
            continue;

        if (idx + 1 < max_length) {
            dst[idx++] = ch;
            outbyte(ch);
        } else {
            dst[idx] = '\0';
            return idx;
        }
    }
}

static uint32_t pow_uint32(uint32_t base, unsigned int exp)
{
    uint32_t result = 1;

    while (exp--)
        result *= base;

    return result;
}

static uint32_t parse_uint32(const uint32_t lower_bound, const uint32_t upper_bound,
    const uint32_t default_value)
{
    const unsigned int bytes_read = readline(buffer, sizeof(buffer) / sizeof(*buffer));
    uint32_t value = 0;
    
    if (bytes_read > 0 && buffer[bytes_read] == '\0') {
        for (unsigned int pv_idx = 0; pv_idx < bytes_read; ++pv_idx)
            value += (buffer[pv_idx] - '0') * pow_uint32(10, bytes_read - pv_idx - 1);
        if (value < lower_bound || value > upper_bound)
            value = default_value;
    } else
        value = default_value;

    return value;
}

static enum DifficultyTier parse_difficulty_tier(const enum DifficultyTier default_tier)
{
    const unsigned int bytes_read = readline(buffer, sizeof(buffer) / sizeof(*buffer));
    
    if (bytes_read == 1)
        switch (*buffer) {
        case 'E': return DIFFICULTY_EASY;
        case 'M': return DIFFICULTY_MEDIUM;
        case 'H': return DIFFICULTY_HARD;
        case 'C': return DIFFICULTY_CUSTOM;
        default: return default_tier;
        }

    return default_tier;
}

int main()
{
    static unsigned char mac[] = { 0x00, 0x11, 0x22, 0x33, 0x00, 0x19 };

    requests_queue = xQueueCreate(1, sizeof(struct MessageRequestInfo));
    graphics_queue = xQueueCreate(1, sizeof(struct MessagePuzzleInfo));
    challenge_queue = xQueueCreate(1, sizeof(struct MessagePuzzleInfo));
    
    network_init(mac, accept_input_task);

    vTaskStartScheduler();

    return 0;
}

static void accept_input_task(void * const data)
{
    (void) data;

    struct MessageRequestInfo request_info = {
        .metadata = {
            .valid = true
        }
    };

    print("Enter a seed: ([0]-4294967295) ");
    request_info.metadata.seed = parse_uint32(0, 4294967295, 0);

    print("Enter a size index: ([0]-15) ");
    request_info.metadata.difficulty.size_index = parse_uint32(0, 15, 0);
    
    print("Enter a difficulty tier: ([E],M,H,C) ");
    request_info.metadata.difficulty.tier = parse_difficulty_tier(DIFFICULTY_EASY);

    puzzle_request_print(&request_info);
    xQueueSend(requests_queue, &request_info, portMAX_DELAY);

    xTaskCreate(&request_protocol_task, "request_protocol_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO, NULL);
    vTaskSuspend(NULL);
}

static void draw_puzzles_task(void * const data)
{
    (void) data;
    
    static struct VideoState video_state;
    video_initialise(&video_state);

    struct MessagePuzzleInfo puzzle_info;
    
    while (1)
        if (xQueueReceive(graphics_queue, &puzzle_info, portMAX_DELAY) == pdTRUE) {
            print("draw_puzzles_task: drawing to framebuffer...\r\n");
            video_draw_puzzle(&video_state, &puzzle_info);
        }

    vTaskDelete(NULL);
}

static void solve_puzzles_task(void * const data)
{
    (void) data;

    struct MessagePuzzleInfo puzzle_info;
    XSolver_toplevel solver;

    static uint32_t solver_ram = 64;
    XSolver_toplevel_Initialize(&solver, XPAR_XSOLVER_TOPLEVEL_0_BASEADDR);

    while (1)
        if (xQueueReceive(challenge_queue, &puzzle_info, portMAX_DELAY) == pdTRUE) {
            XSolver_toplevel_Set_ram(&solver, (UINTPTR)&solver_ram);
            Xil_DCacheFlushRange((UINTPTR)&solver_ram, sizeof(solver_ram));
            XSolver_toplevel_Start(&solver);
            while (!XSolver_toplevel_IsDone(&solver));

            xil_printf("Received %d from LFSR.\r\n", XSolver_toplevel_Get_return(&solver));
        }

    vTaskDelete(NULL);
}

static int udp_receive_message(const int sock, struct sockaddr_in * const src,
    void * const message, const enum MessageType message_type)
{
    static uint8_t buffer[256];
    static struct MessageError error;

    socklen_t src_len = sizeof(*src);
    const ssize_t data_len = lwip_recvfrom(sock, buffer, sizeof(buffer) / sizeof(*buffer),
        0, (struct sockaddr *) src, &src_len);

    if (data_len >= 0) {
        if (*buffer == MSG_ERROR) {
            if (error_parse(&error, buffer) == 0)
                error_print(&error);

            // Well-defined error handled.
            return -1;
        } else if (message_type == *buffer) {
            switch (*buffer) {
            case MSG_PUZZLE_INFO: return puzzle_parse(message, buffer);
            case MSG_CHUNK_DATA: return chunk_parse(message, buffer);
            default:
                // Desirable message does not have a parser.
                return -1;
            }
        }

        // Received a non-error message, but wasn't of the desired type.
        return -1;
    }

    // Transmission error.
    return -1;
}

static void build_puzzle_data(
    const int sock,
    const struct sockaddr_in * const dst_addr,
    const struct MessageRequestInfo * const request_info
)
{
    static struct MessagePuzzleInfo puzzle_info;
    static struct MessageRequestChunk request_chunk;
    static struct sockaddr_in src_addr;
    
    struct MessageChunkData * const chunk_data = &puzzle_info.chunk;
    
    // Request a puzzle according to the given request_info.
    puzzle_request(request_info, sock, dst_addr);
    if (udp_receive_message(sock, &src_addr, &puzzle_info, MSG_PUZZLE_INFO) != 0) {
        print("request_task: did not receive expected MSG_PUZZLE_INFO.\r\n");
        return;
    }
    
    // For info, describe the puzzle on the serial output.
    puzzle_print(&puzzle_info);
    request_chunk.metadata = puzzle_info.metadata;

    assert(puzzle_info.num_chunks == 1); // TODO remove constraint

    // Request, receive, and parse each chunk of clue data.
    for (uint8_t chunk_id = 0; chunk_id < puzzle_info.num_chunks; ++chunk_id) {
        request_chunk.chunk_id = chunk_id;
        chunk_request(&request_chunk, sock, dst_addr);
        if (udp_receive_message(sock, &src_addr, chunk_data, MSG_CHUNK_DATA) == 0) {
            assert(chunk_data->chunk_id == chunk_id);
            assert(metadata_equal(&chunk_data->metadata, &request_chunk.metadata));
            chunk_print(chunk_data);
            
            if (chunk_data->max_clue_data_count > puzzle_info.global_max_clue_data_count)
                puzzle_info.global_max_clue_data_count = chunk_data->max_clue_data_count;
        }
    }

    // Broadcast out the (small) puzzle specification to the graphics handler and solver.
    xQueueSend(graphics_queue, &puzzle_info, portMAX_DELAY);
    xQueueSend(challenge_queue, &puzzle_info, portMAX_DELAY); 
}

static void request_protocol_task(void * const data)
{
    (void) data;

    struct sockaddr_in local_addr;
    struct sockaddr_in dst_addr;
    const int sock = network_bind_socket(&local_addr);

    if (sock < 0) {
        vTaskDelete(NULL);
        return;
    }

    network_prepare_dst_addr(&dst_addr);
    xTaskCreate(&draw_puzzles_task, "draw_puzzles_task", THREAD_STACKSIZE, NULL, 1, NULL);
    xTaskCreate(&solve_puzzles_task, "solve_puzzles_task", THREAD_STACKSIZE, NULL, 1, NULL);

    struct MessageRequestInfo request_info;

    while (1)
        if (xQueueReceive(requests_queue, &request_info, portMAX_DELAY) == pdTRUE)
            /*
             * On receipt of a request, query the server to build a puzzle of the
             * specified parameters.
             */
            build_puzzle_data(sock, &dst_addr, &request_info);
    
    lwip_close(sock);
    vTaskDelete(NULL);
}
