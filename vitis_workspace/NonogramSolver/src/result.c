#include <assert.h>
#include <xil_printf.h>

#include "metadata.h"
#include "result.h"

int result_parse(struct MessageResult * result, const struct PuzzleMetadata * const metadata, const uint8_t * payload)
{
    assert(*payload == MSG_PUZZLE_INFO);
    payload += sizeof(uint8_t);

    // Verify that the received metadata matches what we expect.
    struct PuzzleMetadata received_metadata;
    payload = metadata_parse(&received_metadata, payload);

    if (!received_metadata.valid || !metadata_equal(metadata, &received_metadata)) {
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

void result_print(const struct MessageResult * const result)
{
    xil_printf("Result in %d seconds: ", result->solve_time);
    
    switch (result->status) {
    case RESULT_INCORRECT:
        print("Incorrect!\r\n");
        break;
    case RESULT_CORRECT:
        print("Correct!\r\n");
        break;
    case RESULT_ERROR:
        print("Error!\r\n");
        break;
    }
}
