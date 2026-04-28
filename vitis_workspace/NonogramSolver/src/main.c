#include <assert.h>

#include <lwip/sockets.h>
#include <xil_printf.h>
#include <xil_cache.h>

#include "chunks.h"
#include "puzzle.h"
#include "video.h"
#include "error.h"
#include "network.h"
#include "serial.h"
#include "solution_driver.h"

#define THREAD_STACKSIZE 1024

static void accept_input_task(void * data);
static void request_protocol_task(void * data);
static void draw_puzzles_task(void * data);
static void solve_puzzles_task(void * data);

QueueHandle_t requests_queue; // REQUEST_INFO messages for puzzle procurement.
QueueHandle_t graphics_queue; // PUZZLE_INFO/CHUNK_DATA messages for drawing.
QueueHandle_t challenge_queue; // PUZZLE_INFO/CHUNK_DATA messages for solving.

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

    xTaskCreate(&request_protocol_task, "request_protocol_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO - 1, NULL);

    print("Enter a seed: ([0]-4294967295) ");
    request_info.metadata.seed = parse_uint32(0, 4294967295, 0);

    print("Enter a size index: ([0]-15) ");
    request_info.metadata.difficulty.size_index = parse_uint32(0, 15, 0);
    
    print("Enter a difficulty tier: ([E],M,H,C) ");
    request_info.metadata.difficulty.tier = parse_difficulty_tier(DIFFICULTY_EASY);

    puzzle_request_print(&request_info);
    xQueueSend(requests_queue, &request_info, portMAX_DELAY);

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

    XSolver_toplevel_Initialize(&solver, XPAR_XSOLVER_TOPLEVEL_0_BASEADDR);

    while (1)
        if (xQueueReceive(challenge_queue, &puzzle_info, portMAX_DELAY) == pdTRUE)
            solver_solve(&solver, &puzzle_info);

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
