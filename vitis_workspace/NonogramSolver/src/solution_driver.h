#ifndef SOLUTION_DRIVER_H
#define SOLUTION_DRIVER_H

#include <xsolver_toplevel.h>

struct MessagePuzzleInfo;

void solver_solve(XSolver_toplevel * solver, const struct MessagePuzzleInfo * puzzle_info);

#endif // SOLUTION_DRIVER_H
