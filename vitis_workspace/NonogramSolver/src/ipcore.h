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

#include "searchjob.h"
#include "../../SolverCore/src/solver_params.h"

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
 * @details Invalidate CPU caches of FPGA-managed DDR regions and update the management structure. If the HLS core is
 *  still running, this function is a no-op.
 * @return The state of the solver returned by the HLS top-level function, or SOLVER_UNFINISHED.
 */
enum SolverState ipcore_finish(
    struct IPCore *ipcore,
    const struct Puzzle *puzzle_info
);

#endif // IPCORE_H
