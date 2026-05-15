// clang-format Language: C

/**
 * @file
 * @brief Server error message interface
 * @date 2026-05-15
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#ifndef ERROR_H
#define ERROR_H

#include "metadata.h"

#define ERROR_MAX_LENGTH (200)

/**
 * @struct ServerError
 * @brief Represents an error returned from the Nonogram server.
 */
struct ServerError
{
    struct Metadata metadata;
    uint8_t original_msg_id;
    uint8_t text_length;
    char error_text[ERROR_MAX_LENGTH + 1];
};

/**
 * @brief Parses a <code>MSG_ERROR</code> message from the server into the given structure.
 * @param dst The destination structure for the error message.
 * @param payload The bytes of the entire payload received from the server.
 * @return 0 on success, -1 on failure.
 * @pre The payload contains the <code>MSG_ERROR</code> identifier in the first byte.
 */
int error_parse(
    struct ServerError * dst,
    const uint8_t * payload
);

/**
 * @brief Serialises the given error message to the serial output.
 * @param message The ServerError message to serialise.
 */
void error_print(const struct ServerError * message);

#endif // ERROR_H
