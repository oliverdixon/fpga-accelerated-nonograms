/**
 * @file
 * @brief Nonogram solver driver implementation
 * @date 2026-05-15
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <xil_cache.h>

#include "../../SolverCore/src/solver_params.h"
#include "chunks.h"
#include "ipcore.h"
#include "logging.h"
#include "puzzle.h"
#include "solver.h"

#define MAX_SEARCH_DEPTH (MAX_SIZE * MAX_SIZE) /**< @brief Maximum recursion depth for the dual-core DFS solver. */
#define IPCORE_COUNT (2) /**< @brief Number of solver IP cores available on the exported HW platform. */

static struct IPCore cores[IPCORE_COUNT]; /**< @brief Solver IP core management blocks */

/**
 * @struct CellChoice
 * @brief Identifier for a cell, determined by its indices, and a flag to indicate if the cell is a fixed point.
 */
struct CellChoice
{
    extent_t row; /**< @brief The row index between 0 and <code>MAX_SIZE - 1</code>. */
    extent_t col; /**< @brief The column index between 0 and <code>MAX_SIZE - 1</code>. */
    bool valid;   /**< @brief Are the indices valid? */
};

static line_t row_patterns[MAX_SIZE * MAX_PATTERN_COUNT]
    __attribute__((section(".pattern_buffers"), aligned(64))); /**< @brief Row patterns given session ClueData. */

static line_t col_patterns[MAX_SIZE * MAX_PATTERN_COUNT]
    __attribute__((section(".pattern_buffers"), aligned(64))); /**< @brief Column patterns given session ClueData. */

static extent_t row_counts[MAX_SIZE]; /**< @brief Counts of row patterns given session ClueData. */
static extent_t col_counts[MAX_SIZE]; /**< @brief Counts of column patterns given session ClueData. */

static line_t out_black[MAX_SIZE]; /**< @brief Final black assignments for active Puzzle. */
static line_t out_white[MAX_SIZE]; /**< @brief Final white assignments for active Puzzle. */

/**
 * @brief Identify the next cell without a black or white cell assignment.
 * @param black The black cell assignments.
 * @param white The white cell assignments.
 * @param puzzle_extent The extent of the square Puzzle.
 * @return The CellChoice identifying the unassigned cell.
 * @warning When used during search, this is quite a poor heuristic. Better ones exist!
 */
static struct CellChoice choose_unknown(
    const line_t * const black,
    const line_t * const white,
    const extent_t puzzle_extent
) {
    struct CellChoice choice = {.valid = false};

    for (extent_t row_idx = 0; row_idx < puzzle_extent; ++row_idx) {
        const line_t known = black[row_idx] | white[row_idx];
        for (extent_t col_idx = 0; col_idx < puzzle_extent; ++col_idx) {
            const line_t col_mask = 1U << col_idx;
            if ((known & col_mask) == 0) {
                choice.valid = true;
                choice.row = row_idx;
                choice.col = col_idx;
                return choice;
            }
        }
    }

    return choice;
}

/**
 * @brief Execute a job synchronously on a single core, blocking until the IP core returns.
 * @param ipcore The IP core to execute.
 * @param puzzle_info The Puzzle to solve.
 * @param in_black The input black cell assignment lines.
 * @param in_white The input white cell assignment lines.
 * @return The return code from the HLS solver.
 * @post The IP core was released following the job.
 */
static enum SolverState run_core_sync(
    struct IPCore * const ipcore,
    const struct Puzzle * const puzzle_info,
    const line_t * const in_black,
    const line_t * const in_white
) {
    const size_t board_bytes = puzzle_info->width * sizeof(line_t);

    memcpy(ipcore->job.black, in_black, board_bytes);
    memcpy(ipcore->job.white, in_white, board_bytes);

    ipcore_execute(ipcore, puzzle_info, row_patterns, row_counts, col_patterns, col_counts);

    // Wait until we receive (and verify) a notification signalling the completion of the IP core.
    enum SolverState state = SOLVER_UNFINISHED;
    uint32_t notify_bits = 0;

    while (state == SOLVER_UNFINISHED) {
        xTaskNotifyWait(0x00, UINT32_MAX, &notify_bits, portMAX_DELAY);

        if ((notify_bits & ipcore->notify_bits) == 0)
            continue;

        state = ipcore_finish(ipcore, puzzle_info);
    }

    assert(!ipcore->busy);
    return state;
}

/**
 * @brief Burn through any pending notifications on the current FreeRTOS task.
 */
