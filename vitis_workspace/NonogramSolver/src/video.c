/**
 * Example of using the Digilent display drivers for Zybo Z7 HDMI output
 * Russell Joyce, 11/03/2019
 */

#include <stdio.h>
#include <assert.h>
#include "xil_types.h"
#include "xil_cache.h"
#include "xparameters.h"

#include "video.h"
#include "puzzle.h"
#include "chunks.h"
#include "zybo_z7_hdmi/display_ctrl.h"
#include "zybo_z7_hdmi/vga_modes.h"

#define FRAME_STRIDE (1440*4)

static const unsigned int glyph_width = 15;
static const unsigned int glyph_height = 21;

static const uint32_t numeric_glyphs[][15] = {
    // 0
    {
        0x07FFF8, 0x07FFF8, 0x07FFF8,
        0x3800E7, 0x3800E7, 0x3800E7,
        0x3838E7, 0x3838E7, 0x3838E7,
        0x38E0E7, 0x38E0E7, 0x38E0E7,
        0x07FFF8, 0x07FFF8, 0x07FFF8
    },

    // 1
    {
        0x000000, 0x000000, 0x000000,
        0x3801C0, 0x3801C0, 0x3801C0,
        0x3FFFFF, 0x3FFFFF, 0x3FFFFF,
        0x380000, 0x380000, 0x380000,
        0x000000, 0x000000, 0x000000
    },

    // 2
    {
        0x3801C0, 0x3801C0, 0x3801C0,
        0x3F8007, 0x3F8007, 0x3F8007,
        0x3870E7, 0x3870E7, 0x3870E7,
        0x380E07, 0x380E07, 0x380E07,
        0x3801F8, 0x3801F8, 0x3801F8
    },

    // 3
    {
        0x070007, 0x070007, 0x070007,
        0x380007, 0x380007, 0x380007,
        0x3800E7, 0x3800E7, 0x3800E7,
        0x380F3F, 0x380F3F, 0x380F3F,
        0x07F807, 0x07F807, 0x07F807
    },

    // 4
    {
        0x00FF00, 0x00FF00, 0x00FF00,
        0x00E0E0, 0x00E0E0, 0x00E0E0,
        0x00E01C, 0x00E01C, 0x00E01C,
        0x3FFFFF, 0x3FFFFF, 0x3FFFFF,
        0x00E000, 0x00E000, 0x00E000
    },

    // 5
    {
        0x0701FF, 0x0701FF, 0x0701FF,
        0x3800E7, 0x3800E7, 0x3800E7,
        0x3800E7, 0x3800E7, 0x3800E7,
        0x3800E7, 0x3800E7, 0x3800E7,
        0x07FF07, 0x07FF07, 0x07FF07
    },

    // 6
    {
        0x07FFF0, 0x07FFF0, 0x07FFF0,
        0x380E38, 0x380E38, 0x380E38,
        0x380E07, 0x380E07, 0x380E07,
        0x380E07, 0x380E07, 0x380E07,
        0x07F000, 0x07F000, 0x07F000
    },

    // 7
    {
        0x000007, 0x000007, 0x000007,
        0x3FF807, 0x3FF807, 0x3FF807,
        0x001C07, 0x001C07, 0x001C07,
        0x0000E7, 0x0000E7, 0x0000E7,
        0x00001F, 0x00001F, 0x00001F
    },

    // 8
    {
        0x07F9F8, 0x07F9F8, 0x07F9F8,
        0x380E07, 0x380E07, 0x380E07,
        0x380E07, 0x380E07, 0x380E07,
        0x380E07, 0x380E07, 0x380E07,
        0x07F9F8, 0x07F9F8, 0x07F9F8
    },

    // 9
    {
        0x0001F8, 0x0001F8, 0x0001F8,
        0x380E07, 0x380E07, 0x380E07,
        0x380E07, 0x380E07, 0x380E07,
        0x0E0E07, 0x0E0E07, 0x0E0E07,
        0x01FFF8, 0x01FFF8, 0x01FFF8
    }
};

static const uint32_t foreground_colour = 0x00FFFFFF;

