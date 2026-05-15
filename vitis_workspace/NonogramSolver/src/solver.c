#include <assert.h>
#include <string.h>
#include <xil_cache.h>
#include <xil_printf.h>
#include <xil_types.h>
#include <xsolver_toplevel.h>

#include "solver.h"
#include "chunks.h"
#include "ipcore.h"
#include "logging.h"
#include "puzzle.h"

#define MAX_PATTERN_COUNT (5005)
#define MAX_SEARCH_DEPTH (MAX_SIZE * MAX_SIZE)
#define IPCORE_COUNT (2)

static struct IPCore cores[IPCORE_COUNT];

struct CellRef
{
    extent_t row;
    extent_t col;
    bool valid;
};

static line_t row_patterns[MAX_SIZE * MAX_PATTERN_COUNT];
static line_t col_patterns[MAX_SIZE * MAX_PATTERN_COUNT];
static extent_t row_counts[MAX_SIZE];
static extent_t col_counts[MAX_SIZE];

static line_t out_black[MAX_SIZE];
static line_t out_white[MAX_SIZE];

static struct CellRef choose_unknown(
    const line_t *const black,
    const line_t *const white,
    const extent_t puzzle_extent
)
{
    struct CellRef choice = {.valid = false};

    for (extent_t row_idx = 0; row_idx < puzzle_extent; ++row_idx) {
        const line_t known = black[row_idx] | white[row_idx];
        for (extent_t col_idx = 0; col_idx < puzzle_extent; ++col_idx) {
            const line_t col_mask = 1U << col_idx;
            if ((known & col_mask) == 0) {
                choice.valid = true;
                choice.row = row_idx;
                choice.col = col_idx;

                assert((known & col_mask) == 0);
                return choice;
            }
        }
    }

    return choice;
}

static enum SolverState run_core_sync(
    struct IPCore *const core,
    const struct Puzzle *const puzzle_info,
    const line_t *const in_black,
    const line_t *const in_white
)
{
    const size_t board_bytes = puzzle_info->width * sizeof(line_t);

    memcpy(core->in_black, in_black, board_bytes);
    memcpy(core->in_white, in_white, board_bytes);

    ipcore_execute(core, puzzle_info, row_patterns, row_counts, col_patterns, col_counts);

    enum SolverState status;
    while ((status = ipcore_finish(core, puzzle_info)) == SOLVER_UNFINISHED);

    return status;
}

static void start_core_job(
    struct IPCore *const core,
    const struct Puzzle *const puzzle_info,
    const struct SearchJob *const job
)
{
    const size_t board_bytes = puzzle_info->width * sizeof(line_t);

    core->job = *job;

    memcpy(core->in_black, job->black, board_bytes);
    memcpy(core->in_white, job->white, board_bytes);

    ipcore_execute(core, puzzle_info, row_patterns, row_counts, col_patterns, col_counts);
    core->busy = true;
}

static void wait_for_cores(
    struct IPCore *const core_1,
    struct IPCore *const core_2,
    const struct Puzzle *const puzzle_info,
    enum SolverState *const a_status,
    enum SolverState *const b_status
)
{
    *a_status = SOLVER_UNFINISHED;
    *b_status = SOLVER_UNFINISHED;

    while (*a_status == SOLVER_UNFINISHED || *b_status == SOLVER_UNFINISHED) {
        if (*a_status == SOLVER_UNFINISHED)
            *a_status = ipcore_finish(core_1, puzzle_info);

        if (*b_status == SOLVER_UNFINISHED)
            *b_status = ipcore_finish(core_2, puzzle_info);
    }
}

static void make_propagated_job_from_core(
    struct SearchJob *const job,
    const struct IPCore *const core,
    const struct Puzzle *const puzzle_info
)
{
    const size_t board_bytes = puzzle_info->width * sizeof(line_t);

    memcpy(job->black, core->out_black, board_bytes);
    memcpy(job->white, core->out_white, board_bytes);

    job->depth = core->job.depth;
    job->propagated = true;
}

