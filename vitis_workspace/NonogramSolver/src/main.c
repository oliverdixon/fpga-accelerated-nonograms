/**
 * @file
 * @brief FreeRTOS Nonogram-solver driver
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#include <assert.h>
#include <FreeRTOS.h>

#include "chunks.h"
#include "error.h"
#include "logging.h"
#include "network.h"
#include "puzzle.h"
#include "result.h"
#include "serial.h"
#include "solver.h"
#include "video.h"

#define THREAD_STACKSIZE (1024) /**< @brief Number of words for a standard FreeRTOS task stack. */

/**
 * @brief FreeRTOS task to accept Puzzle specifications on the serial line and enqueue them on the requests queue.
 * @details This task does not participate in ownership semantics of Puzzle objects. It encodes a Metadata object to
 *  partially characterise a Puzzle and enqueues a copy of the (tiny) object onto the requests queue.
 * @param data Unused task payload.
 */
static void accept_input_task(void * data);

/**
 * @brief FreeRTOS task to receive Metadata specification requests from the requests queue, synchronously send and
 *  receive a full Puzzle specification from the server.
 * @details This task dynamically allocates memory for a single-chunk Puzzle specification and populates with clue data
 *  i.a.w. the Metadata request which arrived on the requests queue. The Puzzle is then broadcast on the graphics queue
 *  and challenge queues for visualisation and solving, respectively. Following the multicast, this task relinquishes
 *  ownership of the Puzzle specification.
 * @param data Unused task payload.
 */
static void request_protocol_task(void * data);

/**
 * @brief FreeRTOS task to receive Puzzle specifications on the graphics queue and visualise them on the HDMI.
 * @details This task receives observing references to the Puzzle management structure on the graphics queue and
 *  temporarily raises its own relative priority to render their grid representation (and clue data and filled cells, if
 *  applicable) on the frame buffer.
 * @param data Unused task payload.
 */
static void draw_puzzles_task(void * data);

/**
 * @brief FreeRTOS task to receive solved Puzzle specifications on the challenge queue and attempt to solve them.
 * @details This task receives an owning reference to an unsolved Puzzle specification on the challenge queue and
 *  attempts to solve it. Following that, it sends an observing reference to the graphics queue to visualise the
 *  filled cells, and passes the owning reference to the solution queue to wrap up the process.
 * @param data Unused task payload.
 */
static void solve_puzzles_task(void * data);

/**
 * @brief FreeRTOS task to receive solved Puzzle specifications from the solution queue and wind up the process.
 * @details This task performs the final stages of the solve procedure. In particular, it synchronously sends and
 *  receives the solution to the server for verification and timing, and takes full ownership of the Puzzle management
 *  structure to free any dynamically allocated memory. Finally, it signals to @ref accept_input_task to accept another
 *  Metadata specification.
 * @param data Unused task payload.
 */
static void submit_protocol_task(void * data);

static StackType_t solve_puzzles_stack[16 * THREAD_STACKSIZE]; /**< @brief Monster stack for the DFS solver. */
static StaticTask_t solve_puzzles_pcb; /**< @brief Statically allocated control block for the solver task. */
static uint8_t recv_buffer[1024]; /**< @brief Persistent buffer for receiving UDP/IP messages from the server. */
struct NetworkState network_state; /**< @brief Management block for the LwIP network state. */

QueueHandle_t requests_queue; /**< @brief Metadata PODs for Puzzle procurement. */
QueueHandle_t graphics_queue; /**< @brief Non-owning Puzzle references for visualisation. */
QueueHandle_t challenge_queue; /**< @brief Owning Puzzle references for solving. */
QueueHandle_t solution_queue;  /**< @brief Owning Puzzle references for submitting and cleaning up. */

TaskHandle_t accept_input_task_handle; /**< @brief Metadata query task handle, to be notified when ready to go. */

/**
 * @brief Entry point to initialise global structures and start the FreeRTOS scheduler.
 * @return Zero.
 */
int main()
{
    requests_queue = xQueueCreate(1, sizeof(struct Metadata));
    graphics_queue = xQueueCreate(1, sizeof(struct Puzzle *));
    challenge_queue = xQueueCreate(1, sizeof(struct Puzzle *));
    solution_queue = xQueueCreate(1, sizeof(struct Puzzle *));

    assert(logging_initialise());
    network_initialise(&network_state, accept_input_task);

    vTaskStartScheduler();

    return 0;
}

