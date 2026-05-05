// clang-format Language: C

#ifndef ERROR_H
#define ERROR_H

#include "metadata.h"

#define ERROR_MAX_LENGTH (200)

struct ServerError
{
    struct Metadata metadata;
    uint8_t original_msg_id;
    uint8_t text_length;
    char error_text[ERROR_MAX_LENGTH + 1];
};

int error_parse(
    struct ServerError * dst,
    const uint8_t * payload
);
void error_print(const struct ServerError * message);

#endif // ERROR_H
