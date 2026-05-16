/**
 * @file
 * @brief HLS Nonogram line solver interface
 * @author Oliver Dixon <od641@york.ac.uk>
 * @date 2026-04-30
 */

#ifndef SOLVER_H
#define SOLVER_H

#include "solver_params.h"

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
