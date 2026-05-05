// clang-format Language: C

#ifndef CHUNKS_H
#define CHUNKS_H

#include <stdint.h>

struct Metadata;

struct ClueData
{
    uint8_t count;
    uint8_t * blocks;
};

struct Chunk
{
    uint8_t chunk_id;
    uint8_t num_chunks;
    uint16_t offset;
    uint16_t data_length;
    struct ClueData * clue_data;
    unsigned int clue_count;
    unsigned int max_clue_data_count;
};

struct sockaddr_in;

void chunk_request(
    uint8_t chunk_id,
    int sock,
    const struct sockaddr_in * dst_addr,
    const struct Metadata * const metadata
);

int chunk_parse(
    struct Chunk * dst,
    const struct Metadata * match_metadata,
    const uint8_t * payload
);

void chunk_free(const struct Chunk * chunk_data);
void chunk_print(const struct Chunk * chunk_data);

#endif // CHUNKS_H
