#ifndef SERIAL_H
#define SERIAL_H

#include "metadata.h"

uint32_t parse_uint32(const uint32_t lower_bound, const uint32_t upper_bound,
    const uint32_t default_value);

enum DifficultyTier parse_difficulty_tier(const enum DifficultyTier default_tier);

#endif // SERIAL_H
