#include <assert.h>
#include <stdlib.h>
#include <xil_printf.h>

#include "chunks.h"

void chunk_request(const struct MessageRequestChunk * data, struct udp_pcb * const pcb,
	const ip_addr_t * const dst_ip, const uint16_t dst_port)
{
    if (!pcb)
        return;
	
    const size_t payload_size = sizeof(struct MessageRequestChunk);
    struct pbuf * const buffer = pbuf_alloc(PBUF_TRANSPORT, payload_size, PBUF_RAM);
    if (!buffer) {
        xil_printf("chunk_request: pbuf_alloc failed\r\n");
        return;
    }

    uint8_t * buffer_head = buffer->payload;

    // 1. Message ID (1 byte)
    *buffer_head++ = MSG_REQUEST_CHUNK;

    // 2. Puzzle metadata
    buffer_head = metadata_hton(&data->metadata, buffer_head);

    // 3. Requested chunk ID (1 byte)
    *buffer_head++ = data->chunk_id;

    const err_t status = udp_sendto(pcb, buffer, dst_ip, dst_port);
    if (status != ERR_OK)
        xil_printf("chunk_request: udp_sendto failed: %d\r\n", status);

    pbuf_free(buffer);
}

struct MessageChunkData chunk_parse(const uint8_t * payload)
{
    assert(*payload == MSG_CHUNK_DATA);
    payload += sizeof(uint8_t);

    struct MessageChunkData chunk_data;
    payload = metadata_parse(&chunk_data.metadata, payload);

    if (!chunk_data.metadata.valid) {
        xil_printf("chunk_parse: quitting early due to bad metadata.\r\n");
        return chunk_data;
    }

    chunk_data.chunk_id = *payload++;
    chunk_data.num_chunks = *payload++;

    chunk_data.offset = *payload++ << 8;
    chunk_data.offset |= *payload++;

    chunk_data.data_length = *payload++ << 8;
    chunk_data.data_length |= *payload++;

    chunk_data.clue_data = malloc(sizeof(struct ClueData) * chunk_data.data_length);
    const uint8_t * const clue_end = payload + sizeof(uint8_t) * chunk_data.data_length;

    size_t line_idx = 0;

    for (; payload < clue_end; ++line_idx) {
        // Get the number of elements for this line and read them into the clue data.
        struct ClueData * clue_line = &chunk_data.clue_data[line_idx];
        clue_line->count = *payload++;
        clue_line->blocks = malloc(sizeof(uint8_t) * clue_line->count);
        for (uint8_t element_idx = 0; element_idx < clue_line->count; ++element_idx)
            clue_line->blocks[element_idx] = *payload++;
    }

    chunk_data.clue_count = line_idx;
    return chunk_data;
}

void chunk_free(struct MessageChunkData * chunk_data)
{
    for (size_t line_idx = 0; line_idx < chunk_data->clue_count; ++line_idx)
        free(chunk_data->clue_data[line_idx].blocks);

    free(chunk_data->clue_data);
}

void chunk_print(const struct MessageChunkData * const chunk_data)
{
    if (chunk_data->metadata.valid) {
        xil_printf("MessageChunkData:\r\n\t");
        metadata_print(&chunk_data->metadata);
        xil_printf("\tChunk ID: %d\r\n\tChunk Count: %d\r\n\tOffset: %d\r\n\tData Length: %d\r\n"
            "\tClue Count (derived): %d\r\n",
            chunk_data->chunk_id, chunk_data->num_chunks, chunk_data->offset,
            chunk_data->data_length, chunk_data->clue_count);

        for (size_t line_idx = 0; line_idx < chunk_data->clue_count; ++line_idx) {
            struct ClueData * clue_line = &chunk_data->clue_data[line_idx];
            xil_printf("\tClue %02d: ", line_idx);
            for (uint8_t element_idx = 0; element_idx < clue_line->count; ++element_idx)
                xil_printf("%02x ", clue_line->blocks[element_idx]);
            xil_printf("\r\n");
        }
    } else
        xil_printf("MessageChunkData: INVALID\r\n");
}
