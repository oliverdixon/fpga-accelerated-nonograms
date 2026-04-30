/**
 * @file
 * @brief Line solver interface, intended to be ported to HLS to configure FPGA.
 */

#ifndef SW_PROTOTYPING_SOLVER_H
#define SW_PROTOTYPING_SOLVER_H

#include <stdint.h>

typedef uint32_t line_t;
typedef uint16_t extent_t;

#define MAX_SIZE (20)
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

/**
 * @brief Attempt to solve a Nonogram puzzle.
 * @param row_patterns Precomputed valid row patterns.
 * @param row_counts Indexed map for sizes of each row pattern.
 * @param col_patterns Precomputed valid column patterns.
 * @param col_counts Indexed map for sizes of each column pattern.
 * @param puzzle_size The extent of the square puzzle.
 * @param in_black Input for the previous black cell assignments.
 * @param in_white Input for the previous white cell assignments.
 * @param out_black Output for the black cell assignments.
 * @param out_white Output for the white cell assignments.
 * @return The state of the solver indicating whether a valid solution has been validated, the
 *  solver got stuck, or if an explicit contradiction was encountered.
 */
enum SolverState solve(
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

#endif // SW_PROTOTYPING_SOLVER_H