static void drain_solver_notifications() {
    uint32_t ignored = 0;
    while (xTaskNotifyWait(0x00u, UINT32_MAX, &ignored, 0) == pdTRUE)
        ;
}

/**
 * @brief Evaluate the two immediate binary branches of a stuck search state in parallel.
 * @param current_job The current DFS job whose propagated fixed point is being branched from.
 * @param choice The unresolved cell selected as the branching point.
 * @param puzzle_info Metadata and shared puzzle state for the puzzle being solved.
 * @param propagated_black Black-cell assignments after propagation of the current job.
 * @param propagated_white White-cell assignments after propagation of the current job.
 * @param black_status Output status returned by the solver core assigned to the black branch.
 * @param white_status Output status returned by the solver core assigned to the white branch.
 * @details
 *  <p>
 *      This function creates two child jobs from the propagated state of the current search node: one assuming the
 *      chosen cell is black, and one assuming the chosen cell is white.
 *  </p>
 *  <p>
 *      By convention, the black branch is assigned to solver core 0 and the white branch is assigned to solver core 1.
 *      Both IP cores are started before either result is consumed, allowing the two child branches to be propagated
 *      concurrently.
 *  </p>
 *  <p>
 *      The function blocks until both solver cores have completed and writes their final SolverState values to the
 *      given black and white statuses.
 *  </p>
 *
 * @note The output buffers remain valid only until the corresponding core is reused.
 * @pre Core 0 is not busy.
 * @pre Core 1 is not busy.
 */
static void explore_binary_children(
    const struct SearchJob * const current_job,
    const struct CellChoice * const choice,
    const struct Puzzle * const puzzle_info,
    const line_t * const propagated_black,
    const line_t * const propagated_white,
    enum SolverState * const black_status,
    enum SolverState * const white_status
) {
    assert(!cores[0].busy);
    assert(!cores[1].busy);

    // By convention, explore the black branch on Core 0, and the white branch on Core 1.
    struct SearchJob * const black_job = &cores[0].job;
    struct SearchJob * const white_job = &cores[1].job;

    searchjob_populate(black_job, current_job, puzzle_info, propagated_black, propagated_white, false);
    searchjob_populate(white_job, current_job, puzzle_info, propagated_black, propagated_white, false);

    const line_t mask = (line_t)1U << choice->col;
    black_job->black[choice->row] |= mask;
    white_job->white[choice->row] |= mask;

    *black_status = SOLVER_UNFINISHED;
    *white_status = SOLVER_UNFINISHED;

    drain_solver_notifications();
    ipcore_execute(&cores[0], puzzle_info, row_patterns, row_counts, col_patterns, col_counts);
    ipcore_execute(&cores[1], puzzle_info, row_patterns, row_counts, col_patterns, col_counts);

    while (*black_status == SOLVER_UNFINISHED || *white_status == SOLVER_UNFINISHED) {
        uint32_t notify_bits = 0;
        xTaskNotifyWait(0x00, UINT32_MAX, &notify_bits, portMAX_DELAY);

        if (*black_status == SOLVER_UNFINISHED && notify_bits & cores[0].notify_bits)
            *black_status = ipcore_finish(&cores[0], puzzle_info);

        if (*white_status == SOLVER_UNFINISHED && notify_bits & cores[1].notify_bits)
            *white_status = ipcore_finish(&cores[1], puzzle_info);
    }
}

/**
 * @brief Solve the current puzzle using depth-first search with two parallel HLS propagation cores.
 * @param puzzle_info Metadata, clues, and solution storage for the puzzle being solved.
 * @return The result of the search.
 * @details
 *  <p>
 *      This function keeps recursive search control in software while using the HLS solver IP cores to propagate
 *      individual board states to a fixed point.
 *  </p>
 *  <p>
 *      The search begins from an empty assignment. Each job contains black/white row masks and a DFS depth. For jobs
 *      that have not yet been propagated, solver core 0 is used synchronously to obtain a fixed point. Jobs marked as
 *      already propagated are reused directly without another HLS call.
 *  </p>
 *  <p>
 *      If propagation returns:
 *      <ul>
 *          <li><code>SOLVER_CONTRADICTION</code>, the branch is discarded and deferred work is popped</li>;
 *          <li><code>SOLVER_OK</code>, the propagated board is copied to <code>out_black</code> or
 *              <code>out_white</code>;</li>
 *          <li><code>SOLVER_STUCK</code>, an unresolved cell is selected and two child jobs are created</li>.
 *      </ul>
 *      For stuck states, the chosen cell is branched in both polarities. The black and white assumptions are evaluated
 *      concurrently by @ref explore_binary_children. If both branches remain stuck, the black branch is continued
 *      depth-first and the white branch is deferred via the search-job stack. If only one branch remains stuck, that
 *      branch becomes the next current job. If both branches contradict, the driver backtracks to deferred work.
 *  </p>
 *
 * @note The function currently branches on a single unknown cell at a time. Hard puzzles may still
 *  require many search nodes even with two propagation cores.
 */