static void explore_binary_children(
    const struct SearchJob *const current_job,
    const struct CellRef *const choice,
    const struct Puzzle *const puzzle_info,
    const line_t *const propagated_black,
    const line_t *const propagated_white,
    enum SolverState *const black_status,
    enum SolverState *const white_status
)
{
    struct SearchJob black_child = {
        .propagated = false,
        .depth = current_job->depth + 1
    };

    struct SearchJob white_child = {
        .propagated = false,
        .depth = current_job->depth + 1
    };

    const size_t board_bytes = puzzle_info->width * sizeof(line_t);
    memcpy(black_child.black, propagated_black, board_bytes);
    memcpy(black_child.white, propagated_white, board_bytes);
    memcpy(white_child.black, propagated_black, board_bytes);
    memcpy(white_child.white, propagated_white, board_bytes);

    const line_t mask = (line_t) 1U << choice->col;
    black_child.black[choice->row] |= mask;
    white_child.white[choice->row] |= mask;

    *black_status = SOLVER_UNFINISHED;
    *white_status = SOLVER_UNFINISHED;

    start_core_job(
        &cores[0],
        puzzle_info,
        &black_child
    );

    start_core_job(
        &cores[1],
        puzzle_info,
        &white_child
    );

    wait_for_cores(
        &cores[0],
        &cores[1],
        puzzle_info,
        black_status,
        white_status
    );
}

static enum SearchResult search_two_core_dfs(
    const struct Puzzle *const puzzle_info
)
{
    const size_t board_bytes = puzzle_info->width * sizeof(line_t);
    bool saw_unknown = false;

    struct SearchJob current = {
        .propagated = false,
        .depth = 0
    };

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

            if (!ipcore_dequeue_job(&current))
                return SEARCH_UNKNOWN;

            continue;
        }

        enum SolverState status;
        line_t propagated_black[MAX_SIZE];
        line_t propagated_white[MAX_SIZE];

        if (current.propagated) {
            // If this job came directly from a previous SOLVER_STUCK result, it is already propagated.
            memcpy(propagated_black, current.black, board_bytes);
            memcpy(propagated_white, current.white, board_bytes);
            status = SOLVER_STUCK;
        } else {
            // Otherwise, allocate a core to run the job.
            status = run_core_sync(
                &cores[0],
                puzzle_info,
                current.black,
                current.white
            );

            if (status == SOLVER_OK || status == SOLVER_STUCK) {
                memcpy(propagated_black, cores[0].out_black, board_bytes);
                memcpy(propagated_white, cores[0].out_white, board_bytes);
            }
        }

        // Base case: if the job derived a contradiction, dequeue it and report back up the chain.
        if (status == SOLVER_CONTRADICTION) {
            if (!ipcore_dequeue_job(&current))
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
         * Inductive case: if the solver reports SOLVER_STUCK, spawn a couple of new jobs for each branch at the fixed
         * point and allocate one to each solver IP core.
         */
        const struct CellRef choice =
                choose_unknown(propagated_black, propagated_white, puzzle_info->width);

        if (!choice.valid) {
            /*
             * If there's no valid choice, no fixed point was reached. We didn't reach an explicit contradiction in the
             * clue data, but this branch is useless.
             */
            saw_unknown = true;
            if (!ipcore_dequeue_job(&current))
                return SEARCH_UNKNOWN;

            continue;
        }

        const line_t mask = (line_t) 1U << choice.col;
        if ((propagated_black[choice.row] | propagated_white[choice.row]) & mask) {
            saw_unknown = true;

            if (!ipcore_dequeue_job(&current))
                return SEARCH_UNKNOWN;

            continue;
        }

        enum SolverState black_status = SOLVER_UNFINISHED;
        enum SolverState white_status = SOLVER_UNFINISHED;
        explore_binary_children(&current, &choice, puzzle_info, propagated_black, propagated_white, &black_status,
                                &white_status);

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
            make_propagated_job_from_core(&black_next, &cores[0], puzzle_info);

        if (white_stuck)
            make_propagated_job_from_core(&white_next, &cores[1], puzzle_info);

        // Continue depth-first. If both are stuck, continue with the black branch and defer white.
        if (black_stuck && white_stuck) {
            if (!ipcore_enqueue_job(&white_next))
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
        if (!ipcore_dequeue_job(&current))
            return saw_unknown ? SEARCH_UNKNOWN : SEARCH_FAILED;
    }
}

static unsigned int min_required_tail(
    const struct ClueData *const block,
    const unsigned int start_idx
)
{
    if (start_idx >= block->count)
        return 0;

    unsigned int sum = 0;
    for (unsigned int idx = start_idx; idx < block->count; ++idx)
        sum += block->blocks[idx];

    sum += block->count - start_idx - 1;
    return sum;
}