static void accept_input_task(
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void * const data
) {
    (void)data;

    struct Metadata request_metadata = {.valid = true};

    xTaskCreate(
        &request_protocol_task, "request_protocol_task", THREAD_STACKSIZE, NULL,
        DEFAULT_THREAD_PRIO - 1, NULL
    );

    accept_input_task_handle = xTaskGetCurrentTaskHandle();
    xTaskNotify(accept_input_task_handle, 0, eNoAction);

    // ReSharper disable once CppDFAEndlessLoop
    while (1)
        if (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) == pdPASS) {
            request_metadata.valid = true;

            print("Enter a seed: ([0]-4294967295) ");
            request_metadata.seed = parse_uint32(0, 4294967295, 0);

            print("Enter a size index: ([0]-15) ");
            request_metadata.difficulty.size_index = parse_uint32(0, 15, 0);

            print("Enter a difficulty tier: ([E],M,H,C) ");
            request_metadata.difficulty.tier = parse_difficulty_tier(DIFFICULTY_EASY);

            xQueueSend(requests_queue, &request_metadata, portMAX_DELAY);
        }

    // ReSharper disable once CppDFAUnreachableCode
    vTaskSuspend(NULL);
}

static void draw_puzzles_task(
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void * const data
) {
    (void)data;

    static struct VideoState video_state;
    if (!video_initialise(&video_state)) {
        logging_puts("Video could not be initialised.");
        vTaskDelete(NULL);
        return;
    }

    const struct Puzzle * puzzle_info = NULL;

    // ReSharper disable once CppDFAEndlessLoop
    while (1)
        if (xQueueReceive(graphics_queue, &puzzle_info, portMAX_DELAY) == pdTRUE) {
            // Temporarily make us a higher priority, since we don't want to be suspended half-way through a scanline.
            vTaskPrioritySet(NULL, DEFAULT_THREAD_PRIO + 1);
            video_draw_puzzle(&video_state, puzzle_info);
            vTaskPrioritySet(NULL, DEFAULT_THREAD_PRIO - 1);
        }

    // ReSharper disable once CppDFAUnreachableCode
    vTaskDelete(NULL);
}

