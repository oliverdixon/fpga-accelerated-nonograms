#ifndef MESSAGE_PUZZLE_INFO_H
#define MESSAGE_PUZZLE_INFO_H

#include <lwip/sockets.h>

#include "metadata.h"

struct MessagePuzzleInfo
{
	struct PuzzleMetadata metadata;
	uint8_t width;
	uint8_t height;
	uint8_t num_chunks;
	uint16_t clue_bytes;
	unsigned int global_max_clue_data_count;
};

struct MessageRequestInfo
{
	struct PuzzleMetadata metadata;
};

void puzzle_request(const struct MessageRequestInfo * data,
	int sock, const struct sockaddr_in * dst_addr);

int puzzle_parse(struct MessagePuzzleInfo * dst, const uint8_t * payload);
void puzzle_print(const struct MessagePuzzleInfo * puzzle_info);

#endif // MESSAGE_PUZZLE_INFO_H
