#include <assert.h>
#include <lwip/def.h>
#include <xil_printf.h>

#include "logging.h"
#include "metadata.h"

uint8_t * metadata_hton(
    const struct Metadata * const data,
    uint8_t * buffer_head
) {
    // Reorder seed to network order
    const uint32_t net_seed = htonl(data->seed);
    memcpy(buffer_head, &net_seed, sizeof(uint32_t));
    buffer_head += sizeof(uint32_t);

    // Append difficulty byte
    *buffer_head = data->difficulty.size_index | (data->difficulty.tier << 4);

    return buffer_head + sizeof(uint8_t);
}

const uint8_t * metadata_parse(
    struct Metadata * const metadata,
    const uint8_t * const payload
) {
    const uint8_t difficulty = *(payload + sizeof(uint32_t));
    bool valid = true;

    metadata->seed = ntohl(*(const uint32_t *)payload);
    metadata->difficulty.size_index = difficulty & 0x0F;
    metadata->difficulty.tier = (difficulty >> 4) & 0x03;

    if (metadata->difficulty.tier > DIFFICULTY_HARD) {
        logging_printf("metadata_parse: Invalid difficulty tier %02x.", metadata->difficulty.tier);
        valid = false;
    }

    if (metadata->difficulty.size_index > SIZE_INDEX_20) {
        logging_printf("metadata_parse: Invalid size index %02x.", metadata->difficulty.size_index);
        valid = false;
    }

    metadata->valid = valid;
    return payload + sizeof(uint32_t) + sizeof(uint8_t);
}

void metadata_print(
    const struct Metadata * const metadata
) {
    assert(metadata->valid);
    xil_printf(
        "Seed: %04x\r\n\tSize Index: %d\r\n\tDifficulty Tier: %d\r\n", metadata->seed,
        metadata->difficulty.size_index, metadata->difficulty.tier
    );
}

bool metadata_equal(
    const struct Metadata * const lhs,
    const struct Metadata * const rhs
) {
    if (lhs == NULL) {
        if (rhs == NULL)
            return true;
        else
            return false;
    }

    if (rhs == NULL) {
        if (lhs == NULL)
            return true;
        else
            return false;
    }

    return lhs->valid == rhs->valid && lhs->seed == rhs->seed &&
           lhs->difficulty.size_index == rhs->difficulty.size_index &&
           lhs->difficulty.tier == rhs->difficulty.tier;
}
