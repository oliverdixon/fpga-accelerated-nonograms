// clang-format Language: C

#ifndef PUZZLE_H
#define PUZZLE_H

#include <FreeRTOS.h>
#include <semphr.h>

#include "chunks.h"
#include "metadata.h"
#include "solver.h"

enum SearchResult
{
    SEARCH_SOLVED,
    SEARCH_FAILED,
    SEARCH_UNKNOWN,
    SEARCH_NOT_RUN
};

struct Puzzle
{
    struct Metadata metadata;
    uint8_t width;
    uint8_t height;
    uint8_t num_chunks;
    uint16_t clue_bytes;

    unsigned int global_max_clue_data_count;
    struct Chunk chunk; // TODO: support multiple chunks // TODO needs to be a ptr protected by mutexes.

    enum SearchResult solved_state;
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
void puzzle_free(const struct Puzzle * puzzle);

#endif // PUZZLE_H
