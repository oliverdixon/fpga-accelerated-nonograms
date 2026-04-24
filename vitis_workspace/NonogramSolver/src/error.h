#ifndef ERROR_H
#define ERROR_H

#include <stdint.h>

#include "metadata.h"

struct MessageError
{
    struct PuzzleMetadata metadata;
    uint8_t original_msg_id;
    uint8_t text_length;
    char * error_text;
};

struct MessageError error_parse(const uint8_t * payload);
void error_print(const struct MessageError * message);
void error_free(const struct MessageError * message);

#endif // ERROR_H
