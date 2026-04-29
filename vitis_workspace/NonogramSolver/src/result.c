#include <assert.h>
#include <lwip/sockets.h>
#include <xil_printf.h>

#include "logging.h"
#include "metadata.h"
#include "puzzle.h"
#include "result.h"

/*
 * Message ID (1 byte)
 * Metadata
 */
#define MESSAGE_SUBMIT_SOLUTION_MAX_LENGTH                                                         \
    (1 + MESSAGE_METADATA_LENGTH + ((MAX_SIZE + 1) / 8) * MAX_SIZE)

static uint8_t send_buf[MESSAGE_SUBMIT_SOLUTION_MAX_LENGTH];

static uint8_t pack_left_aligned_byte(
    const line_t row,
    const uint8_t bit_offset,
    const uint8_t bit_count
) {
    const line_t mask = (1U << bit_count) - 1U;
    return ((row >> bit_offset) & mask) << (8U - bit_count);
}

int result_parse(
    struct MessageResult * const result,
    const struct Metadata * const metadata,
    const uint8_t * payload
) {
    assert(*payload == MSG_RESULT);
    payload += sizeof(uint8_t);

    // Verify that the received metadata matches what we expect.
    struct Metadata received_metadata;
    payload = metadata_parse(&received_metadata, payload);

    if (!received_metadata.valid || !metadata_equal(metadata, &received_metadata)) {
        logging_puts("result_parse: quitting early due to bad metadata.\r\n");
        return -1;
    }

    result->status = *payload++;

    result->solve_time = *payload++ << 8;
    result->solve_time |= *payload++ << 8;
    result->solve_time |= *payload++ << 8;
    result->solve_time |= *payload;

    return 0;
}

int result_send(
    const struct Puzzle * const puzzle,
    const int sock,
    const struct sockaddr_in * const dst_addr
) {
    if (!puzzle->is_solved)
        // What's the point in submitting a solution if we don't have one?
        return -1;

    uint8_t * buffer_head = send_buf;

    // 1. Message ID (1 byte)
    *buffer_head++ = MSG_SUBMIT_SOLUTION;

    // 2. Puzzle metadata
    buffer_head = metadata_hton(&puzzle->metadata, buffer_head);

    // 3. Solution bitmap (ceil(width / 8) * height bytes)
    xSemaphoreTake(puzzle->solution_semaphore, portMAX_DELAY);

    for (uint8_t row_idx = 0; row_idx < puzzle->height; ++row_idx) {
        const line_t row = puzzle->solution_bitmap[row_idx];

        // Pack the bitmap as bytes of left-aligned bits.
        for (uint8_t bit_offset = 0; bit_offset < puzzle->width; bit_offset += 8) {
            uint8_t bit_count = puzzle->width - bit_offset;
            if (bit_count > 8)
                bit_count = 8;
            *buffer_head++ = pack_left_aligned_byte(row, bit_offset, bit_count);
        }
    }

    xSemaphoreGive(puzzle->solution_semaphore);

    lwip_sendto(
        sock, send_buf, buffer_head - send_buf, 0, (struct sockaddr *)dst_addr,
        sizeof(struct sockaddr_in)
    );

    return 0;
}

void result_print(
    const struct MessageResult * const result
) {
    xil_printf("Result in %d milliseconds: ", result->solve_time);

    switch (result->status) {
    case RESULT_INCORRECT:
        print("Incorrect!\r\n");
        break;
    case RESULT_CORRECT:
        print("Correct!\r\n");
        break;
    case RESULT_ERROR:
        print("Error!\r\n");
        break;
    }
}
