// clang-format Language: C

/**
 * @file
 * @brief Puzzle Metadata interface
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#ifndef METADATA_H
#define METADATA_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief The length, in bytes, of a Metadata message received on the wire.
 * @details
 *  <ul>
 *      <li>4 bytes for the seed</li>
 *      <li>1 byte for the difficulty specification</li>
 *  </ul>
 */
#define MESSAGE_METADATA_LENGTH (4 + 1)

/**
 * @enum MessageType
 * @brief Known types of messages to the Nonogram server.
 */
enum MessageType
{
    MSG_NO_MESSAGE = 0x00,

    MSG_REQUEST_INFO = 0x01,
    MSG_PUZZLE_INFO = 0x02,
    MSG_REQUEST_CHUNK = 0x03,
    MSG_CHUNK_DATA = 0x04,
    MSG_SUBMIT_SOLUTION = 0x05,
    MSG_RESULT = 0x06,

    MSG_ERROR = 0xFF
};

/**
 * @enum DifficultyTier
 * @brief Known difficulty tiers to the Nonogram server.
 */
enum DifficultyTier
{
    DIFFICULTY_CUSTOM = 0,
    DIFFICULTY_EASY = 1,
    DIFFICULTY_MEDIUM = 2,
    DIFFICULTY_HARD = 3,
};

/**
 * @enum SizeIndex
 * @brief Known indices of square Puzzle sizes to the Nonogram server.
 */
enum SizeIndex
{
    SIZE_INDEX_5 = 0,
    SIZE_INDEX_6 = 1,
    SIZE_INDEX_7 = 2,
    SIZE_INDEX_8 = 3,
    SIZE_INDEX_10 = 4,
    SIZE_INDEX_12 = 5,
    SIZE_INDEX_14 = 6,
    SIZE_INDEX_16 = 7,
    SIZE_INDEX_18 = 8,
    SIZE_INDEX_20 = 9,
    SIZE_INDEX_22 = 10,
    SIZE_INDEX_24 = 11,
    SIZE_INDEX_26 = 12,
    SIZE_INDEX_28 = 13,
    SIZE_INDEX_30 = 14,
    SIZE_INDEX_32 = 15,
};

/**
 * @struct Metadata
 * @brief Metadata to characterise a Puzzle specification, modulo the clue data.
 */
struct Metadata
{
    bool valid; /**< @brief Are the other Metadata fields valid? */
    uint32_t seed; /**< @brief The seed of the Puzzle. */
    struct
    {
        enum SizeIndex size_index : 4; /**< @brief The SizeIndex of the Puzzle, */
        enum DifficultyTier tier : 2; /**< @brief The DifficultyTier of the Puzzle. */
    } difficulty; /**< @brief The packed difficulty information, formatted i.a.w. protocol requirements. */
};

/**
 * @brief Prepare the given Metadata object for transmission in the given buffer.
 * @param metadata The Metadata object to serialise.
 * @param buffer_head The destination buffer for the networked-ordered Metadata bytes.
 * @return The advanced buffer head, pointing at the next unwritten byte.
 */
uint8_t * metadata_hton(
    const struct Metadata * metadata,
    uint8_t * buffer_head
);

/**
 * @brief Parse the given payload into the given Metadata structure.
 * @param metadata The destination structure.
 * @param payload The raw bytes in network order.
 * @return Was the payload parsed into the given Metadata management structure?
 */
const uint8_t * metadata_parse(
    struct Metadata * metadata,
    const uint8_t * payload
);

/**
 * @brief Serialise the given Metadata specification to unbuffered serial output.
 * @param metadata The Metadata to pretty-print.
 */
void metadata_print(const struct Metadata * metadata);

/**
 * @brief Compare two Puzzle Metadata objects for commutative equality.
 * @param lhs The first Metadata object.
 * @param rhs The second Metadata object.
 * @return Do the given Metadata objects describe the same Puzzle parameters?
 */
bool metadata_equal(
    const struct Metadata * lhs,
    const struct Metadata * rhs
);

#endif // METADATA_H
