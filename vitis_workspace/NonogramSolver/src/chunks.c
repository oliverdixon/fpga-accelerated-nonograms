#include <assert.h>
#include <stdlib.h>
#include <xil_printf.h>

#include "chunks.h"

static uint8_t send_buf[MESSAGE_REQUEST_CHUNK_LENGTH];

void chunk_request(const struct MessageRequestChunk * data,
        const int sock, const struct sockaddr_in * const dst_addr)
{
    uint8_t * buffer_head = send_buf;

    // 1. Message ID (1 byte)
    *buffer_head++ = MSG_REQUEST_CHUNK;

    // 2. Puzzle metadata
    buffer_head = metadata_hton(&data->metadata, buffer_head);

    // 3. Requested chunk ID (1 byte)
    *buffer_head++ = data->chunk_id;

    sendto(sock, send_buf, buffer_head - send_buf, 0, (struct sockaddr *) dst_addr,
        sizeof(struct sockaddr_in));
}

int chunk_parse(struct MessageChunkData * const dst, const uint8_t * payload)
{
    assert(*payload == MSG_CHUNK_DATA);
    payload += sizeof(uint8_t);

    dst->max_clue_data_count = 0;

    payload = metadata_parse(&dst->metadata, payload);

    if (!dst->metadata.valid) {
        print("chunk_parse: quitting early due to bad metadata.\r\n");
        return -1;
    }

    dst->chunk_id = *payload++;
    dst->num_chunks = *payload++;

    dst->offset = *payload++ << 8;
    dst->offset |= *payload++;

    dst->data_length = *payload++ << 8;
    dst->data_length |= *payload++;

    dst->clue_data = malloc(sizeof(struct ClueData) * dst->data_length);
    const uint8_t * const clue_end = payload + sizeof(uint8_t) * dst->data_length;

    size_t line_idx = 0;

    for (; payload < clue_end; ++line_idx) {
        // Get the number of elements for this line and read them into the clue data.
        struct ClueData * clue_line = &dst->clue_data[line_idx];
        clue_line->count = *payload++;

        if (clue_line->count > dst->max_clue_data_count)
            dst->max_clue_data_count = clue_line->count;

        clue_line->blocks = malloc(sizeof(uint8_t) * clue_line->count);
        for (uint8_t element_idx = 0; element_idx < clue_line->count; ++element_idx)
            clue_line->blocks[element_idx] = *payload++;
    }

    dst->clue_count = line_idx;
    return 0;
}

void chunk_free(const struct MessageChunkData * chunk_data)
{
    for (size_t line_idx = 0; line_idx < chunk_data->clue_count; ++line_idx)
        free(chunk_data->clue_data[line_idx].blocks);

    free(chunk_data->clue_data);
}

void chunk_print(const struct MessageChunkData * const chunk_data)
{
    print("\r\n");
    
    if (chunk_data->metadata.valid) {
        print("MessageChunkData:\r\n\t");
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
            print("\r\n");
        }
    } else
        print("MessageChunkData: INVALID\r\n");

    print("\r\n");
}
