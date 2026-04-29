// clang-format Language: C

#ifndef PUZZLE_H
#define PUZZLE_H

#include <FreeRTOS.h>
#include <semphr.h>

#include "chunks.h"
#include "metadata.h"
#include "solver.h"

struct Puzzle
{
    struct Metadata metadata;
    uint8_t width;
    uint8_t height;
    uint8_t num_chunks;
    uint16_t clue_bytes;

    unsigned int global_max_clue_data_count;
    struct Chunk chunk; // TODO: support multiple chunks

    bool is_solved;
    SemaphoreHandle_t solution_semaphore;
    line_t * solution_bitmap;
};

void puzzle_request(
    const struct Metadata * metadata,
    int sock,
    const struct sockaddr_in * dst_addr
);

int puzzle_parse(
    struct Puzzle * dst,
    const uint8_t * payload
);
void puzzle_print(const struct Puzzle * puzzle_info);

#endif // PUZZLE_H
