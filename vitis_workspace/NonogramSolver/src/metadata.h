// clang-format Language: C

#ifndef PUZZLE_METADATA_H
#define PUZZLE_METADATA_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Seed (4 bytes)
 * Difficulty (1 byte)
 */
#define MESSAGE_METADATA_LENGTH (4 + 1)

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

enum DifficultyTier
{
    DIFFICULTY_CUSTOM = 0,
    DIFFICULTY_EASY = 1,
    DIFFICULTY_MEDIUM = 2,
    DIFFICULTY_HARD = 3,
};

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
};

struct Metadata
{
    bool valid;
    uint32_t seed;
    struct
    {
        enum SizeIndex size_index : 4;
        enum DifficultyTier tier : 2;
    } difficulty;
};

uint8_t * metadata_hton(
    const struct Metadata * data,
    uint8_t * buffer_head
);
const uint8_t * metadata_parse(
    struct Metadata * metadata,
    const uint8_t * payload
);
void metadata_print(const struct Metadata * metadata);
bool metadata_equal(
    const struct Metadata * lhs,
    const struct Metadata * rhs
);

#endif // PUZZLE_METADATA_H
