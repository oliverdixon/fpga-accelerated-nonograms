#include <assert.h>
#include <lwip/def.h>
#include <xil_printf.h>

#include "metadata.h"

uint8_t * metadata_hton(const struct PuzzleMetadata * const data, uint8_t * buffer_head)
{
    // Reorder seed to network order
    const uint32_t net_seed = htonl(data->seed);
    memcpy(buffer_head, &net_seed, sizeof(uint32_t));
    buffer_head += sizeof(uint32_t);

    // Append difficulty byte
    memcpy(buffer_head, &data->difficulty, sizeof(uint8_t));

    return buffer_head + sizeof(uint8_t);
}

const uint8_t * metadata_parse(struct PuzzleMetadata * const metadata, const uint8_t * const payload)
{
    const uint8_t difficulty = *(payload + sizeof(uint32_t));
    bool valid = true;

    metadata->seed = ntohl(*(const uint32_t *) payload);
    metadata->difficulty.size_index = difficulty & 0x0F;
    metadata->difficulty.tier = (difficulty >> 4) & 0x03;

    if (metadata->difficulty.tier > DIFFICULTY_HARD) {
        xil_printf("metadata_parse: Invalid difficulty tier %02x.\r\n", metadata->difficulty.tier);
        valid = false;
    }
    
    if (metadata->difficulty.size_index >= SIZE_INDEX_MAX) {
        xil_printf("metadata_parse: Invalid size index %02x.\r\n", metadata->difficulty.size_index);
        valid = false;
    }

    metadata->valid = valid;
    return payload + sizeof(uint32_t) + sizeof(uint8_t);
}

void metadata_print(const struct PuzzleMetadata * const metadata)
{
    assert(metadata->valid);
    xil_printf("Seed: %04x\r\n\tSize Index: %d\r\n\tDifficulty Tier: %d\r\n",
        metadata->seed, metadata->difficulty.size_index, metadata->difficulty.tier);
}
