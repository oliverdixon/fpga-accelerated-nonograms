// clang-format Language: C

#ifndef IPCORE_H
#define IPCORE_H

#include <stdbool.h>

#include <FreeRTOS.h>
#include <task.h>
#include <xsolver_toplevel.h>

#include "../../SolverCore/src/solver_params.h"

struct SearchJob
{
    line_t black[MAX_SIZE];
    line_t white[MAX_SIZE];
    unsigned int depth;
    bool propagated;
};

struct PendingJobs
{
    struct SearchJob jobs[(MAX_SIZE * MAX_SIZE) + 1];
    extent_t count;
};

struct IPCore
{
    bool busy;
    XSolver_toplevel solver;
    TaskHandle_t notify_task;
    uint32_t notify_bits;

    struct SearchJob job;
    enum SolverState return_code;

    line_t in_black[MAX_SIZE];
    line_t in_white[MAX_SIZE];
    line_t out_black[MAX_SIZE];
    line_t out_white[MAX_SIZE];
};

struct Puzzle;

bool ipcore_initialise(
    struct IPCore * ipcore,
    uint32_t base_address,
    uint16_t interrupt_intr,
    uint32_t notify_bits
);

void ipcore_execute(
    struct IPCore * ipcore,
    const struct Puzzle * puzzle_info,
    const line_t * row_patterns,
    const extent_t * row_counts,
    const line_t * col_patterns,
    const extent_t * col_counts
);

void ipcore_finish(
    struct IPCore * ipcore,
    const struct Puzzle * const puzzle_info
);

bool ipcore_enqueue_job(const struct SearchJob * job);

bool ipcore_dequeue_job(struct SearchJob * job);

#endif // IPCORE_H
