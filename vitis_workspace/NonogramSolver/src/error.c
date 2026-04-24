#include <assert.h>
#include <xil_printf.h>
#include <stdlib.h>

#include "error.h"

struct MessageError error_parse(const uint8_t * payload)
{
    assert(*payload == MSG_ERROR);
    payload += sizeof(uint8_t);

    struct MessageError message;
    payload = metadata_parse(&message.metadata, payload);

    if (!message.metadata.valid)
        xil_printf("error_parse: bad metadata.\r\n");

    message.original_msg_id = *payload++;
    message.text_length = *payload++;

    message.error_text = malloc(sizeof(uint8_t) * (message.text_length + 1));
    memcpy(message.error_text, payload, sizeof(uint8_t) * message.text_length);
    message.error_text[message.text_length] = '\0';

    return message;
}

void error_print(const struct MessageError * const message)
{
    xil_printf("MessageError:\r\n\t");
    if (message->metadata.valid)
        metadata_print(&message->metadata);
    else
        xil_printf("Invalid metadata\r\n");

    xil_printf("\tBad Message ID: %d\r\n\tText Length: %d\r\n\tError Text: %s\r\n",
        message->original_msg_id, message->text_length, message->error_text);
}

void error_free(const struct MessageError * const message)
{
    free(message->error_text);
}
