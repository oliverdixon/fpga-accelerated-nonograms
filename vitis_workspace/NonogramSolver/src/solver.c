#include <assert.h>
#include <xil_cache.h>
#include <xil_printf.h>
#include <xsolver_toplevel.h>

#include "chunks.h"
#include "solver.h"
#include "puzzle.h"

typedef uint32_t line_t;
typedef uint8_t extent_t;

#define MAX_SIZE (20)
#define MAX_PATTERN_COUNT (256)

static line_t row_patterns[MAX_SIZE * MAX_PATTERN_COUNT];
static line_t col_patterns[MAX_SIZE * MAX_PATTERN_COUNT];
static extent_t row_counts[MAX_SIZE];
static extent_t col_counts[MAX_SIZE];

static line_t out_black[MAX_SIZE];
static line_t out_white[MAX_SIZE];

static uint32_t run_hls_core(XSolver_toplevel * const solver, const struct MessagePuzzleInfo * const puzzle_info)
{
    XSolver_toplevel_Set_row_patterns(solver, (UINTPTR)&row_patterns);
    XSolver_toplevel_Set_row_counts(solver, (UINTPTR)&row_counts);
    XSolver_toplevel_Set_col_patterns(solver, (UINTPTR)&col_patterns);
    XSolver_toplevel_Set_col_counts(solver, (UINTPTR)&col_counts);
    XSolver_toplevel_Set_puzzle_size(solver, puzzle_info->width);
    XSolver_toplevel_Set_out_black(solver, (UINTPTR)&out_black);
    XSolver_toplevel_Set_out_white(solver, (UINTPTR)&out_white);

    Xil_DCacheFlushRange((UINTPTR)&row_patterns, sizeof(row_patterns));
    Xil_DCacheFlushRange((UINTPTR)&row_counts, sizeof(row_counts));
    Xil_DCacheFlushRange((UINTPTR)&col_patterns, sizeof(col_patterns));
    Xil_DCacheFlushRange((UINTPTR)&col_counts, sizeof(col_counts));
    Xil_DCacheFlushRange((UINTPTR)&puzzle_info->width, sizeof(puzzle_info->width));
    Xil_DCacheFlushRange((UINTPTR)&out_black, sizeof(out_black));
    Xil_DCacheFlushRange((UINTPTR)&out_white, sizeof(out_white));
    
    XSolver_toplevel_Start(solver);
    while (!XSolver_toplevel_IsDone(solver));

    Xil_DCacheInvalidateRange((UINTPTR)&out_white, sizeof(out_white));
    Xil_DCacheInvalidateRange((UINTPTR)&out_black, sizeof(out_black));

    return XSolver_toplevel_Get_return(solver);
}

static unsigned int min_required_tail(const struct ClueData * const block, const unsigned int start_idx)
{
    if (start_idx >= block->count)
        return 0;

    unsigned int sum = 0;
    for (unsigned int idx = start_idx; idx < block->count; ++idx)
        sum += block->blocks[idx];

    sum += block->count - start_idx - 1;
    return sum;
}

static extent_t generate_pattern_induction(
        const extent_t puzzle_size,
        const struct ClueData * const block,
        const unsigned int clue_idx,
        const unsigned int min_start_idx,
        const line_t partial_line,
        line_t * const pattern_out,
        extent_t next_pattern_idx)
{
    if (clue_idx == block->count) {
        // Base case: we've reached the end of the clues, so commit our current line.
        pattern_out[next_pattern_idx++] = partial_line;
        return next_pattern_idx;
    }

    const unsigned int block_len = block->blocks[clue_idx]; // The length of the target continuous block.
    const unsigned int latest_start_idx = puzzle_size - block_len - min_required_tail(block, clue_idx + 1);

    for (unsigned int start_idx = min_start_idx; start_idx <= latest_start_idx; ++start_idx) {
        const line_t block_mask = ((1U << block_len) - 1) << start_idx;
        const unsigned next_idx = clue_idx + 1 == block->count ? start_idx + block_len : start_idx + block_len + 1;

        next_pattern_idx = generate_pattern_induction(
            puzzle_size,
            block,
            clue_idx + 1,
            next_idx,
            partial_line | block_mask,
            pattern_out,
            next_pattern_idx
        );
    }

    return next_pattern_idx;
}

static extent_t generate_pattern(
        line_t dst[MAX_SIZE],
        const extent_t puzzle_size,
        struct ClueData * const block)
{
    if (block->count == 0) {
        dst[0] = 0;
        return 1;
    }

    return generate_pattern_induction(puzzle_size, block, 0, 0, 0, dst, 0);
}

static void compute_valid_patterns(
        const extent_t puzzle_size,
        line_t dst[MAX_SIZE * MAX_PATTERN_COUNT],
        extent_t counts[MAX_SIZE],
        struct ClueData * const clues,
        const unsigned int clue_count)
{
    for (unsigned int clue_idx = 0; clue_idx < clue_count; ++clue_idx)
        counts[clue_idx] = generate_pattern(&dst[clue_idx * MAX_PATTERN_COUNT], puzzle_size, &clues[clue_idx]);
}

static void print_board(
        const line_t black[MAX_SIZE],
        const line_t white[MAX_SIZE],
        const extent_t puzzle_size)
{
    for (extent_t row_idx = 0; row_idx < puzzle_size; ++row_idx) {
        for (extent_t col_idx = 0; col_idx < puzzle_size; ++col_idx) {
            const line_t mask = 1U << col_idx;

            if (black[row_idx] & mask)
                outbyte('#');
            else if (white[row_idx] & mask)
                outbyte('.');
            else
                outbyte('?');
        }

        print("\r\n");
    }
}

void solver_solve(XSolver_toplevel * const solver, const struct MessagePuzzleInfo * const puzzle_info)
{
    assert(puzzle_info->width == puzzle_info->height);
    assert(puzzle_info->chunk.clue_count == puzzle_info->width + puzzle_info->height);

    memset(row_patterns, 0, sizeof(row_patterns));
    memset(col_patterns, 0, sizeof(col_patterns));
    memset(row_counts, 0, sizeof(row_counts));
    memset(col_counts, 0, sizeof(col_counts));
    memset(out_black, 0, sizeof(out_black));
    memset(out_white, 0, sizeof(out_white));

    // I.a.w. the clues, precompute all valid row and column patterns.

    compute_valid_patterns(puzzle_info->width, row_patterns, row_counts, puzzle_info->chunk.clue_data,
        puzzle_info->width);

    compute_valid_patterns(puzzle_info->width, col_patterns, col_counts,
        &puzzle_info->chunk.clue_data[puzzle_info->width], puzzle_info->height);

    const uint32_t hls_ret = run_hls_core(solver, puzzle_info);
    xil_printf("HLS core says: %d\r\n", hls_ret);
    print_board(out_black, out_white, puzzle_info->width);
}
