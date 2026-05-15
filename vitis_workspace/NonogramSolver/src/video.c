/**
 * @file
 * @brief HDMI video implementation
 * @date 2026-05-15
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#include <assert.h>
#include <string.h>
#include <xil_cache.h>
#include <xparameters.h>

#include "video.h"
#include "chunks.h"
#include "logging.h"
#include "puzzle.h"

#include "glyph_bitmaps.h"
#include "zybo_z7_hdmi/vga_modes.h"

#define FRAME_STRIDE (1440 * 4)

#define FOREGROUND_COLOUR ((uint32_t)0x00FFFFFF);

static void draw_character(
    const struct VideoState * const state,
    const char item,
    const uint32_t x,
    const uint32_t y
) {
    assert(item >= '0' && item <= '9');

    const uint32_t stride = state->display_ctrl.stride / 4;
    const uint32_t * const glyph = numeric_glyphs[item - '0'];
    uint32_t * const frame = state->display_ctrl.framePtr[state->display_ctrl.curFrame];

    for (unsigned int col_idx = 0; col_idx < glyph_width; ++col_idx) {
        const uint32_t bits = glyph[col_idx];
        for (unsigned int row_idx = 0; row_idx < glyph_height; ++row_idx)
            if (bits & (1U << row_idx))
                frame[(y + row_idx) * stride + (x + col_idx)] = FOREGROUND_COLOUR;
    }
}

static void draw_rectangle(
    const struct VideoState * const state,
    const uint32_t left_x,
    uint32_t top_y,
    const uint32_t width,
    const uint32_t height
) {
    uint32_t * const frame = state->display_ctrl.framePtr[state->display_ctrl.curFrame];

    const uint32_t stride = state->display_ctrl.stride / 4;
    const uint32_t right_x = left_x + width;
    const uint32_t bottom_y = (top_y + height) * stride;
    top_y *= stride;

    // Draw the top and bottom lines.
    for (unsigned int x = left_x; x < right_x; ++x) {
        frame[top_y + x] = FOREGROUND_COLOUR;
        frame[bottom_y + x] = FOREGROUND_COLOUR;
    }

    // Draw the left and right lines.
    for (unsigned int y = top_y; y < bottom_y; y += stride) {
        frame[y + left_x] = FOREGROUND_COLOUR;
        frame[y + right_x] = FOREGROUND_COLOUR;
    }
}

static void draw_filled_rectangle(
    const struct VideoState * const state,
    const uint32_t left_x,
    const uint32_t top_y,
    const uint32_t width,
    const uint32_t height
) {
    uint32_t * const frame = state->display_ctrl.framePtr[state->display_ctrl.curFrame];

    const uint32_t stride = state->display_ctrl.stride / 4;
    const uint32_t right_x = left_x + width;
    const uint32_t bottom_y = (top_y + height) * stride;

    // Draw the filled rectangle.
    for (unsigned int x = left_x; x < right_x; ++x)
        for (unsigned int y = top_y * stride; y < bottom_y; y += stride)
            frame[y + x] = FOREGROUND_COLOUR;
}

static void draw_clue_element(
    const struct VideoState * const state,
    uint8_t clue,
    uint32_t x_pos,
    const uint32_t y_pos,
    const uint32_t glyph_advance_extent
) {
    if (clue > 9)
        x_pos -= glyph_advance_extent / 2;

    while (clue != 0) {
        draw_character(state, (clue % 10) + '0', x_pos, y_pos);
        clue /= 10;
        x_pos += glyph_advance_extent;
    }
}

int video_initialise(
    struct VideoState * video_state
) {
    for (unsigned int fb_idx = 0; fb_idx < DISPLAY_NUM_FRAMES; ++fb_idx)
        video_state->frame_refs[fb_idx] = video_state->frame_buffers[fb_idx];

    int status = DisplayInitialize(
        &video_state->display_ctrl, XPAR_HDMI_AXI_VDMA_0_BASEADDR, XPAR_XVTC_0_BASEADDR,
        XPAR_HDMI_AXI_DYNCLK_0_BASEADDR, video_state->frame_refs, FRAME_STRIDE
    );

    if (status)
        return status;

    status = DisplayChangeFrame(&video_state->display_ctrl, 0);

    if (status)
        return status;

    status = DisplaySetMode(&video_state->display_ctrl, &VMODE_1440x900);

    if (status)
        return status;

    return DisplayStart(&video_state->display_ctrl);
}

void video_draw_puzzle(
    const struct VideoState * const video_state,
    const struct Puzzle * const puzzle_info
) {
    // Blank the entire frame to black.
    memset(
        video_state->display_ctrl.framePtr[video_state->display_ctrl.curFrame], 0x00, MAX_FRAME * 4
    );

    // Draw 50x50 rectangles for each square with a bit of padding.

    /*
     * The padding is determined by the maximum number of clues we need to print.
     * Clues take a maximum of two digits' worth of space (since the maximum grid
     * size is 15x15).
     */
    static const unsigned int max_digits = 2;
    static const unsigned int box_extent = 20;      // Pixel extent of boxes in both directions
    static const unsigned int internal_padding = 5; // Padding between boxes and clues
    static const unsigned int stride = box_extent + internal_padding; // Stride for boxes
    static const unsigned int glyph_spacing = 2; // Spacing between glyphs in the same clue

    const unsigned int external_padding = (glyph_width + internal_padding + glyph_spacing) *
                                          max_digits * puzzle_info->global_max_clue_data_count;

    unsigned int x_pos = external_padding;
    unsigned int y_pos = external_padding;

    unsigned int col_clue_idx = puzzle_info->height;
    unsigned int row_clue_idx = 0;

    const struct ClueData * const clue_data = puzzle_info->chunk.clue_data;

    const bool read_solution_bitmap = puzzle_info->solved_state != SEARCH_NOT_RUN;
    logging_puts("Will write filled cells.");

    /*
     * If the puzzle is solved, we want access to the solution bitmap so it can be
     * visualised as well.
     */
    if (read_solution_bitmap)
        xSemaphoreTake(puzzle_info->solution_semaphore, portMAX_DELAY);

    for (unsigned int col_idx = 0; col_idx < puzzle_info->width; ++col_idx) {
        /*
         * Topmost box on this column, so we might have some column clues.
         * By convention, column clues immediately succeed row clues. There are precisely
         * as many clues as rows/columns, so we just offset the index.
         */
        const struct ClueData * const col_clue = &clue_data[col_clue_idx++];
        const uint32_t start_x = x_pos + (box_extent / 2) - (glyph_width / 2);
        for (unsigned int element_idx = 0; element_idx < col_clue->count; ++element_idx)
            draw_clue_element(
                video_state, col_clue->blocks[element_idx], start_x,
                (element_idx + 1) * (internal_padding + glyph_height), glyph_width + glyph_spacing
            );

        const line_t col_mask = 1U << col_idx;

        for (unsigned int row_idx = 0; row_idx < puzzle_info->height; ++row_idx) {
            if (col_idx == 0) {
                /*
                 * Leftmost box on this row, so we might have some row clues.
                 * By convention, row clues come first, so we don't have to offset the index.
                 */
                const struct ClueData * const row_clue = &clue_data[row_clue_idx++];
                const uint32_t start_y = y_pos + (box_extent / 2) - (glyph_height / 2);
                for (unsigned int element_idx = 0; element_idx < row_clue->count; ++element_idx)
                    draw_clue_element(
                        video_state, row_clue->blocks[element_idx],
                        (element_idx + 1) * (internal_padding + glyph_width), start_y,
                        glyph_width + glyph_spacing
                    );
            }

            if (read_solution_bitmap &&
                (puzzle_info->solution_bitmap[row_idx] & col_mask) == col_mask)
                draw_filled_rectangle(video_state, x_pos, y_pos, box_extent, box_extent);
            else
                draw_rectangle(video_state, x_pos, y_pos, box_extent, box_extent);
            y_pos += stride;
        }

        x_pos += stride;
        y_pos = external_padding;
    }

    if (read_solution_bitmap)
        xSemaphoreGive(puzzle_info->solution_semaphore);

    Xil_DCacheFlush();
}
