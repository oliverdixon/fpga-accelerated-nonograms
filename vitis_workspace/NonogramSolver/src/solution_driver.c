#include <assert.h>
#include <xil_cache.h>
#include <xil_printf.h>
#include <xsolver_toplevel.h>

#include "solution_driver.h"
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

void solver_solve(XSolver_toplevel * const solver, const struct MessagePuzzleInfo * const puzzle_info)
{
    assert(puzzle_info->width == puzzle_info->height);

    memset(row_patterns, 0, sizeof(row_patterns));
    memset(col_patterns, 0, sizeof(col_patterns));
    memset(row_counts, 0, sizeof(row_counts));
    memset(col_counts, 0, sizeof(col_counts));
    memset(out_black, 0, sizeof(out_black));
    memset(out_white, 0, sizeof(out_white));

    // TODO: I.a.w. the clues, precompute all valid row and column patterns.
    
    const uint32_t hls_ret = run_hls_core(solver, puzzle_info);
    xil_printf("HLS core says: %d\r\n", hls_ret);
}
