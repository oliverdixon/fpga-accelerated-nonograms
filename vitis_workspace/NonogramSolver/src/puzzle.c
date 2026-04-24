#include <assert.h>
#include <xil_printf.h>

#include "puzzle.h"

static uint8_t send_buf[16]; // TODO reduce size

void puzzle_request(const struct MessageRequestInfo * const data,
     const int sock, const struct sockaddr_in * const dst_addr)
{
    uint8_t * buffer_head = send_buf;

    // 1. Message ID (1 byte)
    *buffer_head++ = MSG_REQUEST_INFO;

    // 2. Puzzle metadata
    buffer_head = metadata_hton(&data->metadata, buffer_head);

    sendto(sock, send_buf, buffer_head - send_buf, 0, (struct sockaddr *) dst_addr,
        sizeof(struct sockaddr_in));
}

int puzzle_parse(struct MessagePuzzleInfo * const dst, const uint8_t * payload)
{
    assert(*payload == MSG_PUZZLE_INFO);
    payload += sizeof(uint8_t);

    payload = metadata_parse(&dst->metadata, payload);

    if (!dst->metadata.valid) {
        xil_printf("puzzle_parse: quitting early due to bad metadata.\r\n");
        return -1;
    }

    dst->width = *payload++;
    dst->height = *payload++;
    dst->num_chunks = *payload++;

    dst->clue_bytes = *payload++ << 8;
    dst->clue_bytes |= *payload;

    dst->global_max_clue_data_count = 0;

    return 0;
}

void puzzle_print(const struct MessagePuzzleInfo * const puzzle_info)
{
    if (puzzle_info->metadata.valid) {
        xil_printf("MessagePuzzleInfo:\r\n\t");
        metadata_print(&puzzle_info->metadata);
        xil_printf("\tWidth: %d\r\n\tHeight: %d\r\n\tChunk Count: %d\r\n\tClue Bytes: %d\r\n",
            puzzle_info->width, puzzle_info->height, puzzle_info->num_chunks,
            puzzle_info->clue_bytes);
    } else
        xil_printf("MessagePuzzleInfo: INVALID\r\n");
}
