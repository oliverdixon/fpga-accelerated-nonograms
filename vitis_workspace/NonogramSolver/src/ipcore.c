/**
 * @file
 * @brief Solver HLS IP core manager implementation
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#include <assert.h>
#include <xil_cache.h>
#include <xsolver_toplevel.h>

#include "ipcore.h"
#include "puzzle.h"
#include "solver.h"

/**
 * @brief Clear any pending interrupts on the given solver core.
 * @param solver The solver from which to clear interrupts.
 * @return The interrupt state, prior to clearing.
 */
static uint32_t clear_pending_interrupts(
    XSolver_toplevel * const solver
) {
    const uint32_t interrupt_status = XSolver_toplevel_InterruptGetStatus(solver);
    if (interrupt_status != 0)
        XSolver_toplevel_InterruptClear(solver, interrupt_status);

    return interrupt_status;
}

/**
 * @brief ISR to receive interrupts from the solver IP cores and propagate notifications to the
 *  solver task.
 * @param data Task payload as a pointer to the IP core metadata.
 */
static void finished_isr(
    void * const data
) {
    struct IPCore * const ipcore = data;
    BaseType_t higher_priority_task_woken = pdFALSE;
    const uint32_t status = clear_pending_interrupts(&ipcore->solver);

    if ((status & 1) != 0)
        xTaskNotifyFromISR(ipcore->notify_task, ipcore->notify_bits, eSetBits, &higher_priority_task_woken);

    portYIELD_FROM_ISR(higher_priority_task_woken);
}

bool ipcore_initialise(
    struct IPCore * const ipcore,
    const uint32_t base_address,
    const uint16_t interrupt_intr,
    const uint32_t notify_bits
) {
    ipcore->busy = true;
    ipcore->job.depth = 0;
    ipcore->job.propagated = false;
    ipcore->notify_task = xTaskGetCurrentTaskHandle();
    ipcore->notify_bits = notify_bits;

    const int status = XSolver_toplevel_Initialize(&ipcore->solver, base_address);

    if (status == 0) {
        clear_pending_interrupts(&ipcore->solver);
        XSolver_toplevel_InterruptGlobalEnable(&ipcore->solver);
        XSolver_toplevel_InterruptEnable(&ipcore->solver, 0x01);

        if (xPortInstallInterruptHandler(interrupt_intr, finished_isr, ipcore) != pdPASS)
            return false;

        vPortEnableInterrupt(interrupt_intr);

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
    assert(!ipcore->busy);
    clear_pending_interrupts(&ipcore->solver);

    XSolver_toplevel_Set_row_patterns(&ipcore->solver, (UINTPTR)row_patterns);
    XSolver_toplevel_Set_row_counts(&ipcore->solver, (UINTPTR)row_counts);
    XSolver_toplevel_Set_col_patterns(&ipcore->solver, (UINTPTR)col_patterns);
    XSolver_toplevel_Set_col_counts(&ipcore->solver, (UINTPTR)col_counts);

    XSolver_toplevel_Set_puzzle_size(&ipcore->solver, puzzle_info->width);

    XSolver_toplevel_Set_in_black(&ipcore->solver, (UINTPTR)ipcore->job.black);
    XSolver_toplevel_Set_in_white(&ipcore->solver, (UINTPTR)ipcore->job.white);

    XSolver_toplevel_Set_out_black(&ipcore->solver, (UINTPTR)ipcore->out_black);
    XSolver_toplevel_Set_out_white(&ipcore->solver, (UINTPTR)ipcore->out_white);

    const unsigned int line_length = sizeof(line_t) * puzzle_info->width;

    Xil_DCacheFlushRange((UINTPTR)ipcore->job.black, line_length);
    Xil_DCacheFlushRange((UINTPTR)ipcore->job.white, line_length);

    /*
     * The out_{black,white} buffers are controlled by the FPGA, so invalidate the CPU's cache of them. This is distinct
     * the flushing the "CPU-owned" buffers.
     */
    Xil_DCacheInvalidateRange((UINTPTR)ipcore->out_black, line_length);
    Xil_DCacheInvalidateRange((UINTPTR)ipcore->out_white, line_length);

    ipcore->busy = true;
    XSolver_toplevel_Start(&ipcore->solver);
}

enum SolverState ipcore_finish(
    struct IPCore *const ipcore,
    const struct Puzzle *const puzzle_info
) {
    if (XSolver_toplevel_IsDone(&ipcore->solver)) {
        const unsigned int line_length = sizeof(line_t) * puzzle_info->width;
        Xil_DCacheInvalidateRange((UINTPTR)ipcore->out_black, line_length);
        Xil_DCacheInvalidateRange((UINTPTR)ipcore->out_white, line_length);
        ipcore->busy = false;

        return (enum SolverState)XSolver_toplevel_Get_return(&ipcore->solver);
    }

    return SOLVER_UNFINISHED;
}