static enum SearchResult search_two_core_dfs(
    const struct Puzzle * const puzzle_info
) {
    const size_t board_bytes = puzzle_info->width * sizeof(line_t);
    bool saw_unknown = false;

    struct SearchJob current = {.propagated = false, .depth = 0};

    memset(current.black, 0, board_bytes);
    memset(current.white, 0, board_bytes);

    unsigned int max_seen_depth = 0;

    while (1) {
        if (current.depth > max_seen_depth) {
            max_seen_depth = current.depth;
            logging_printf("Searcher saw new maximum depth of %d.", current.depth);
        }

        if (current.depth > MAX_SEARCH_DEPTH) {
            saw_unknown = true;

            if (!searchjob_dequeue(&current))
                return SEARCH_UNKNOWN;

            continue;
        }

        enum SolverState status;
        line_t propagated_black[MAX_SIZE];
        line_t propagated_white[MAX_SIZE];

        if (current.propagated) {
            /*
             * If this job came directly from a previous SOLVER_STUCK result, it is already
             * propagated.
             */
            memcpy(propagated_black, current.black, board_bytes);
            memcpy(propagated_white, current.white, board_bytes);
            status = SOLVER_STUCK;
        } else {
            // Otherwise, allocate a core to run the job.
            status = run_core_sync(&cores[0], puzzle_info, current.black, current.white);

            if (status == SOLVER_OK || status == SOLVER_STUCK) {
                memcpy(propagated_black, cores[0].out_black, board_bytes);
                memcpy(propagated_white, cores[0].out_white, board_bytes);
            }
        }

        // Base case: if the job derived a contradiction, dequeue it and report back up the chain.
        if (status == SOLVER_CONTRADICTION) {
            if (!searchjob_dequeue(&current))
                return saw_unknown ? SEARCH_UNKNOWN : SEARCH_FAILED;

            continue;
        }

        // Base case: likewise, if the job derived a solution, report it back up the chain.
        if (status == SOLVER_OK) {
            memcpy(out_black, propagated_black, board_bytes);
            memcpy(out_white, propagated_white, board_bytes);
            return SEARCH_SOLVED;
        }

        /*
         * Inductive case: if the solver reports SOLVER_STUCK, spawn a couple of new jobs for each
         * branch at the fixed point and allocate one to each solver IP core.
         */
        const struct CellChoice choice = choose_unknown(propagated_black, propagated_white, puzzle_info->width);

        if (!choice.valid) {
            /*
             * If there's no valid choice, no fixed point was reached. We didn't reach an explicit
             * contradiction in the clue data, but this branch is useless.
             */
            saw_unknown = true;
            if (!searchjob_dequeue(&current))
                return SEARCH_UNKNOWN;

            continue;
        }

        const line_t mask = (line_t)1U << choice.col;
        if ((propagated_black[choice.row] | propagated_white[choice.row]) & mask) {
            saw_unknown = true;

            if (!searchjob_dequeue(&current))
                return SEARCH_UNKNOWN;

            continue;
        }

        enum SolverState black_status = SOLVER_UNFINISHED;
        enum SolverState white_status = SOLVER_UNFINISHED;
        explore_binary_children(
            &current, &choice, puzzle_info, propagated_black, propagated_white, &black_status, &white_status
        );

        // If one of the branches derived a solution, we're done.
        if (black_status == SOLVER_OK) {
            memcpy(out_black, cores[0].out_black, board_bytes);
            memcpy(out_white, cores[0].out_white, board_bytes);
            return SEARCH_SOLVED;
        }

        if (white_status == SOLVER_OK) {
            memcpy(out_black, cores[1].out_black, board_bytes);
            memcpy(out_white, cores[1].out_white, board_bytes);
            return SEARCH_SOLVED;
        }

        // Otherwise, propagate jobs for each stuck branch.
        struct SearchJob black_next;
        struct SearchJob white_next;

        const bool black_stuck = black_status == SOLVER_STUCK;
        const bool white_stuck = white_status == SOLVER_STUCK;

        if (black_stuck)
            searchjob_populate(&black_next, &cores[0].job, puzzle_info, cores[0].out_black, cores[0].out_white, true);

        if (white_stuck)
            searchjob_populate(&white_next, &cores[1].job, puzzle_info, cores[1].out_black, cores[1].out_white, true);

        // Continue depth-first. If both are stuck, continue with the black branch and defer white.
        if (black_stuck && white_stuck) {
            if (!searchjob_enqueue(&white_next))
                saw_unknown = true;

            current = black_next;
            continue;
        }

        if (black_stuck) {
            current = black_next;
            continue;
        }

        if (white_stuck) {
            current = white_next;
            continue;
        }

        // Both children contradicted. Backtrack to deferred work.
        if (!searchjob_dequeue(&current))
            return saw_unknown ? SEARCH_UNKNOWN : SEARCH_FAILED;
    }
}

