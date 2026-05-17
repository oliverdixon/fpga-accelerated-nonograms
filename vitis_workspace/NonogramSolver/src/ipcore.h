// clang-format Language: C

/**
 * @file
 * @brief Solver HLS IP core manager interface
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#ifndef IPCORE_H
#define IPCORE_H

#include <FreeRTOS.h>
#include <stdbool.h>
#include <task.h>
#include <xsolver_toplevel.h>

#include "../../SolverCore/src/solver_params.h"

/**
 * @struct SearchJob
 * @brief Represents the state of a single job assigned to an IP core during the dual-core DFS procedure.
 */
struct SearchJob
{
    line_t black[MAX_SIZE] __attribute__((aligned(64))); /**< @brief Black cell assignments */
    line_t white[MAX_SIZE] __attribute__((aligned(64))); /**< @brief White cell assignments */
    unsigned int depth; /**< @brief Depth of the job in the DFS tree */
    bool propagated; /**< @brief Indicates propagation state of the job */
};

/**
 * @struct PendingJobs
 * @brief Deferred work queue for the backtracking DFS, bounded above by the maximum search depth.
 */
struct PendingJobs
{
    struct SearchJob jobs[MAX_SIZE * MAX_SIZE + 1]; /**< @brief Deferred work */
    extent_t count; /**< @brief Number of deferred jobs */
};

/**
 * @struct IPCore
 * @brief Represents a single interrupt-driven solver IP core.
 * @note This structure is not task/thread-safe; IP cores are intended to be exclusively accessed by a single RTOS task.
 */
struct IPCore
{
    bool busy; /**< @brief Lightweight single-task semaphore to indicate usage. */
    XSolver_toplevel solver; /**< @brief Xilinx SW driver IP core handle. */
    TaskHandle_t notify_task; /**< @brief Task to notify on interrupt from IP core. */
    uint32_t notify_bits; /**< @brief Notification bits to set on interrupt.  */

    struct SearchJob job; /**< @brief Current search job assigned to the core. */

    line_t out_black[MAX_SIZE] __attribute__((aligned(64))); /**< @brief Last-posted black cell assignments. */
    line_t out_white[MAX_SIZE] __attribute__((aligned(64))); /**< @brief Last-posted white cell assignments. */
};

struct Puzzle;

/**
 * @brief Initialise an IP core with the Xilinx SW API from the exported HW platform.
 * @param ipcore The target IP core management structure.
 * @param base_address The base address of the HLS component, e.g. <code>XPAR_SOLVER_TOPLEVEL_0_BASEADDR</code>.
 * @param interrupt_intr The fabric interrupt address, e.g. <code>XPAR_FABRIC_SOLVER_TOPLEVEL_0_INTR</code>
 * @param notify_bits The bits to set in the 32-bit FreeRTOS notification field upon interrupt of the HLS component.
 * @return Was the IP core successfully initialised?
 */
bool ipcore_initialise(
    struct IPCore * ipcore,
    uint32_t base_address,
    uint16_t interrupt_intr,
    uint32_t notify_bits
);

/**
 * @brief Execute the given IP core to line-solve the given puzzle with precomputed patterns.
 * @param ipcore The target IP core management structure.
 * @param puzzle_info Metadata of the Puzzle to refine.
 * @param row_patterns Precomputed valid row patterns for the given Puzzle.
 * @param row_counts Counts of the row patterns.
 * @param col_patterns Precomputed valid column patterns for the given Puzzle.
 * @param col_counts Counts of the column patterns.
 * @pre The IP core is not already busy.
 */
void ipcore_execute(
    struct IPCore * ipcore,
    const struct Puzzle * puzzle_info,
    const line_t * row_patterns,
    const extent_t * row_counts,
    const line_t * col_patterns,
    const extent_t * col_counts
);

/**
 * @brief Attempt to wind up a completed IP core.
 * @param ipcore The target IP core management structure.
 * @param puzzle_info The information of the Puzzle being refined by the IP core.
 * @details Invalidate CPU caches of FPGA-managed DDR regions and update the management structure.
 * @return The state of the solver returned by the HLS top-level function.
 */
enum SolverState ipcore_finish(
    struct IPCore *ipcore,
    const struct Puzzle *puzzle_info
);

/**
 * @brief Populate the given job as a child of the given parent job.
 * @param job The target IP core management structure.
 * @param parent_job The parent of the job being populated.
 * @param puzzle_info The information of the Puzzle being solved.
 * @param inherited_black Inherited black cell assignments from the parent DFS search node.
 * @param inherited_white Inherited white cell assignments from the parent DFS search node.
 * @param already_propagated Has the given job already been propagated?
 * @todo Move to searchjob.h
 */
void ipcore_populate_job(
    struct SearchJob *job,
    const struct SearchJob * parent_job,
    const struct Puzzle * puzzle_info,
    const line_t * inherited_black,
    const line_t * inherited_white,
    bool already_propagated
);

/**
 * @brief Enqueue the given job into the internal deferred work queue.
 * @param job The job to enqueue.
 * @return Was the given job enqueued?
 * @todo Move to searchjob.h
 */
bool ipcore_enqueue_job(const struct SearchJob * job);

/**
 * @brief Dequeue the topmost job from the internal deferred work queue into the given node.
 * @param job The destination node for the topmost deferred job.
 * @return Was the job dequeued into the destination node?
 * @todo Move to searchjob.h
 */
bool ipcore_dequeue_job(struct SearchJob * job);

#endif // IPCORE_H
