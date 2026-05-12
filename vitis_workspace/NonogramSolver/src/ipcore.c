#include <xil_cache.h>
#include <xsolver_toplevel.h>

#include "ipcore.h"
#include "logging.h"
#include "puzzle.h"
#include "solver.h"

bool ipcore_initialise(
    struct IPCore * const ipcore,
    const uint32_t base_address
) {
    ipcore->busy = true;
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

    // TODO is it necessary to flush outputs here?
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
