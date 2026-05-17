/**
 * @file
 * @brief Puzzle implementation
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#include <assert.h>
#include <string.h>
#include <xil_printf.h>
#include <lwip/sockets.h>

#include "puzzle.h"
#include "chunks.h"
#include "logging.h"

/**
 * @brief The length, in bytes, of a Puzzle request message sent on the wire.
 * @details
 *  <ul>
 *      <li>Message type ID</li>
 *      <li>Puzzle Metadata</li>
 *  </ul>
 */
#define MESSAGE_REQUEST_INFO_LENGTH (1 + MESSAGE_METADATA_LENGTH)

static uint8_t send_buf[MESSAGE_REQUEST_INFO_LENGTH]; /**< @brief The persistent buffer to prepare Puzzle requests. */

void puzzle_request(
    const struct Metadata * const metadata,
    const int sock,
    const struct sockaddr_in * const dst_addr
) {
    uint8_t * buffer_head = send_buf;

    // 1. Message ID (1 byte)
    *buffer_head++ = MSG_REQUEST_INFO;

    // 2. Puzzle metadata
    buffer_head = metadata_hton(metadata, buffer_head);

    lwip_sendto(
        sock, send_buf, buffer_head - send_buf, 0, (struct sockaddr *)dst_addr,
        sizeof(struct sockaddr_in)
    );
}

bool puzzle_parse(
    struct Puzzle *const puzzle,
    const uint8_t *payload
) {
    assert(*payload == MSG_PUZZLE_INFO);
    payload += sizeof(uint8_t);

    payload = metadata_parse(&puzzle->metadata, payload);

    if (!puzzle->metadata.valid) {
        logging_puts("puzzle_parse: quitting early due to bad metadata.");
        return false;
    }

    puzzle->width = *payload++;
    puzzle->height = *payload++;
    puzzle->num_chunks = *payload++;

    puzzle->clue_bytes = *payload++ << 8;
    puzzle->clue_bytes |= *payload;

    puzzle->global_max_clue_data_count = 0;
    puzzle->solved_state = SEARCH_NOT_RUN;

    if ((puzzle->solution_semaphore = xSemaphoreCreateMutex()) == NULL)
        return false;

    memset(puzzle->solution_bitmap, 0, sizeof(line_t) * puzzle->width);

    return true;
}

void puzzle_print(
    const struct Puzzle * const puzzle
) {
    print("\r\n");

    if (puzzle->metadata.valid) {
        print("Puzzle:\r\n\t");
        metadata_print(&puzzle->metadata);
        xil_printf(
            "\tWidth: %d\r\n\tHeight: %d\r\n\tChunk Count: %d\r\n\tClue Bytes: %d\r\n",
            puzzle->width, puzzle->height, puzzle->num_chunks,
            puzzle->clue_bytes
        );
    } else
        print("Puzzle: INVALID\r\n");

    print("\r\n");
}

void puzzle_free(
    struct Puzzle * const puzzle
) {
    if (puzzle->solution_semaphore != NULL) {
        /*
         * We need to make sure the semaphore is free, but vSemaphoreDelete indicates that it shouldn't be held by any
         * task.
         */
        xSemaphoreTake(puzzle->solution_semaphore, portMAX_DELAY);
        xSemaphoreGive(puzzle->solution_semaphore);
        vSemaphoreDelete(puzzle->solution_semaphore);
    }

    chunk_free(&puzzle->chunk);
    free(puzzle);
}
