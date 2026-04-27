/**
 * @file
 */

#ifndef SW_PROTOTYPING_SOLVER_H
#define SW_PROTOTYPING_SOLVER_H

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
 * @param out_black Output for the black cell assignments.
 * @param out_white Output for the white cell assignments.
 * @return The state of the solver indicating whether a valid solution has been validated, the solver got stuck, or if
 *  an explicit contradiction was encountered.
 */
enum SolverState solve(
    const line_t row_patterns[MAX_SIZE][MAX_PATTERN_COUNT],
    const extent_t row_counts[MAX_SIZE],
    const line_t col_patterns[MAX_SIZE][MAX_PATTERN_COUNT],
    const extent_t col_counts[MAX_SIZE],
    extent_t puzzle_size,
    line_t out_black[MAX_SIZE],
    line_t out_white[MAX_SIZE]
);

#ifdef __cplusplus
}
#endif

#endif // SW_PROTOTYPING_SOLVER_H
