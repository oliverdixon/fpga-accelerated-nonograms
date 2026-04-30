#include <assert.h>
#include <lwip/sockets.h>
#include <xil_printf.h>

#include "chunks.h"
#include "logging.h"
#include "puzzle.h"
#include "solver.h"

/*
 * Message ID (1 byte)
 * Metadata
 */
#define MESSAGE_REQUEST_INFO_LENGTH (1 + MESSAGE_METADATA_LENGTH)

static uint8_t send_buf[MESSAGE_REQUEST_INFO_LENGTH];
static line_t bitmap_buffer[MAX_SIZE];
static SemaphoreHandle_t bitmap_mutex = NULL;
static StaticSemaphore_t bitmap_mutex_state;

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

int puzzle_parse(
    struct Puzzle * const dst,
    const uint8_t * payload
) {
    assert(*payload == MSG_PUZZLE_INFO);
    payload += sizeof(uint8_t);

    payload = metadata_parse(&dst->metadata, payload);

    if (!dst->metadata.valid) {
        logging_puts("puzzle_parse: quitting early due to bad metadata.");
        return -1;
    }

    dst->width = *payload++;
    dst->height = *payload++;
    dst->num_chunks = *payload++;

    dst->clue_bytes = *payload++ << 8;
    dst->clue_bytes |= *payload;

    dst->global_max_clue_data_count = 0;
    dst->solution_bitmap = bitmap_buffer;

    if (bitmap_mutex == NULL)
        bitmap_mutex = xSemaphoreCreateMutexStatic(&bitmap_mutex_state);

    dst->solution_semaphore = bitmap_mutex;
    dst->solution_bitmap = bitmap_buffer;
    dst->solved_state = SEARCH_NOT_RUN;

    return 0;
}

void puzzle_print(
    const struct Puzzle * const puzzle_info
) {
    print("\r\n");

    if (puzzle_info->metadata.valid) {
        print("Puzzle:\r\n\t");
        metadata_print(&puzzle_info->metadata);
        xil_printf(
            "\tWidth: %d\r\n\tHeight: %d\r\n\tChunk Count: %d\r\n\tClue Bytes: %d\r\n",
            puzzle_info->width, puzzle_info->height, puzzle_info->num_chunks,
            puzzle_info->clue_bytes
        );
    } else
        print("Puzzle: INVALID\r\n");

    print("\r\n");
}

void puzzle_free(const struct Puzzle * const puzzle)
{
    chunk_free(&puzzle->chunk);
}
