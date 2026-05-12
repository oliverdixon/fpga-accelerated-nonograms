// clang-format Language: C

#ifndef SOLUTION_DRIVER_H
#define SOLUTION_DRIVER_H

#include <xsolver_toplevel.h>

#define MAX_SIZE (20)

typedef uint32_t line_t;
typedef uint16_t extent_t;

struct Puzzle;

void solver_initialise_environment();

void solver_solve(
    XSolver_toplevel * solver,
    struct Puzzle * puzzle_info
);

#endif // SOLUTION_DRIVER_H
