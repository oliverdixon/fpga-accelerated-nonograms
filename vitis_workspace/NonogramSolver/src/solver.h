// clang-format Language: C

#ifndef SOLUTION_DRIVER_H
#define SOLUTION_DRIVER_H

#include <xsolver_toplevel.h>

#define MAX_SIZE (20)

typedef uint32_t line_t;
typedef uint16_t extent_t;

struct Puzzle;

enum SolverState
{
    SOLVER_OK,
    SOLVER_STUCK,
    SOLVER_CONTRADICTION,
    SOLVER_UNFINISHED = 0xFF
};

void solver_initialise_environment();

void solver_solve(struct Puzzle * puzzle_info);

#endif // SOLUTION_DRIVER_H
