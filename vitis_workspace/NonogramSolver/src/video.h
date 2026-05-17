// clang-format Language: C

/**
 * @file
 * @brief HDMI video interface
 * @date 2026-05-15
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#ifndef VIDEO_H
#define VIDEO_H

#include <stdbool.h>
#include <stdint.h>

#include "zybo_z7_hdmi/display_ctrl.h"

#define MAX_FRAME (1440 * 900) /**< @brief Maximum frame size, in pixels. */

/**
 * @struct VideoState
 * @brief Collects the double-buffered HDMI state
 */
struct VideoState
{
    DisplayCtrl display_ctrl; /**< @brief Management block for the HDMI. */
    uint32_t frame_buffers[DISPLAY_NUM_FRAMES][MAX_FRAME] __attribute__((aligned(0x20))); /**< @brief Frame data. */
    void * frame_refs[DISPLAY_NUM_FRAMES]; /**< @brief References to each frame buffer. */
};

struct Chunk;
struct Puzzle;

/**
 * @brief Initialises the HDMI video controller with the standard resolution into the given
 * VideoState.
 * @param video_state The destination HDMI state.
 * @return 0 on success, -1 on failure.
 */
bool video_initialise(struct VideoState * video_state);

/**
 * @brief Renders the given puzzle grid and clue data (and solution grid, if applicable) to the
 * initialised VideoState.
 * @param video_state The initialised VideoState structure.
 * @param puzzle_info The Puzzle to render on-screen.
 */
void video_draw_puzzle(
    const struct VideoState * video_state,
    const struct Puzzle * puzzle_info
);

#endif // VIDEO_H
