#ifndef MESSAGE_PUZZLE_INFO_H
#define MESSAGE_PUZZLE_INFO_H

#include <lwip/sockets.h>

#include "metadata.h"
#include "chunks.h"

/*
 * Message ID (1 byte)
 * Metadata
 * Width (1 byte)
 * Height (1 byte)
 * Number of chunks (1 byte)
 * Total size of the clue data (2 bytes)
 */
#define MESSAGE_PUZZLE_INFO_LENGTH (1 + MESSAGE_METADATA_LENGTH + 1 + 1 + 1 + 2)

/*
 * Message ID (1 byte)
 * Metadata
 */
#define MESSAGE_REQUEST_INFO_LENGTH (1 + MESSAGE_METADATA_LENGTH)

struct MessagePuzzleInfo
{
	struct PuzzleMetadata metadata;
	uint8_t width;
	uint8_t height;
	uint8_t num_chunks;
	uint16_t clue_bytes;
	
	unsigned int global_max_clue_data_count;
	struct MessageChunkData chunk; // TODO: support multiple chunks
};

struct MessageRequestInfo
{
	struct PuzzleMetadata metadata;
};

void puzzle_request(const struct MessageRequestInfo * data,
	int sock, const struct sockaddr_in * dst_addr);

int puzzle_parse(struct MessagePuzzleInfo * dst, const uint8_t * payload);
void puzzle_print(const struct MessagePuzzleInfo * puzzle_info);
void puzzle_request_print(const struct MessageRequestInfo * request_info);

#endif // MESSAGE_PUZZLE_INFO_H
