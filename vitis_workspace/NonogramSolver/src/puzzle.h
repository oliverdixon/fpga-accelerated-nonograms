#ifndef MESSAGE_PUZZLE_INFO_H
#define MESSAGE_PUZZLE_INFO_H

#include <lwip/udp.h>
#include <lwip/ip.h>

#include "metadata.h"

struct MessagePuzzleInfo
{
	struct PuzzleMetadata metadata;
	uint8_t width;
	uint8_t height;
	uint8_t num_chunks;
	uint16_t clue_bytes;
};

struct MessageRequestInfo
{
	struct PuzzleMetadata metadata;
};

void puzzle_request(const struct MessageRequestInfo * data, struct udp_pcb * pcb,
	const ip_addr_t * dst_ip, uint16_t dst_port);

struct MessagePuzzleInfo puzzle_parse(const uint8_t * payload);
void puzzle_print(const struct MessagePuzzleInfo * puzzle_info);

#endif // MESSAGE_PUZZLE_INFO_H
