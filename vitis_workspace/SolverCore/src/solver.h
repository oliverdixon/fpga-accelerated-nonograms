/**
 * @file
 * @brief HLS Nonogram line solver interface
 * @author Oliver Dixon <od641@york.ac.uk>
 * @date 2026-04-30
 */

#ifndef SOLVER_H
#define SOLVER_H

#include <stdint.h>

/**
 * @brief A bitset describing a single line (row or transposed column) of the puzzle.
 * @details A set bit indicates an assignment, and a cleared bit indicates no assignment at
 *  the corresponding index.
 */
typedef uint32_t line_t;

/**
 * @brief A general utility type to store lengths and extents in puzzle computations.
 */
typedef uint16_t extent_t;

/**
 * @brief Maximum extent of the square puzzle grid
 */
#define MAX_SIZE (20)

/**
 * @brief Maximum number of valid patterns produced for a single line, parameterised by
 *  MAX_SIZE.
 * @details The total number of valid pattern combinations is given by the binomial coefficient
 *  @f$ P := {{n-s+1}\choose k} @f$, where @f$ n @f$ is the puzzle extent, @f$ s @f$ is the total
 *  filled-cell count, and @f$ k @f$ is the number of clue blocks. @f$ P @f$ is maximised for
 *  @f$ N=20 @f$ when @f$ k=6 @f$ and @f$ s=6 @f$.
 */
#define MAX_PATTERN_COUNT (5005)

/**
 * @enum SolverState
 * @brief Descriptor for the result of a single line-solving run.
 */
enum SolverState
{
    SOLVER_OK,    /**< @brief Solver produced the cell assignments which require validation. */
    SOLVER_STUCK, /**< @brief Solver got stuck and needs to backtrack. */
    SOLVER_CONTRADICTION /**< @brief Solver encountered an assignment contradiction. */
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
