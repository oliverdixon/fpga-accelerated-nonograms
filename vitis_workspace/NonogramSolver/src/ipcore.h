// clang-format Language: C

#ifndef IPCORE_H
#define IPCORE_H

#include <stdbool.h>

#include "solver.h"

struct SearchJob
{
    line_t black[MAX_SIZE];
    line_t white[MAX_SIZE];
};

struct IPCore
{
    bool busy;
    XSolver_toplevel solver;
    struct SearchJob job;
    line_t in_black[MAX_SIZE];
    line_t in_white[MAX_SIZE];
    line_t out_black[MAX_SIZE];
    line_t out_white[MAX_SIZE];
};

bool ipcore_initialise(
    struct IPCore * ipcore,
    uint32_t base_address
);

void ipcore_execute(
    struct IPCore * ipcore,
    const struct Puzzle * puzzle_info,
    const line_t * row_patterns,
    const extent_t * row_counts,
    const line_t * col_patterns,
    const extent_t * col_counts
);

enum SolverState ipcore_finish(
    struct IPCore * ipcore,
    const struct Puzzle * puzzle_info
);

#endif // IPCORE_H
