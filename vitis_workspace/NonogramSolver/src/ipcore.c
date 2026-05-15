#include <xil_cache.h>
#include <xsolver_toplevel.h>

#include "ipcore.h"
#include "logging.h"
#include "puzzle.h"
#include "solver.h"

static struct PendingJobs pending_jobs = {
    .count = 0
};

bool ipcore_initialise(
    struct IPCore * const ipcore,
    const uint32_t base_address
) {
    ipcore->busy = true;
    ipcore->job.depth = 0;
    ipcore->job.propagated = false;
    
    const int status = XSolver_toplevel_Initialize(&ipcore->solver, base_address);

    if (status == 0) {
        XSolver_toplevel_InterruptEnable(&ipcore->solver, 0x01);
        ipcore->busy = false;
        return true;
    }

    return false;
}

void ipcore_execute(
    struct IPCore * const ipcore,
    const struct Puzzle * const puzzle_info,
    const line_t * const row_patterns,
    const extent_t * const row_counts,
    const line_t * const col_patterns,
    const extent_t * const col_counts
) {
    const unsigned int line_length = sizeof(line_t) * puzzle_info->width;

    XSolver_toplevel_Set_row_patterns(&ipcore->solver, (UINTPTR)row_patterns);
    XSolver_toplevel_Set_row_counts(&ipcore->solver, (UINTPTR)row_counts);
    XSolver_toplevel_Set_col_patterns(&ipcore->solver, (UINTPTR)col_patterns);
    XSolver_toplevel_Set_col_counts(&ipcore->solver, (UINTPTR)col_counts);

    XSolver_toplevel_Set_puzzle_size(&ipcore->solver, puzzle_info->width);

    XSolver_toplevel_Set_in_black(&ipcore->solver, (UINTPTR)ipcore->in_black);
    XSolver_toplevel_Set_in_white(&ipcore->solver, (UINTPTR)ipcore->in_white);

    XSolver_toplevel_Set_out_black(&ipcore->solver, (UINTPTR)ipcore->out_black);
    XSolver_toplevel_Set_out_white(&ipcore->solver, (UINTPTR)ipcore->out_white);

    Xil_DCacheFlushRange((UINTPTR)ipcore->in_black, line_length);
    Xil_DCacheFlushRange((UINTPTR)ipcore->in_white, line_length);

    Xil_DCacheFlushRange((UINTPTR)ipcore->out_black, line_length);
    Xil_DCacheFlushRange((UINTPTR)ipcore->out_white, line_length);

    ipcore->busy = true;
    XSolver_toplevel_Start(&ipcore->solver);
}

enum SolverState ipcore_finish(
    struct IPCore * const ipcore,
    const struct Puzzle * const puzzle_info
)
{
    // TODO switch to an interrupt driven model.
    if (XSolver_toplevel_IsDone(&ipcore->solver)) {
        ipcore->busy = false;

        const unsigned int line_length = sizeof(line_t) * puzzle_info->width;
        Xil_DCacheInvalidateRange((UINTPTR)ipcore->out_black, line_length);
        Xil_DCacheInvalidateRange((UINTPTR)ipcore->out_white, line_length);

        return (enum SolverState)XSolver_toplevel_Get_return(&ipcore->solver);
    }

    return SOLVER_UNFINISHED;
}

bool ipcore_enqueue_job(
    const struct SearchJob * const job
)
{
    if (pending_jobs.count >= (MAX_SIZE * MAX_SIZE) + 1)
        return false;

    pending_jobs.jobs[pending_jobs.count++] = *job;
    return true;
}

bool ipcore_dequeue_job(
    struct SearchJob * const job
)
{
    if (pending_jobs.count == 0)
        return false;

    *job = pending_jobs.jobs[--pending_jobs.count];
    return true;
}
