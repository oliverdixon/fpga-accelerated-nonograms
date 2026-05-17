/**
 * @file
 * @brief Puzzle result implementation
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#include <assert.h>
#include <xil_printf.h>
#include <lwip/sockets.h>

#include "result.h"
#include "logging.h"
#include "metadata.h"
#include "puzzle.h"
#include "../../SolverCore/src/solver_params.h"

/**
 * @brief The length, in bytes, of a verification request message sent on the wire.
 * @details
 *  <ul>
 *      <li>Message type ID</li>
 *      <li>Puzzle Metadata</li>
 *      <li>Solution bitmap (packed into bits on logical lines)</li>
 *  </ul>
 */
#define MESSAGE_SUBMIT_SOLUTION_MAX_LENGTH (1 + MESSAGE_METADATA_LENGTH + ((MAX_SIZE + 1) / 8) * MAX_SIZE)

static uint8_t send_buf[MESSAGE_SUBMIT_SOLUTION_MAX_LENGTH]; /**< @brief The persistent buffer to prepare requests. */

bool result_parse(
    struct Result *const result,
    const struct Metadata *const metadata,
    const uint8_t *payload
) {
    assert(*payload == MSG_RESULT);
    payload += sizeof(uint8_t);

    // Verify that the received metadata matches what we expect.
    struct Metadata received_metadata;
    payload = metadata_parse(&received_metadata, payload);

    if (!received_metadata.valid || !metadata_equal(metadata, &received_metadata)) {
        logging_puts("result_parse: quitting early due to bad metadata.");
        return false;
    }

    result->status = *payload++;

    result->solve_time = *payload++ << 8;
    result->solve_time |= *payload++ << 8;
    result->solve_time |= *payload++ << 8;
    result->solve_time |= *payload;

    return true;
}

void result_send(
    const struct Puzzle *const puzzle,
    const int sock,
    const struct sockaddr_in *const dst_addr
) {
    if (puzzle->solved_state != SEARCH_SOLVED)
        // What's the point in submitting a solution if we don't have one?
        return;

    uint8_t * buffer_head = send_buf;

    // 1. Message ID (1 byte)
    *buffer_head++ = MSG_SUBMIT_SOLUTION;

    // 2. Puzzle metadata
    buffer_head = metadata_hton(&puzzle->metadata, buffer_head);

    // 3. Solution bitmap (ceil(width / 8) * height bytes)
    xSemaphoreTake(puzzle->solution_semaphore, portMAX_DELAY);

    for (uint8_t row_idx = 0; row_idx < puzzle->height; ++row_idx) {
        const line_t row = puzzle->solution_bitmap[row_idx];

        // Pack the row into a bitmap of bytes of left-aligned bits.
        for (uint8_t col_idx = 0; col_idx < puzzle->width; col_idx += 8) {
            uint8_t packed = 0;

            for (uint8_t bit = 0; bit < 8 && col_idx + bit < puzzle->width; ++bit)
                if (row & 1U << (col_idx + bit))
                    packed |= (uint8_t)(1U << (7U - bit));

            *buffer_head++ = packed;
        }
    }

    xSemaphoreGive(puzzle->solution_semaphore);

    lwip_sendto(
        sock, send_buf, buffer_head - send_buf, 0, (struct sockaddr *)dst_addr,
        sizeof(struct sockaddr_in)
    );
}

void result_print(
    const struct Result * const result
) {
    xil_printf("Server timed result to %d milliseconds: ", result->solve_time);

    switch (result->status) {
    case RESULT_INCORRECT:
        print("Server says: Incorrect!\r\n");
        break;
    case RESULT_CORRECT:
        print("Server says: Correct!\r\n");
        break;
    default:
        print("Server says: Error!\r\n");
        break;
    }
}