/**
 * @brief Compute the minimum number of cells required to place the remaining clue blocks.
 * @param block The clue group describing one row or column.
 * @param start_idx Index of the first clue block to include in the suffix calculation.
 * @return The minimum number of cells required to place clues <code>start_idx..block->count - 1</code>, including
 *  mandatory single-cell separators between adjacent remaining blocks.
 * @details
 *  This is used during pattern generation to determine how far the current clue block may be shifted to the right while
 *  still leaving enough space for all later clue blocks.
 * @pre block != NULL
 */
static unsigned int min_required_tail(
    const struct ClueGroup * const block,
    const unsigned int start_idx
) {
    assert(block != NULL);
    if (start_idx >= block->count)
        return 0;

    unsigned int sum = 0;
    for (unsigned int idx = start_idx; idx < block->count; ++idx)
        sum += block->clues[idx];

    sum += block->count - start_idx - 1;
    return sum;
}

/**
 * @brief Recursively enumerate all valid bit-mask patterns for a row or column clue group.
 * @param puzzle_size Extent of the square puzzle, in cells.
 * @param block The clue group for the line.
 * @param clue_idx Index of the clue block currently being placed.
 * @param min_start_idx Earliest cell index at which the current clue block may start.
 * @param partial_line Bit mask containing all clue blocks already placed by earlier recursion levels.
 * @param pattern_out Destination array receiving generated line patterns.
 * @param next_pattern_idx Index of the next free entry in <code>pattern_out</code>.
 * @return The next free pattern index after all patterns reachable from this recursion state have been emitted.
 * @details
 *  Each recursive level places exactly one clue block. The block is tried at every legal start position from
 *  <code>min_start_idx</code> to the latest position that still leaves enough room for the remaining blocks and their
 *  mandatory separators.
 * @pre block != NULL
 */
static extent_t generate_pattern_induction(
    const extent_t puzzle_size,
    const struct ClueGroup * const block,
    const unsigned int clue_idx,
    const unsigned int min_start_idx,
    const line_t partial_line,
    line_t * const pattern_out,
    extent_t next_pattern_idx
) {
    assert(block != NULL);
    if (clue_idx == block->count) {
        // Base case: we've reached the end of the clues, so commit our current line.
        pattern_out[next_pattern_idx++] = partial_line;
        return next_pattern_idx;
    }

    const unsigned int block_len = block->clues[clue_idx]; // The length of the target continuous block.
    const unsigned int latest_start_idx = puzzle_size - block_len - min_required_tail(block, clue_idx + 1);

    for (unsigned int start_idx = min_start_idx; start_idx <= latest_start_idx; ++start_idx) {
        const line_t block_mask = ((1U << block_len) - 1) << start_idx;
        const unsigned next_idx = clue_idx + 1 == block->count ? start_idx + block_len : start_idx + block_len + 1;

        next_pattern_idx = generate_pattern_induction(
            puzzle_size, block, clue_idx + 1, next_idx, partial_line | block_mask, pattern_out, next_pattern_idx
        );
    }

    return next_pattern_idx;
}

/**
 * @brief Generate all legal bit-mask patterns for a single row or column clue group.
 * @param dst Destination array receiving generated line patterns.
 * @param puzzle_size Length of the row or column, in cells.
 * @param block The clue group to expand into concrete line patterns.
 * @return Number of generated patterns written to the destination.
 * @details
 *  <p>
 *      A pattern is a line bit mask representing one complete assignment of black cells for a line. Bits set to one are
 *      black/filled cells; bits set to zero are white/empty cells.
 *  </p>
 *  <p>
 *      If the clue group is empty, the only legal pattern is the all-white line, represented by zero. Otherwise, this
 *      function delegates to <code>generate_pattern_induction()</code> to recursively place each clue block in all
 *      legal positions.
 *  </p>
 */
