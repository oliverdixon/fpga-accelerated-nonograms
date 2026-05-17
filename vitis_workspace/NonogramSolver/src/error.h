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

#define ERROR_MAX_LENGTH (200) /**< @brief Protocol-defined maximum length of the error string, in bytes. */

/**
 * @struct ServerError
 * @brief Represents an error returned from the Nonogram server.
 */
struct ServerError
{
    struct Metadata metadata; /**< @brief Optional Puzzle Metadata to which the ServerError relates. */
    enum MessageType original_msg_id; /**< @brief The type of message which caused the ServerError. */
    uint8_t text_length; /**< @brief The length of the error text in bytes. */
    char error_text[ERROR_MAX_LENGTH + 1]; /**< @brief A human-readable description of the ServerError. */
};

/**
 * @brief Parses a <code>MSG_ERROR</code> message from the server into the given structure.
 * @param dst The destination structure for the error message.
 * @param payload The bytes of the entire payload received from the server.
 * @return Was the payload successfully parsed into the ServerError?
 * @pre The payload contains the <code>MSG_ERROR</code> identifier in the first byte.
 */
bool error_parse(
    struct ServerError *dst,
    const uint8_t *payload
);

/**
 * @brief Serialises the given error message to the serial output.
 * @param message The ServerError message to serialise.
 */
void error_print(const struct ServerError * message);

#endif // ERROR_H
