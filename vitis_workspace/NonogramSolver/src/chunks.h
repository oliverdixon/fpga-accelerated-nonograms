#ifndef MESSAGE_REQUEST_CHUNK_H
#define MESSAGE_REQUEST_CHUNK_H

#include <lwip/sockets.h>

#include "metadata.h"

/*
 * Message ID (1 byte)
 * Metadata
 * Chunk ID (1 byte)
 * Number of chunks (1 byte)
 * Offset (2 bytes)
 * Data length (2 bytes)
 */
#define MESSAGE_CHUNK_DATA_MIN_LENGTH (1 + MESSAGE_METADATA_LENGTH + 1 + 1 + 2 + 2)

/*
 * Message ID (1 byte)
 * Metadata
 * Chunk ID (1 byte)
 */
#define MESSAGE_REQUEST_CHUNK_LENGTH (1 + MESSAGE_METADATA_LENGTH + 1)

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
    unsigned int clue_count;
    unsigned int max_clue_data_count;
};

void chunk_request(const struct MessageRequestChunk * data,
    int sock, const struct sockaddr_in * dst_addr);

int chunk_parse(struct MessageChunkData * dst, const uint8_t * payload);
void chunk_free(const struct MessageChunkData * chunk_data);
void chunk_print(const struct MessageChunkData * chunk_data);

#endif // MESSAGE_REQUEST_CHUNK_H
