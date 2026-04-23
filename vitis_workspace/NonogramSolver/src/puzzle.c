#include <assert.h>
#include <xil_printf.h>

#include "puzzle.h"

void puzzle_request(const struct MessageRequestInfo * const data,
    struct udp_pcb * const pcb, const ip_addr_t * dst_ip, const uint16_t dst_port)
{
    if (!pcb)
        return;
	
    const size_t payload_size = sizeof(struct MessageRequestInfo);
    struct pbuf * const buffer = pbuf_alloc(PBUF_TRANSPORT, payload_size, PBUF_RAM);
    if (!buffer) {
        xil_printf("puzzle_request: pbuf_alloc failed\r\n");
        return;
    }

    uint8_t * buffer_head = buffer->payload;

    // 1. Message ID (1 byte)
    *buffer_head++ = MSG_REQUEST_INFO;

    // 2. Puzzle metadata
    buffer_head = metadata_hton(&data->metadata, buffer_head);

    const err_t status = udp_sendto(pcb, buffer, dst_ip, dst_port);
    if (status != ERR_OK)
        xil_printf("puzzle_request: udp_sendto failed: %d\r\n", status);

    pbuf_free(buffer);
}

struct MessagePuzzleInfo puzzle_parse(const uint8_t * payload)
{
    assert(*payload == MSG_PUZZLE_INFO);
    payload += sizeof(uint8_t);

    struct MessagePuzzleInfo puzzle_info;
    payload = metadata_parse(&puzzle_info.metadata, payload);

    if (!puzzle_info.metadata.valid) {
        xil_printf("puzzle_parse: quitting early due to bad metadata.\r\n");
        return puzzle_info;
    }

    puzzle_info.width = *payload++;
    puzzle_info.height = *payload++;
    puzzle_info.num_chunks = *payload++;

    puzzle_info.clue_bytes = *payload++ << 8;
    puzzle_info.clue_bytes |= *payload;

    return puzzle_info;
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
