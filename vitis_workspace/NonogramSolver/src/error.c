#include <assert.h>
#include <xil_printf.h>
#include <stdlib.h>

#include "error.h"

int error_parse(struct MessageError * dst, const uint8_t * payload)
{
    assert(*payload == MSG_ERROR);
    payload += sizeof(uint8_t);

    payload = metadata_parse(&dst->metadata, payload);

    if (!dst->metadata.valid)
        // Protocol spec states that metadata is optional in errors.
        print("error_parse: bad metadata.\r\n");

    dst->original_msg_id = *payload++;
    dst->text_length = *payload++;

    if (dst->text_length > ERROR_MAX_LENGTH) {
        print("error_parse: message too long.\r\n");
        return -1;
    }

    memcpy(dst->error_text, payload, sizeof(uint8_t) * dst->text_length);
    dst->error_text[dst->text_length] = '\0';
    
    return 0;
}

void error_print(const struct MessageError * const message)
{
    print("\r\nMessageError:\r\n\t");
    if (message->metadata.valid)
        metadata_print(&message->metadata);
    else
        print("Invalid metadata\r\n");

    xil_printf("\tBad Message ID: %d\r\n\tText Length: %d\r\n\tError Text: %s\r\n\r\n",
        message->original_msg_id, message->text_length, message->error_text);
}
