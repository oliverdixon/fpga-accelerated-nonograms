// clang-format Language: C

#ifndef RESULT_H
#define RESULT_H

#include <stdint.h>

struct Metadata;
struct Puzzle;
struct sockaddr_in;

enum ResultCode
{
    RESULT_INCORRECT = 0x00,
    RESULT_CORRECT = 0x01,
    RESULT_ERROR = 0x02
};

struct Result
{
    enum ResultCode status : 8;
    uint32_t solve_time;
};

int result_parse(
    struct Result * result,
    const struct Metadata * metadata,
    const uint8_t * payload
);

int result_send(
    const struct Puzzle * puzzle,
    int sock,
    const struct sockaddr_in * dst_addr
);

void result_print(const struct Result * result);

#endif // RESULT_H
