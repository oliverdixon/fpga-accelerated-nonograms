// clang-format Language: C

/**
 * @file
 * @brief Puzzle interface
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#ifndef PUZZLE_H
#define PUZZLE_H

#include <FreeRTOS.h>
#include <semphr.h>

#include "../../SolverCore/src/solver_params.h"
#include "chunks.h"
#include "metadata.h"

/**
 * @enum SearchResult
 * @brief Result of an attempted searching solve.
 */
enum SearchResult
{
    SEARCH_SOLVED,  /**< @brief The Puzzle was successfully solved. */
    SEARCH_FAILED,  /**< @brief The search failed due to an inconsistent Puzzle. */
    SEARCH_UNKNOWN, /**< @brief The search could not come to a definite conclusion. */
    SEARCH_NOT_RUN  /**< @brief The search has not been run. */
};

/**
 * @struct Puzzle
 * @brief The full specification of a single-Chunk Nonogram Puzzle including Metadata and an optional solution bitmap.
 */
struct Puzzle
{
    struct Metadata metadata; /**< @brief Metadata for the Puzzle */
    uint8_t width;            /**< @brief Width of the Puzzle, in number of cells. */
    uint8_t height;           /**< @brief Height of the Puzzle, in number of cells. */
    uint8_t num_chunks;       /**< @brief Number of Chunks used to store the clues. */
    uint16_t clue_bytes;      /**< @brief Number of bytes used to transmit the clue data over the wire. */

    unsigned int global_max_clue_data_count; /**< @brief Maximum number of clues in any group of the Puzzle. */
    struct Chunk chunk;                      /**< @brief The single Chunk containing clue data. */

    enum SearchResult solved_state;       /**< @brief The last-known solution state of the Puzzle. */
    SemaphoreHandle_t solution_semaphore; /**< @brief A semaphore to protect the solution bitmap. */
    line_t solution_bitmap[MAX_SIZE];     /**< @brief The solution bitmap indicating black-cell assignments. */
};

/**
 * @brief Send a Puzzle request to the Nonogram server given the specification Metadata.
 * @param metadata The Metadata of the requested Puzzle.
 * @param sock The opened socket to the Nonogram server.
 * @param dst_addr The address of the Nonogram server.
 */
void puzzle_request(
    const struct Metadata * metadata,
    int sock,
    const struct sockaddr_in * dst_addr
);

/**
 * @brief Parse a received byte payload into the given Puzzle buffer.
 * @param puzzle The destination Puzzle buffer.
 * @param payload The payload received
 * @return Was the given payload successfully parsed into the destination Puzzle management structure?
 * @pre The payload must have type <code>MSG_PUZZLE_INFO</code>.
 */
bool puzzle_parse(
    struct Puzzle * puzzle,
    const uint8_t * payload
);

/**
 * @brief Serialise the given Puzzle management structure (with all clue data) to the unbuffered serial line.
 * @param puzzle The Puzzle to serialise.
 */
void puzzle_print(const struct Puzzle * puzzle);

/**
 * @brief Frees all dynamically allocated memory managed by the given Puzzle.
 * @param puzzle The Puzzle to de-allocate.
 */
void puzzle_free(struct Puzzle * puzzle);

#endif // PUZZLE_H
