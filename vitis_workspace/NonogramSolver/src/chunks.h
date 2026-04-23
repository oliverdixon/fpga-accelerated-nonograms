#ifndef MESSAGE_REQUEST_CHUNK_H
#define MESSAGE_REQUEST_CHUNK_H

#include <lwip/udp.h>
#include <lwip/ip.h>

#include "metadata.h"

struct MessageRequestChunk
{
    struct PuzzleMetadata metadata;
    uint8_t chunk_id;
};

struct ClueData
{
    uint8_t count;
    uint8_t * blocks;
};

struct MessageChunkData
{
    struct PuzzleMetadata metadata;
    uint8_t chunk_id;
    uint8_t num_chunks;
    uint16_t offset;
    uint16_t data_length;
    struct ClueData * clue_data;
    size_t clue_count;
};

void chunk_request(const struct MessageRequestChunk * data, struct udp_pcb * pcb,
	const ip_addr_t * dst_ip, uint16_t dst_port);

struct MessageChunkData chunk_parse(const uint8_t * payload);
void chunk_free(struct MessageChunkData * chunk_data);
void chunk_print(const struct MessageChunkData * chunk_data);

#endif // MESSAGE_REQUEST_CHUNK_H
