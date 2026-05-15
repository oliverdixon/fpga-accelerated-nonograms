// clang-format Language: C

/**
 * @file
 * @brief Clue data and chunking interface
 * @date 2026-05-15
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#ifndef CHUNKS_H
#define CHUNKS_H

#include <stdint.h>

struct Metadata;

/**
 * @brief Clue line for a single row or column; each eight-bit integer represents a single clue.
 */
struct ClueData
{
    uint8_t count;
    uint8_t * blocks;
};

/**
 * @brief A single chunk received from the Nonogram server containing clue lines.
 */
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

/**
 * @brief Issue a <code>MSG_REQUEST_CHUNK</code> to the server to request a single Chunk.
 * @param chunk_id The ID of the Chunk to request.
 * @param sock The network socket to the Nonogram server.
 * @param dst_addr The address of the Nonogram server.
 * @param metadata Metadata of the puzzle relating to the requested Chunk.
 */
void chunk_request(
    uint8_t chunk_id,
    int sock,
    const struct sockaddr_in * dst_addr,
    const struct Metadata * metadata
);

/**
 * @brief Parse a received <code>MSG_CHUNK_DATA</code> into the given destination Chunk.
 * @param dst The destination of the parsed Chunk.
 * @param match_metadata Metadata of the puzzle being requested, which the received Chunk is
 * expected to match.
 * @param payload The entire bytes received from the server.
 * @return 0 on success, -1 on failure (if the payload metadata didn't match the expected metadata).
 * @pre The payload contains the <code>MSG_CHUNK_DATA</code> identifier in the first byte.
 * @note This function performs dynamic allocation to store the clues, as there is no upper bound on
 * their size or numerosity.
 */
int chunk_parse(
    struct Chunk * dst,
    const struct Metadata * match_metadata,
    const uint8_t * payload
);

/**
 * @brief Frees dynamically allocated data from a Chunk
 * @param chunk_data The Chunk to free
 */
void chunk_free(const struct Chunk * chunk_data);

/**
 * @brief Serialises a Chunk with its ClueData on the serial output.
 * @param chunk_data The Chunk to serialise
 */
void chunk_print(const struct Chunk * chunk_data);

#endif // CHUNKS_H
