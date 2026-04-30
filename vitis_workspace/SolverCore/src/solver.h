#ifndef SOLVER_H
#define SOLVER_H

#include <stdint.h>

typedef uint32_t line_t;
typedef uint16_t extent_t;

#define MAX_SIZE (32)
#define MAX_PATTERN_COUNT (6000)

enum SolverState
{
    SOLVER_OK,
    SOLVER_STUCK,
    SOLVER_CONTRADICTION
};

#ifdef __cplusplus
extern "C" {

#endif

uint32_t solver_toplevel(
    const line_t * row_patterns,
    const extent_t * row_counts,
    const line_t * col_patterns,
    const extent_t * col_counts,
    extent_t puzzle_size,
    const line_t * in_black,
    const line_t * in_white,
    line_t * out_black,
    line_t * out_white
);

#ifdef __cplusplus
}
#endif

#endif // SOLVER_H