static void draw_character(const struct VideoState * const state,
		const char item, const uint32_t x, const uint32_t y)
{
	assert(item >= '0' && item <= '9');

	const uint32_t stride = state->display_ctrl.stride / 4;
	const uint32_t * const glyph = numeric_glyphs[item - '0'];
	uint32_t * const frame = state->display_ctrl.framePtr[state->
		display_ctrl.curFrame];

	for (unsigned int col_idx = 0; col_idx < glyph_width; ++col_idx) {
		const uint32_t bits = glyph[col_idx];
		for (unsigned int row_idx = 0; row_idx < glyph_height; ++row_idx)
			if (bits & (1U << row_idx))
				frame[(y + row_idx) * stride + (x + col_idx)] = foreground_colour;
	}
}

static void draw_rectangle(const struct VideoState * const state,
		const uint32_t left_x, const uint32_t top_y,
		const uint32_t width, const uint32_t height)
{
	uint32_t * const frame = state->display_ctrl.framePtr[state->
		display_ctrl.curFrame];

	const uint32_t stride = state->display_ctrl.stride / 4;
	const uint32_t right_x = left_x + width;
	const uint32_t bottom_y = top_y + height;

	// Draw the top and bottom lines.
	for (unsigned int x = left_x; x < right_x; ++x) {
		frame[top_y * stride + x] = foreground_colour;
		frame[bottom_y * stride + x] = foreground_colour;
	}

	// Draw the left and right lines.
	for (unsigned int y = top_y; y < bottom_y; ++y) {
		frame[y * stride + left_x] = foreground_colour;
		frame[y * stride + right_x] = foreground_colour;
	}
}

void video_initialise(struct VideoState * video_state)
{
	for (unsigned int fb_idx = 0; fb_idx < DISPLAY_NUM_FRAMES; ++fb_idx)
		video_state->frame_refs[fb_idx] = video_state->frame_buffers[fb_idx];

	DisplayInitialize(&video_state->display_ctrl, XPAR_HDMI_AXI_VDMA_0_BASEADDR,
		XPAR_XVTC_0_BASEADDR, XPAR_HDMI_AXI_DYNCLK_0_BASEADDR, video_state->frame_refs,
		FRAME_STRIDE);

	DisplayChangeFrame(&video_state->display_ctrl, 0);
	DisplaySetMode(&video_state->display_ctrl, &VMODE_1440x900);
	DisplayStart(&video_state->display_ctrl);
}

void video_draw_puzzle(const struct VideoState * video_state,
    	const struct MessageChunkData * chunk_data,
	    const struct MessagePuzzleInfo * puzzle_info)
{
	// Blank the entire frame to black.
	memset(video_state->display_ctrl.framePtr[video_state->
		display_ctrl.curFrame], 0x00, MAX_FRAME * 4);

	// Draw 50x50 rectangles for each square with a bit of padding.

	/*
	 * The padding is determined by the maximum number of clues we need to print.
	 * Clues take a maximum of two digits' worth of space (since the maximum grid
	 * size is 15x15).
	 */
	const unsigned int box_extent = 50;
	const unsigned int internal_padding = 10;
	const unsigned int stride = box_extent + internal_padding;
	const unsigned int external_padding = (glyph_width + internal_padding) * 2 *
		chunk_data->max_clue_data_count;

	unsigned int x_pos = external_padding;
	unsigned int y_pos = external_padding;

	unsigned int clue_idx = 0;

	for (unsigned int x_idx = 0; x_idx < puzzle_info->width; ++x_idx) {
		for (unsigned int y_idx = 0; y_idx < puzzle_info->height; ++y_idx) {
			if (x_idx == 0) {
				// Leftmost box on this row, so we might have some row clues.
				const struct ClueData * const clue = &chunk_data->clue_data[clue_idx++];
				const unsigned int element_count = clue->count;
				for (unsigned int element_idx = 0; element_idx < element_count; ++element_idx)
					draw_character(video_state, clue->blocks[element_idx] + '0', internal_padding, y_pos);
			}

			draw_rectangle(video_state, x_pos, y_pos, box_extent, box_extent);
			y_pos += stride;
		}

		x_pos += stride;
		y_pos = external_padding;
	}

	Xil_DCacheFlush();
}
