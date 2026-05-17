/**
 * @file
 * @brief HDMI video implementation
 * @date 2026-05-15
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#include <assert.h>
#include <string.h>
#include <xil_cache.h>

#include "video.h"
#include "chunks.h"
#include "glyph_bitmaps.h"
#include "puzzle.h"
#include "zybo_z7_hdmi/vga_modes.h"

#define FRAME_STRIDE (1440 * 4) /**< @brief Number of pixels to stride for each scanline. */
#define FOREGROUND_COLOUR ((uint32_t)0x00FFFFFF) /**< @brief Foreground colour for drawing. */
#define BACKGROUND_COLOUR ((uint32_t)0x00000000) /**< @brief Background colour for drawing. */

/**
 * @brief Draw the glyph corresponding to the given item at the specified position.
 * @param state The VideoState management block.
 * @param item The character to render in Base 36.
 * @param left_x The leftmost position, in pixels.
 * @param top_y The topmost position, in pixels.
 * @note If the given character cannot be rendered, a placeholder fallback glyph is produced in its place.
 */
static void draw_character(
    const struct VideoState * const state,
    const char item,
    const uint32_t left_x,
    const uint32_t top_y
) {
    const uint32_t stride = state->display_ctrl.stride / 4;
    const glyph_word_t * const glyph = glyph_data[item < GLYPH_COUNT ? item : GLYPH_COUNT - 1];
    uint32_t * const frame = state->display_ctrl.framePtr[state->display_ctrl.curFrame];

    for (unsigned int col_idx = 0; col_idx < GLYPH_WIDTH; ++col_idx) {
        const glyph_word_t bits = glyph[col_idx];
        for (unsigned int row_idx = 0; row_idx < GLYPH_HEIGHT; ++row_idx)
            if (bits & (glyph_word_t)1U << row_idx)
                frame[(top_y + row_idx) * stride + (left_x + col_idx)] = FOREGROUND_COLOUR;
    }
}

/**
 * @brief Draw an unfilled rectangle of the specified size at the specified position.
 * @param state The VideoState management block.
 * @param left_x The leftmost position, in pixels.
 * @param top_y The topmost position, in pixels.
 * @param width The width of the rectangle, in pixels.
 * @param height The height of the rectangle, in pixels.
 */
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

/**
 * @brief Draw a filled rectangle of the specified size at the specified position.
 * @param state The VideoState management block.
 * @param left_x The leftmost position, in pixels.
 * @param top_y The topmost position, in pixels.
 * @param width The width of the rectangle, in pixels.
 * @param height The height of the rectangle, in pixels.
 */
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

bool video_initialise(
    struct VideoState * const video_state
) {
    for (unsigned int fb_idx = 0; fb_idx < DISPLAY_NUM_FRAMES; ++fb_idx)
        video_state->frame_refs[fb_idx] = video_state->frame_buffers[fb_idx];

    if (DisplayInitialize(&video_state->display_ctrl, XPAR_HDMI_AXI_VDMA_0_BASEADDR, XPAR_XVTC_0_BASEADDR,
            XPAR_HDMI_AXI_DYNCLK_0_BASEADDR, video_state->frame_refs, FRAME_STRIDE) != 0)
        return false;

    if (DisplayChangeFrame(&video_state->display_ctrl, 0) != 0)
        return false;

    if (DisplaySetMode(&video_state->display_ctrl, &VMODE_1440x900) != 0)
        return false;

    if (DisplayStart(&video_state->display_ctrl) != 0)
        return false;

    return true;
}

void video_draw_puzzle(
    const struct VideoState * const video_state,
    const struct Puzzle * const puzzle_info
) {
    // Blank the entire frame.
    memset(video_state->display_ctrl.framePtr[video_state->display_ctrl.curFrame], BACKGROUND_COLOUR, MAX_FRAME * 4);

    /*
     * Draw 50x50 rectangles for each square with a bit of padding.
     *
     * The padding is determined by the maximum number of clues we need to print.
     * Clues take a maximum of two digits' worth of space (since the maximum grid
     * size is 15x15).
     */
    static const unsigned int box_extent = 20; // Pixel extent of boxes in both directions
    static const unsigned int internal_padding = 5; // Padding between boxes and clues
    static const unsigned int stride = box_extent + internal_padding; // Stride for boxes

    // External padding, i.e. the pixel skip in both directions before the grid starts.
    const unsigned int external_padding = (GLYPH_EXTENT + internal_padding) *
        (puzzle_info->global_max_clue_data_count + 1);

    unsigned int x_pos = external_padding;
    unsigned int y_pos = external_padding;

    unsigned int col_clue_idx = puzzle_info->height;
    unsigned int row_clue_idx = 0;

    const struct ClueGroup * const clue_data = puzzle_info->chunk.clue_data;

    const bool read_solution_bitmap = puzzle_info->solved_state == SEARCH_SOLVED;

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
        const struct ClueGroup * const col_clue = &clue_data[col_clue_idx++];
        const uint32_t start_x = x_pos + box_extent / 2 - GLYPH_WIDTH / 2;
        for (unsigned int element_idx = 0; element_idx < col_clue->count; ++element_idx)
            draw_character(video_state, col_clue->clues[element_idx], start_x, (element_idx + 1) *
                (internal_padding + GLYPH_HEIGHT));

        const line_t col_mask = 1U << col_idx;

        for (unsigned int row_idx = 0; row_idx < puzzle_info->height; ++row_idx) {
            if (col_idx == 0) {
                /*
                 * Leftmost box on this row, so we might have some row clues.
                 * By convention, row clues come first, so we don't have to offset the index.
                 */
                const struct ClueGroup * const row_clue = &clue_data[row_clue_idx++];
                const uint32_t start_y = y_pos + box_extent / 2 - GLYPH_HEIGHT / 2;
                for (unsigned int element_idx = 0; element_idx < row_clue->count; ++element_idx)
                    draw_character(video_state, row_clue->clues[element_idx], (element_idx + 1) *
                        (internal_padding + GLYPH_WIDTH), start_y);
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
