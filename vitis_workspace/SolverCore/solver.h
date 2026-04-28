#ifndef SOLVER_H
#define SOLVER_H

#include <stdint.h>

typedef uint32_t line_t;
typedef uint8_t extent_t;

#define MAX_SIZE (20)
#define MAX_PATTERN_COUNT (256)

enum SolverState
{
    SOLVER_OK,
    SOLVER_STUCK,
    SOLVER_CONTRADICTION
};

uint32_t solver_toplevel(
    const line_t * row_patterns,
    const extent_t * row_counts,
    const line_t * col_patterns,
    const extent_t * col_counts,
    extent_t puzzle_size,
    line_t * out_black,
    line_t * out_white
);

#endif // SOLVER_H