static void solve_puzzles_task(
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void * const data
) {
    (void)data;

    assert(solver_initialise_environment());
    struct Puzzle * puzzle_info = NULL;

    // Spawn the submission task to receive the solved Puzzle specification.
    xTaskCreate(&submit_protocol_task, "submit_protocol_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO - 1, NULL);

    // ReSharper disable once CppDFAEndlessLoop
    while (1)
        if (xQueueReceive(challenge_queue, &puzzle_info, portMAX_DELAY) == pdTRUE) {
            if (solver_solve(puzzle_info) == SEARCH_SOLVED)
                xQueueSend(graphics_queue, &puzzle_info, portMAX_DELAY);

            xQueueSend(solution_queue, &puzzle_info, portMAX_DELAY);
        }

    // ReSharper disable once CppDFAUnreachableCode
    vTaskDelete(NULL);
}

/**
 * @brief Receive a message over the UDP/IP interface and run the suitable handler, to be used during a synchronous
 *  protocol run.
 * @param sock The opened socket to the server.
 * @param src The source network address.
 * @param dst The destination network address.
 * @param message_type The sought message type.
 * @param match_metadata Expected metadata described by the response.
 * @return Was a message of the expected type and metadata successfully received and parsed?
 */
static bool udp_receive_message(
    const int sock,
    struct sockaddr_in * const src,
    void * const dst,
    const enum MessageType message_type,
    const struct Metadata * const match_metadata
) {
    static struct ServerError error;

    socklen_t src_len = sizeof(*src);
    const ssize_t data_len = lwip_recvfrom(
        sock, recv_buffer, sizeof(recv_buffer) / sizeof(*recv_buffer), 0, (struct sockaddr *)src, &src_len
    );

    if (data_len >= 0) {
        if (*recv_buffer == MSG_ERROR) {
            if (error_parse(&error, recv_buffer))
                error_print(&error);

            // Well-defined error handled.
            return false;
        }

        if (message_type == *recv_buffer) {
            switch (*recv_buffer) {
            case MSG_PUZZLE_INFO:
                return puzzle_parse(dst, recv_buffer);
            case MSG_CHUNK_DATA:
                return chunk_parse(dst, match_metadata, recv_buffer);
            case MSG_RESULT:
                return result_parse(dst, match_metadata, recv_buffer);
            default:
                logging_printf("Received message of type %d does not have a parser.", *recv_buffer);
                return false;
            }
        }

        logging_printf("Received message of type %d, but expected %d.", *recv_buffer, message_type);
        return false;
    }

    logging_puts("Transmission error when receiving UDP packet.");
    return false;
}

/**
 * @brief Synchronously execute the Puzzle procurement protocol with the Nonogram server.
 * @param sock The opened socket to the server.
 * @param dst_addr The destination address of the server.
 * @param metadata Metadata of the Puzzle to request.
 * @return An owning reference to the dynamically allocated Puzzle management structure.
 * @warning Currently this driver is only capable of dealing with single-chunk puzzles. Anything larger will assert.
 */
static struct Puzzle * build_puzzle_data(
    const int sock,
    const struct sockaddr_in * const dst_addr,
    const struct Metadata * const metadata
) {
    static struct sockaddr_in src_addr;

    struct Puzzle * const puzzle_info = calloc(1, sizeof(struct Puzzle));
    if (puzzle_info == NULL)
        return NULL;

    struct Chunk * const chunk_data = &puzzle_info->chunk;

    // Request a puzzle according to the given request_info.
    puzzle_request(metadata, sock, dst_addr);
    if (!udp_receive_message(sock, &src_addr, puzzle_info, MSG_PUZZLE_INFO, NULL)) {
        free(puzzle_info);
        return NULL;
    }

    // For info, describe the puzzle on the serial output.
    puzzle_print(puzzle_info);
    assert(puzzle_info->num_chunks == 1);

    // Request, receive, and parse each chunk of clue data.
    for (uint8_t chunk_id = 0; chunk_id < puzzle_info->num_chunks; ++chunk_id) {
        chunk_request(chunk_id, sock, dst_addr, &puzzle_info->metadata);
        if (udp_receive_message(sock, &src_addr, chunk_data, MSG_CHUNK_DATA, &puzzle_info->metadata)) {
            if (chunk_data->chunk_id == chunk_id) {
                chunk_print(chunk_data);
                if (chunk_data->max_clue_data_count > puzzle_info->global_max_clue_data_count)
                    puzzle_info->global_max_clue_data_count = chunk_data->max_clue_data_count;
            } else {
                logging_printf("Unexpected chunk: requested %d, but received %d.", chunk_id, chunk_data->chunk_id);
                free(puzzle_info);
                return NULL;
            }
        } else {
            free(puzzle_info);
            return NULL;
        }
    }

    return puzzle_info;
}

static void request_protocol_task(
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void * const data
) {
    (void)data;

    /*
     * Spawn the graphics and solver tasks required to receive the multicast Puzzle specification. The solver task may
     * recurse, so we provide a large statically allocated stack in excess of standard FreeRTOS limits.
     */
    xTaskCreate(&draw_puzzles_task, "draw_puzzles_task", THREAD_STACKSIZE, NULL, DEFAULT_THREAD_PRIO - 1, NULL);
    xTaskCreateStatic(
        &solve_puzzles_task, "solve_puzzles_task", sizeof(solve_puzzles_stack) / sizeof(*solve_puzzles_stack), NULL,
        DEFAULT_THREAD_PRIO - 1, solve_puzzles_stack, &solve_puzzles_pcb
    );

    struct Metadata request_metadata;

    // ReSharper disable once CppDFAEndlessLoop
    while (1)
        if (xQueueReceive(requests_queue, &request_metadata, portMAX_DELAY) == pdTRUE) {
            // On receipt of a request, query the server to build a puzzle of the specified parameters.
            xSemaphoreTake(network_state.mutex, portMAX_DELAY);
            struct Puzzle * const puzzle_info = build_puzzle_data(network_state.sock, &network_state.dst_addr,
                &request_metadata);
            xSemaphoreGive(network_state.mutex);

            if (puzzle_info != NULL) {
                // Broadcast out the puzzle specification to the graphics handler and solver.
                xQueueSend(graphics_queue, &puzzle_info, portMAX_DELAY);
                xQueueSend(challenge_queue, &puzzle_info, portMAX_DELAY);
            } else
                // If we couldn't procure the requested puzzle, ask for another one!
                xTaskNotify(accept_input_task_handle, 0, eNoAction);
        }

    // ReSharper disable once CppDFAUnreachableCode
    vTaskDelete(NULL);
}

static void submit_protocol_task(
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void * const data
) {
    (void)data;

    static struct sockaddr_in src_addr;
    struct Puzzle * puzzle_info = NULL;
    struct Result result = {.status = RESULT_ERROR};

    // ReSharper disable once CppDFAEndlessLoop
    while (1)
        if (xQueueReceive(solution_queue, &puzzle_info, portMAX_DELAY) == pdTRUE) {
            // Send the puzzle to the server for verification.
            xSemaphoreTake(network_state.mutex, portMAX_DELAY);

            result_send(puzzle_info, network_state.sock, &network_state.dst_addr);
            if (udp_receive_message(network_state.sock, &src_addr, &result, MSG_RESULT, &puzzle_info->metadata))
                result_print(&result);

            xSemaphoreGive(network_state.mutex);
            result.status = RESULT_ERROR;
            puzzle_free(puzzle_info);
            xTaskNotify(accept_input_task_handle, 0, eNoAction);
        }

    // ReSharper disable once CppDFAUnreachableCode
    vTaskDelete(NULL);
}
