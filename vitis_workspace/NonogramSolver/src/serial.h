// clang-format Language: C

/**
 * @file
 * @brief Simple text-parsing interface
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#ifndef SERIAL_H
#define SERIAL_H

#include "metadata.h"

/**
 * @brief Read an unsigned 32-bit integer from the serial line.
 * @param lower_bound The inclusive lower bound.
 * @param upper_bound The inclusive upper bound.
 * @param default_value The default value to return should the interpreted value be invalid or out-of-bounds.
 * @return The read number, or the given default.
 */
uint32_t parse_uint32(
    uint32_t lower_bound,
    uint32_t upper_bound,
    uint32_t default_value
);

/**
 * @brief Parse a difficulty-tier specification from the serial line.
 * @param default_tier The default tier to return should the interpreted value be invalid.
 * @return The read tier, or the given default.
 */
enum DifficultyTier parse_difficulty_tier(enum DifficultyTier default_tier);

#endif // SERIAL_H
