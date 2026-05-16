#include <xil_printf.h>

#include "logging.h"
#include "serial.h"

static char buffer[16];

static unsigned int readline(
    char * const dst,
    const unsigned int max_length
) {
    if (max_length == 0)
        return 0;

    unsigned int idx = 0;

    while (1) {
        const char ch = inbyte();

        switch (ch) {
        case '\r':
        case '\n':
            print("\r\n");
            dst[idx] = '\0';
            return idx;
        case '\b':
        case 0x7f:
            if (idx > 0) {
                --idx;
                print("\b \b");
            }
            continue;
        default:;
        }

        if (ch < 0x20)
            continue;

        if (idx + 1 < max_length) {
            dst[idx++] = ch;
            outbyte(ch);
        } else {
            dst[idx] = '\0';
            return idx;
        }
    }
}

static uint32_t pow_uint32(
    const uint32_t base,
    unsigned int exp
) {
    uint32_t result = 1;

    while (exp--)
        result *= base;

    return result;
}

uint32_t parse_uint32(
    const uint32_t lower_bound,
    const uint32_t upper_bound,
    const uint32_t default_value
) {
    const unsigned int bytes_read = readline(buffer, sizeof(buffer) / sizeof(*buffer));
    uint32_t value = 0;

    if (bytes_read > 0 && buffer[bytes_read] == '\0') {
        for (unsigned int pv_idx = 0; pv_idx < bytes_read; ++pv_idx)
            value += (buffer[pv_idx] - '0') * pow_uint32(10, bytes_read - pv_idx - 1);
        if (value < lower_bound || value > upper_bound) {
            value = default_value;
            logging_puts("Value out of bounds; using default.");
        }
    } else {
        value = default_value;
        logging_puts("Invalid value; using default.");
    }

    return value;
}

enum DifficultyTier parse_difficulty_tier(
    const enum DifficultyTier default_tier
)
{
    const unsigned int bytes_read = readline(buffer, sizeof(buffer) / sizeof(*buffer));

    if (bytes_read == 1)
        switch (*buffer) {
        case 'E':
            return DIFFICULTY_EASY;
        case 'M':
            return DIFFICULTY_MEDIUM;
        case 'H':
            return DIFFICULTY_HARD;
        case 'C':
            return DIFFICULTY_CUSTOM;
        default:
            logging_puts("Invalid selection; using default.");
            return default_tier;
        }

    return default_tier;
}
