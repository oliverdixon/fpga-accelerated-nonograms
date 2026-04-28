#ifndef RESULT_H
#define RESULT_H

#include "metadata.h"

enum ResultCode
{
    RESULT_INCORRECT = 0x00,
    RESULT_CORRECT = 0x01,
    RESULT_ERROR = 0x02
};

struct MessageResult
{
    struct PuzzleMetadata metadata;
    enum ResultCode status : 8;
    uint32_t solve_time;   
};

int result_parse(struct MessageResult * result, const uint8_t * payload);
void result_print(struct MessageResult * result);

#endif // RESULT_H
