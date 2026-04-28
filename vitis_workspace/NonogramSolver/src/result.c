#include <assert.h>
#include <xil_printf.h>

#include "result.h"

int result_parse(struct MessageResult * const result, const uint8_t * payload)
{
    assert(*payload == MSG_PUZZLE_INFO);
    payload += sizeof(uint8_t);

    payload = metadata_parse(&result->metadata, payload);

    if (!result->metadata.valid) {
        print("result_parse: quitting early due to bad metadata.\r\n");
        return -1;
    }

    result->status = *payload++;
    
    result->solve_time = *payload++ << 8;
    result->solve_time |= *payload++ << 8;
    result->solve_time |= *payload++ << 8;
    result->solve_time |= *payload;

    return 0;
}

void result_print(struct MessageResult * result)
{
    print("\r\n");
    
    if (result->metadata.valid) {
        print("MessageResult:\r\n\t");
        metadata_print(&result->metadata);
        xil_printf("\tStatus: %d\r\n\tSolve Time: %d\r\n", result->status, result->solve_time);
    } else
        print("MessageResult: INVALID\r\n");

    print("\r\n");
}
