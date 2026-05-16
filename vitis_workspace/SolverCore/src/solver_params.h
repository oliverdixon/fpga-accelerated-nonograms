#ifndef SOLVER_PARAMS_H
#define SOLVER_PARAMS_H

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
typedef uint32_t extent_t;

/**
 * @brief Maximum extent of the square puzzle grid
 */
#define MAX_SIZE (32)

/**
 * @brief Maximum number of valid patterns produced for a single line, parameterised by
 *  MAX_SIZE.
 * @details The total number of valid pattern combinations is given by the binomial coefficient
 *  @f$ P := {{n-s+1}\choose k} @f$, where @f$ n @f$ is the puzzle extent, @f$ s @f$ is the total
 *  filled-cell count, and @f$ k @f$ is the number of clue blocks.
 */
#define MAX_PATTERN_COUNT (1307504)

/**
 * @enum SolverState
 * @brief Describes the return code of the solver.
 */
enum SolverState
{
    SOLVER_OK,    /**< @brief The solver reported success */
    SOLVER_STUCK, /**< @brief The solver reached a fixed point where a cell must be manually chosen.
                   */
    SOLVER_CONTRADICTION, /**< @brief The solver derived a contradiction against the clue data. */
    SOLVER_UNFINISHED = 0xFF /**< @brief The solver has not finished execution.  */
};

#endif // SOLVER_PARAMS_H
