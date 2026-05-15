// clang-format Language: C

/**
 * @file
 * @brief Nonogram solver driver interface
 * @date 2026-05-15
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#ifndef SOLVER_H
#define SOLVER_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_SIZE (20)

typedef uint32_t line_t;
typedef uint16_t extent_t;

struct Puzzle;

/**
 * @enum SolverState
 * @brief Describes the return code of the solver.
 */
enum SolverState
{
    SOLVER_OK, /**< @brief The solver reported success */
    SOLVER_STUCK, /**< @brief The solver reached a fixed point where a cell must be manually chosen. */
    SOLVER_CONTRADICTION, /**< @brief The solver derived a contradiction against the clue data. */
    SOLVER_UNFINISHED = 0xFF /**< @brief The solver has not finished execution.  */
};


/**
 * @brief Initialise the solver environment, including all line-solving HLS IP cores.
 * @return Was the environment successfully initialised?
 */
bool solver_initialise_environment();

/**
 * @brief Attempt to solve the described Puzzle and write solution data to the Puzzle bitmap.
 * @param puzzle_info The Puzzle to solve.
 * @pre The Puzzle is square.
 * @pre The Puzzle contains as many clues as combined rows and columns.
 */
void solver_solve(struct Puzzle * puzzle_info);

#endif // SOLVER_H
