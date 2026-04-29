#ifndef VIDEO_H
#define VIDEO_H

#include "zybo_z7_hdmi/display_ctrl.h"

#define MAX_FRAME (1440*900)

struct VideoState
{
    DisplayCtrl display_ctrl;
    uint32_t frame_buffers[DISPLAY_NUM_FRAMES][MAX_FRAME] __attribute__((aligned(0x20)));
    void * frame_refs[DISPLAY_NUM_FRAMES];
};

struct Chunk;
struct Puzzle;

void video_initialise(struct VideoState * video_state);

void video_draw_puzzle(const struct VideoState * video_state,
    const struct Puzzle * puzzle_info);

#endif // VIDEO_H
