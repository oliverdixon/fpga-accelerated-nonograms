#include <assert.h>
#include <xil_cache.h>
#include <xsolver_toplevel.h>

#include "ipcore.h"
#include "logging.h"
#include "puzzle.h"
#include "solver.h"

static struct PendingJobs pending_jobs = {
    .count = 0
};

static uint32_t clear_pending_interrupts(XSolver_toplevel * const solver)
{
    const uint32_t interrupt_status = XSolver_toplevel_InterruptGetStatus(solver);
    if (interrupt_status != 0)
        XSolver_toplevel_InterruptClear(solver, interrupt_status);

    return interrupt_status;
}

static void finished_isr(void * const data)
{
    struct IPCore * const ipcore = data;
    BaseType_t higher_priority_task_woken = pdFALSE;
    const uint32_t status = XSolver_toplevel_InterruptGetStatus(&ipcore->solver);
    XSolver_toplevel_InterruptClear(&ipcore->solver, status);

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
    ipcore->return_code = SOLVER_UNFINISHED;
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

    XSolver_toplevel_Set_in_black(&ipcore->solver, (UINTPTR)ipcore->in_black);
    XSolver_toplevel_Set_in_white(&ipcore->solver, (UINTPTR)ipcore->in_white);

    XSolver_toplevel_Set_out_black(&ipcore->solver, (UINTPTR)ipcore->out_black);
    XSolver_toplevel_Set_out_white(&ipcore->solver, (UINTPTR)ipcore->out_white);

    const unsigned int line_length = sizeof(line_t) * puzzle_info->width;

    Xil_DCacheFlushRange((UINTPTR)ipcore->in_black, line_length);
    Xil_DCacheFlushRange((UINTPTR)ipcore->in_white, line_length);

    Xil_DCacheInvalidateRange((UINTPTR)ipcore->out_black, line_length);
    Xil_DCacheInvalidateRange((UINTPTR)ipcore->out_white, line_length);

    ipcore->busy = true;
    ipcore->return_code = SOLVER_UNFINISHED;
    XSolver_toplevel_Start(&ipcore->solver);
}

void ipcore_finish(
    struct IPCore * const ipcore,
    const struct Puzzle * const puzzle_info
)
{
    ipcore->return_code = (enum SolverState)XSolver_toplevel_Get_return(&ipcore->solver);

    if (ipcore->return_code != SOLVER_UNFINISHED) {
        const unsigned int line_length = sizeof(line_t) * puzzle_info->width;
        Xil_DCacheInvalidateRange((UINTPTR)ipcore->out_black, line_length);
        Xil_DCacheInvalidateRange((UINTPTR)ipcore->out_white, line_length);
        ipcore->busy = false;
    }
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
