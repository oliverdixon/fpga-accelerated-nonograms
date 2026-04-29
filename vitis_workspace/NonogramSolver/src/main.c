#include <assert.h>

#include <lwip/sockets.h>
#include <xil_cache.h>
#include <xil_printf.h>

#include "chunks.h"
#include "error.h"
#include "logging.h"
#include "network.h"
#include "puzzle.h"
#include "result.h"
#include "serial.h"
#include "solver.h"
#include "video.h"

#define THREAD_STACKSIZE 1024

static void accept_input_task(void *);
static void request_protocol_task(void *);
static void draw_puzzles_task(void *);
static void solve_puzzles_task(void *);
static void submit_protocol_task(void *);

QueueHandle_t requests_queue;  // Metadata for puzzle procurement.
QueueHandle_t graphics_queue;  // PUZZLE_INFO messages for drawing.
QueueHandle_t challenge_queue; // PUZZLE_INFO messages for solving.
QueueHandle_t solution_queue;  // PUZZLE_INFO messages for verifying.

struct NetworkState
{
    int sock;
    struct sockaddr_in local_addr;
    struct sockaddr_in dst_addr;
    SemaphoreHandle_t mutex;
};

// TODO move to network.c and have a proper interface.
void network_initialise(
    struct NetworkState * const network
) {
    network->sock = network_bind_socket(&network->local_addr, 51050);

    if (network->sock < 0) {
        vTaskDelete(NULL);
        return;
    }

    network_prepare_dst_addr(&network->dst_addr);
    network->mutex = xSemaphoreCreateMutex();
}

struct NetworkState network_state;

int main() {
    static unsigned char mac[] = {0x00, 0x11, 0x22, 0x33, 0x00, 0x19};

    requests_queue = xQueueCreate(1, sizeof(struct Metadata));
    graphics_queue = xQueueCreate(1, sizeof(struct Puzzle));
    challenge_queue = xQueueCreate(1, sizeof(struct Puzzle));
    solution_queue = xQueueCreate(1, sizeof(struct Puzzle));

    logging_initialise();

    network_init(mac, accept_input_task);

    vTaskStartScheduler();

    return 0;
}

static void accept_input_task(
    void * const data
) {
    (void)data;

    struct Metadata request_metadata = {.valid = true};

    // TODO while (1)

    network_initialise(&network_state); // TODO
    xTaskCreate(
        &request_protocol_task, "request_protocol_task", THREAD_STACKSIZE, NULL,
        DEFAULT_THREAD_PRIO - 1, NULL
    );

    print("Enter a seed: ([0]-4294967295) ");
    request_metadata.seed = parse_uint32(0, 4294967295, 0);

    print("Enter a size index: ([0]-15) ");
    request_metadata.difficulty.size_index = parse_uint32(0, 15, 0);

    print("Enter a difficulty tier: ([E],M,H,C) ");
    request_metadata.difficulty.tier = parse_difficulty_tier(DIFFICULTY_EASY);

    xQueueSend(requests_queue, &request_metadata, portMAX_DELAY);

    vTaskSuspend(NULL);
}

static void draw_puzzles_task(
    void * const data
) {
    (void)data;

    static struct VideoState video_state;
    if (video_initialise(&video_state) != 0) {
        logging_puts("Video could not be initialised.\r\n");
        vTaskDelete(NULL);
        return;
    }

    struct Puzzle puzzle_info;

    while (1)
        if (xQueueReceive(graphics_queue, &puzzle_info, portMAX_DELAY) == pdTRUE)
            video_draw_puzzle(&video_state, &puzzle_info);

    vTaskDelete(NULL);
}

static void solve_puzzles_task(
    void * const data
) {
    (void)data;

    struct Puzzle puzzle_info;
    XSolver_toplevel solver;

    XSolver_toplevel_Initialize(&solver, XPAR_XSOLVER_TOPLEVEL_0_BASEADDR);

    while (1)
        if (xQueueReceive(challenge_queue, &puzzle_info, portMAX_DELAY) == pdTRUE) {
            solver_solve(&solver, &puzzle_info);
            if (puzzle_info.is_solved) {
                xQueueSend(graphics_queue, &puzzle_info, portMAX_DELAY);
                xQueueSend(solution_queue, &puzzle_info, portMAX_DELAY);
            }
        }

    vTaskDelete(NULL);
}

