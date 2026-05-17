// clang-format Language: C

/**
 * @file
 * @brief Clue data and chunking interface
 * @date 2026-05-15
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#ifndef CHUNKS_H
#define CHUNKS_H

#include <stdbool.h>
#include <stdint.h>

struct Metadata;

/**
 * @struct ClueGroup
 * @brief Grouped clue line for a single row or column; each eight-bit integer represents a single clue.
 */
struct ClueGroup
{
    uint8_t count; /**< @brief Number of clues */
    uint8_t * clues; /**< @brief Clue data */
};

/**
 * @struct Chunk
 * @brief A single chunk received from the Nonogram server containing clue lines.
 */
struct Chunk
{
    uint8_t chunk_id; /**< @brief Zero-based sequential ID of the chunk. */
    uint8_t num_chunks; /**< @brief Total number of chunks. */
    uint16_t offset; /**< @brief Offset of the Chunk ClueGroup first element within the total Puzzle clue data. */
    uint16_t data_length; /**< @brief Length of the clue data. */
    struct ClueGroup * clue_data; /**< @brief Clue groups. */
    unsigned int clue_group_count; /**< @brief Number of clue groups. */
    unsigned int max_clue_data_count; /**< @brief Clue count of the largest read group. */
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
 * @return Was the Chunk successfully parsed?
 * @pre The payload contains the <code>MSG_CHUNK_DATA</code> identifier in the first byte.
 * @note This function performs dynamic allocation to store the clues, as there is no upper bound on
 * their size or numerosity.
 */
bool chunk_parse(
    struct Chunk *dst,
    const struct Metadata *match_metadata,
    const uint8_t *payload
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