static extent_t generate_pattern(
    line_t dst[MAX_PATTERN_COUNT],
    const extent_t puzzle_size,
    const struct ClueGroup * const block
) {
    if (block == NULL || block->count == 0) {
        dst[0] = 0;
        return 1;
    }

    return generate_pattern_induction(puzzle_size, block, 0, 0, 0, dst, 0);
}

/**
 * @brief Compute all valid patterns for the characterised Puzzle.
 * @param puzzle_extent The puzzle extent, in cells.
 * @param dst The destination pattern array.
 * @param counts The destination counts array.
 * @param clues The populated clue data.
 * @param clue_count The number of clues detained in the given clue data.
 */
static void compute_valid_patterns(
    const extent_t puzzle_extent,
    line_t dst[MAX_SIZE * MAX_PATTERN_COUNT],
    extent_t counts[MAX_SIZE],
    const struct ClueGroup * const clues,
    const unsigned int clue_count
) {
    for (unsigned int clue_idx = 0; clue_idx < clue_count; ++clue_idx)
        counts[clue_idx] = generate_pattern(&dst[clue_idx * MAX_PATTERN_COUNT], puzzle_extent, &clues[clue_idx]);
}

bool solver_initialise_environment() {
    static uint32_t base_addresses[IPCORE_COUNT] = {XPAR_SOLVER_TOPLEVEL_0_BASEADDR, XPAR_SOLVER_TOPLEVEL_1_BASEADDR};

    static uint16_t interrupt_intrs[IPCORE_COUNT] = {
        XPAR_FABRIC_SOLVER_TOPLEVEL_0_INTR, XPAR_FABRIC_SOLVER_TOPLEVEL_1_INTR
    };

    assert(IPCORE_COUNT < 32);
    for (unsigned int core_idx = 0; core_idx < IPCORE_COUNT; ++core_idx)
        if (!ipcore_initialise(&cores[core_idx], base_addresses[core_idx], interrupt_intrs[core_idx], 1U << core_idx))
            return false;

    return true;
}

enum SearchResult solver_solve(
    struct Puzzle * const puzzle_info
)
{
    assert(puzzle_info->width == puzzle_info->height);
    assert(puzzle_info->chunk.clue_group_count == puzzle_info->width + puzzle_info->height);

    memset(row_patterns, 0, sizeof(row_patterns));
    memset(col_patterns, 0, sizeof(col_patterns));
    memset(row_counts, 0, sizeof(row_counts));
    memset(col_counts, 0, sizeof(col_counts));
    memset(out_black, 0, sizeof(out_black));
    memset(out_white, 0, sizeof(out_white));

    // I.a.w. the clues, precompute all valid row and column patterns.

    compute_valid_patterns(
        puzzle_info->width, row_patterns, row_counts, puzzle_info->chunk.clue_data, puzzle_info->width
    );

    compute_valid_patterns(
        puzzle_info->width, col_patterns, col_counts, &puzzle_info->chunk.clue_data[puzzle_info->width],
        puzzle_info->height
    );

    Xil_DCacheFlushRange((UINTPTR)row_patterns, sizeof(row_patterns)); // NOLINT(*-narrowing-conversions)
    Xil_DCacheFlushRange((UINTPTR)row_counts, sizeof(row_counts));     // NOLINT(*-narrowing-conversions)
    Xil_DCacheFlushRange((UINTPTR)col_patterns, sizeof(col_patterns)); // NOLINT(*-narrowing-conversions)
    Xil_DCacheFlushRange((UINTPTR)col_counts, sizeof(col_counts));     // NOLINT(*-narrowing-conversions)

    // Attempt to solve the Nonogram with two-core DFS.

    const enum SearchResult result = search_two_core_dfs(puzzle_info);
    logging_printf("Search completed with status: %d", result);

    // Populate the solution bitmap (set bits indicate black/filled cells).

    xSemaphoreTake(puzzle_info->solution_semaphore, portMAX_DELAY);

    const line_t col_mask = (1U << puzzle_info->width) - 1U;
    for (extent_t row_idx = 0; row_idx < puzzle_info->height; ++row_idx)
        puzzle_info->solution_bitmap[row_idx] = out_black[row_idx] & col_mask;

    xSemaphoreGive(puzzle_info->solution_semaphore);
    puzzle_info->solved_state = result;

    return result;
}
