// clang-format Language: C

/**
 * @file
 * @brief Puzzle result interface
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#ifndef RESULT_H
#define RESULT_H

#include <stdbool.h>
#include <stdint.h>

struct Metadata;
struct Puzzle;
struct sockaddr_in;

/**
 * @enum ResultCode
 * @brief Server indications of the correctness of a submitted Puzzle.
 */
enum ResultCode
{
    RESULT_INCORRECT = 0x00, /**< @brief The solution was definitely incorrect. */
    RESULT_CORRECT = 0x01, /**< @brief The solution was definitely correct. */
    RESULT_ERROR = 0x02 /**< @brief Some error occurred when checking the solution. */
};

/**
 * @struct Result
 * @brief The response from the server examining the submitted solution.
 */
struct Result
{
    enum ResultCode status : 8; /**< @brief The overall determination. */
    uint32_t solve_time; /**< @brief The round-trip time from sending the Puzzle specification to the response. */
};

/**
 * @brief Parse the given payload bytes into the given Result buffer.
 * @param result The destination Result.
 * @param metadata The Metadata which the described Result is expected to match.
 * @param payload The big-endian bytes received from the Nonogram server.
 * @return Was the payload successfully parsed into the given Result?
 * @pre The payload must represent a <code>MSG_RESULT</code> object.
 */
bool result_parse(
    struct Result *result,
    const struct Metadata *metadata,
    const uint8_t *payload
);

/**
 * @brief Send a verification request to the Nonogram server.
 * @param puzzle The Puzzle to have verified.
 * @param sock The opened socket to the Nonogram server.
 * @param dst_addr The address of the Nonogram server.
 */
void result_send(
    const struct Puzzle *puzzle,
    int sock,
    const struct sockaddr_in *dst_addr
);

/**
 * @brief Serialise the given Result to the unbuffered serial line.
 * @param result The Result to serialise.
 */
void result_print(const struct Result * result);

#endif // RESULT_H