static extent_t generate_pattern_induction(
    const extent_t puzzle_size,
    const struct ClueData *const block,
    const unsigned int clue_idx,
    const unsigned int min_start_idx,
    const line_t partial_line,
    line_t *const pattern_out,
    extent_t next_pattern_idx
)
{
    if (clue_idx == block->count) {
        // Base case: we've reached the end of the clues, so commit our current line.
        pattern_out[next_pattern_idx++] = partial_line;
        return next_pattern_idx;
    }

    const unsigned int block_len =
            block->blocks[clue_idx]; // The length of the target continuous block.
    const unsigned int latest_start_idx =
            puzzle_size - block_len - min_required_tail(block, clue_idx + 1);

    for (unsigned int start_idx = min_start_idx; start_idx <= latest_start_idx; ++start_idx) {
        const line_t block_mask = ((1U << block_len) - 1) << start_idx;
        const unsigned next_idx =
                clue_idx + 1 == block->count ? start_idx + block_len : start_idx + block_len + 1;

        next_pattern_idx = generate_pattern_induction(
            puzzle_size, block, clue_idx + 1, next_idx, partial_line | block_mask, pattern_out,
            next_pattern_idx
        );
    }

    return next_pattern_idx;
}

static extent_t generate_pattern(
    line_t dst[MAX_SIZE],
    const extent_t puzzle_size,
    const struct ClueData *const block
)
{
    if (block->count == 0) {
        dst[0] = 0;
        return 1;
    }

    return generate_pattern_induction(puzzle_size, block, 0, 0, 0, dst, 0);
}

static void compute_valid_patterns(
    const extent_t puzzle_size,
    line_t dst[MAX_SIZE * MAX_PATTERN_COUNT],
    extent_t counts[MAX_SIZE],
    const struct ClueData *const clues,
    const unsigned int clue_count
)
{
    for (unsigned int clue_idx = 0; clue_idx < clue_count; ++clue_idx)
        counts[clue_idx] =
                generate_pattern(&dst[clue_idx * MAX_PATTERN_COUNT], puzzle_size, &clues[clue_idx]);
}

void solver_initialise_environment()
{
    static uint32_t base_addresses[IPCORE_COUNT] = {
        XPAR_XSOLVER_TOPLEVEL_0_BASEADDR, XPAR_SOLVER_TOPLEVEL_1_BASEADDR
    };

    for (unsigned int core_idx = 0; core_idx < IPCORE_COUNT; ++core_idx)
        assert(ipcore_initialise(&cores[core_idx], base_addresses[core_idx]));
}

void solver_solve(
    struct Puzzle *const puzzle_info
)
{
    assert(puzzle_info->width == puzzle_info->height);
    assert(puzzle_info->chunk.clue_count == puzzle_info->width + puzzle_info->height);

    memset(row_patterns, 0, sizeof(row_patterns));
    memset(col_patterns, 0, sizeof(col_patterns));
    memset(row_counts, 0, sizeof(row_counts));
    memset(col_counts, 0, sizeof(col_counts));
    memset(out_black, 0, sizeof(out_black));
    memset(out_white, 0, sizeof(out_white));

    // I.a.w. the clues, precompute all valid row and column patterns.

    compute_valid_patterns(
        puzzle_info->width, row_patterns, row_counts, puzzle_info->chunk.clue_data,
        puzzle_info->width
    );

    compute_valid_patterns(
        puzzle_info->width, col_patterns, col_counts,
        &puzzle_info->chunk.clue_data[puzzle_info->width], puzzle_info->height
    );

    Xil_DCacheFlushRange((UINTPTR) row_patterns, sizeof(row_patterns));
    Xil_DCacheFlushRange((UINTPTR) row_counts, sizeof(row_counts));
    Xil_DCacheFlushRange((UINTPTR) col_patterns, sizeof(col_patterns));
    Xil_DCacheFlushRange((UINTPTR) col_counts, sizeof(col_counts));

    // Attempt to solve the Nonogram with two-core DFS.

    const enum SearchResult result = search_two_core_dfs(puzzle_info);
    logging_printf("Search completed with status: %d", result);

    // Populate the solution bitmap.

    xSemaphoreTake(puzzle_info->solution_semaphore, portMAX_DELAY);

    const line_t col_mask = (1U << puzzle_info->width) - 1U;
    for (extent_t row_idx = 0; row_idx < puzzle_info->height; ++row_idx)
        puzzle_info->solution_bitmap[row_idx] = out_black[row_idx] & col_mask;

    xSemaphoreGive(puzzle_info->solution_semaphore);
    puzzle_info->solved_state = result;
}