static int udp_receive_message(
    const int sock,
    struct sockaddr_in * const src,
    void * const dst,
    const enum MessageType message_type,
    const struct Metadata * const match_metadata
) {
    static uint8_t buffer[256]; // TODO big enough?
    static struct ServerError error;

    socklen_t src_len = sizeof(*src);
    const ssize_t data_len = lwip_recvfrom(
        sock, buffer, sizeof(buffer) / sizeof(*buffer), 0, (struct sockaddr *)src, &src_len
    );

    if (data_len >= 0) {
        if (*buffer == MSG_ERROR) {
            if (error_parse(&error, buffer) == 0)
                error_print(&error);

            // Well-defined error handled.
            return -1;
        } else if (message_type == *buffer) {
            switch (*buffer) {
            case MSG_PUZZLE_INFO:
                return puzzle_parse(dst, buffer);
            case MSG_CHUNK_DATA:
                return chunk_parse(dst, match_metadata, buffer);
            case MSG_RESULT:
                return result_parse(dst, match_metadata, buffer);
            default:
                logging_printf("Received message of type %d does not have a parser.\r\n", *buffer);
                return -1;
            }
        }

        logging_printf("Received message of type %d, but expected %d.\r\n", *buffer, message_type);
        return -1;
    }

    logging_puts("Transmission error when receiving UDP packet.\r\n");
    return -1;
}

static void build_puzzle_data(
    const int sock,
    const struct sockaddr_in * const dst_addr,
    const struct Metadata * const metadata
) {
    static struct Puzzle puzzle_info;
    static struct sockaddr_in src_addr;

    struct Chunk * const chunk_data = &puzzle_info.chunk;

    // Request a puzzle according to the given request_info.
    puzzle_request(metadata, sock, dst_addr);
    if (udp_receive_message(sock, &src_addr, &puzzle_info, MSG_PUZZLE_INFO, NULL) != 0)
        return;

    // For info, describe the puzzle on the serial output.
    puzzle_print(&puzzle_info);

    assert(puzzle_info.num_chunks == 1); // TODO remove constraint

    // Request, receive, and parse each chunk of clue data.
    for (uint8_t chunk_id = 0; chunk_id < puzzle_info.num_chunks; ++chunk_id) {
        chunk_request(chunk_id, sock, dst_addr, &puzzle_info.metadata);
        if (udp_receive_message(
                sock, &src_addr, chunk_data, MSG_CHUNK_DATA, &puzzle_info.metadata
            ) == 0) {
            if (chunk_data->chunk_id == chunk_id) {
                chunk_print(chunk_data);
                if (chunk_data->max_clue_data_count > puzzle_info.global_max_clue_data_count)
                    puzzle_info.global_max_clue_data_count = chunk_data->max_clue_data_count;
            } else {
                logging_printf(
                    "Unexpected chunk: requested %d, but received %d.\r\n", chunk_id,
                    chunk_data->chunk_id
                );
                return;
            }
        } else
            return;
    }

    // Broadcast out the (small) puzzle specification to the graphics handler and solver.
    xQueueSend(graphics_queue, &puzzle_info, portMAX_DELAY);
    xQueueSend(challenge_queue, &puzzle_info, portMAX_DELAY);
}

static void request_protocol_task(
    void * const data
) {
    (void)data;

    xTaskCreate(
        &draw_puzzles_task, "draw_puzzles_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO - 1,
        NULL
    );

    xTaskCreate(
        &solve_puzzles_task, "solve_puzzles_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO - 1,
        NULL
    );

    xTaskCreate(
        &submit_protocol_task, "submit_protocol_task", THREAD_STACKSIZE, NULL,
        DEFAULT_THREAD_PRIO - 1, NULL
    );

    struct Metadata request_metadata;

    while (1)
        if (xQueueReceive(requests_queue, &request_metadata, portMAX_DELAY) == pdTRUE) {
            /*
             * On receipt of a request, query the server to build a puzzle of the
             * specified parameters.
             */
            xSemaphoreTake(network_state.mutex, portMAX_DELAY);
            build_puzzle_data(network_state.sock, &network_state.dst_addr, &request_metadata);
            xSemaphoreGive(network_state.mutex);
        }

    vTaskDelete(NULL);
}

static void submit_protocol_task(
    void * const data
) {
    (void)data;

    static struct sockaddr_in src_addr;
    struct Puzzle puzzle_info;
    struct MessageResult result = {.status = RESULT_ERROR};

    while (1)
        if (xQueueReceive(solution_queue, &puzzle_info, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(network_state.mutex, portMAX_DELAY);
            if (result_send(&puzzle_info, network_state.sock, &network_state.dst_addr) == 0)
                udp_receive_message(
                    network_state.sock, &src_addr, &result, MSG_RESULT, &puzzle_info.metadata
                );

            xSemaphoreGive(network_state.mutex);
            result_print(&result);
            result.status = RESULT_ERROR;
        }

    vTaskDelete(NULL);
}
