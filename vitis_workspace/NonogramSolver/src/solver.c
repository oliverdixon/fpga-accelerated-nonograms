#include <assert.h>
#include <string.h>
#include <xil_cache.h>
#include <xil_printf.h>
#include <xil_types.h>
#include <xsolver_toplevel.h>

#include "chunks.h"
#include "ipcore.h"
#include "logging.h"
#include "puzzle.h"
#include "solver.h"

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

static enum SearchResult search(
    const struct Puzzle * const puzzle_info,
    const line_t * row_patterns,
    const extent_t * row_counts,
    const line_t * col_patterns,
    const extent_t * col_counts,
    line_t * black,
    line_t * white,
    const unsigned int depth
);

static void print_board(
    const line_t black[MAX_SIZE],
    const line_t white[MAX_SIZE],
    const extent_t puzzle_size
) {
    for (extent_t row_idx = 0; row_idx < puzzle_size; ++row_idx) {
        for (extent_t col_idx = 0; col_idx < puzzle_size; ++col_idx) {
            const line_t mask = 1U << col_idx;

            if (black[row_idx] & mask)
                outbyte('#');
            else if (white[row_idx] & mask)
                outbyte('.');
            else
                outbyte('?');
        }

        print("\r\n");
    }
}

struct CellRef choose_unknown(
    const line_t * const black,
    const line_t * const white,
    const extent_t puzzle_extent
) {
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

static enum SearchResult search_branch(
    const struct Puzzle * const puzzle_info,
    const line_t * const row_patterns,
    const extent_t * const row_counts,
    const line_t * const col_patterns,
    const extent_t * const col_counts,
    const line_t * const in_black,
    const line_t * const in_white,
    line_t * const out_black,
    line_t * const out_white,
    const unsigned int depth
) {
    line_t propagated_black[MAX_SIZE];
    line_t propagated_white[MAX_SIZE];

    memcpy(propagated_black, in_black, puzzle_info->width * sizeof(line_t));
    memcpy(propagated_white, in_white, puzzle_info->width * sizeof(line_t));

    const enum SearchResult result = search(
        puzzle_info, row_patterns, row_counts, col_patterns, col_counts, propagated_black,
        propagated_white, depth + 1
    );

    if (result == SEARCH_SOLVED) {
        memcpy(out_black, propagated_black, puzzle_info->width * sizeof(line_t));
        memcpy(out_white, propagated_white, puzzle_info->width * sizeof(line_t));
        return SEARCH_SOLVED;
    }

    return result;
}

static enum SearchResult search(
    const struct Puzzle * const puzzle_info,
    const line_t * const row_patterns,
    const extent_t * const row_counts,
    const line_t * const col_patterns,
    const extent_t * const col_counts,
    line_t * const black,
    line_t * const white,
    const unsigned int depth
) {
    static unsigned int max_seen_depth;

    if (depth > max_seen_depth) {
        max_seen_depth = depth;
        logging_printf("Searcher saw new maximum depth of %d.", depth);
    }

    if (depth > MAX_SEARCH_DEPTH)
        return SEARCH_UNKNOWN;

    line_t propagated_black[MAX_SIZE];
    line_t propagated_white[MAX_SIZE];

    const extent_t line_size = sizeof(line_t) * puzzle_info->width;

    // Feed current search state into the solver core.
    memcpy(cores[0].in_black, black, line_size);
    memcpy(cores[0].in_white, white, line_size);

    ipcore_execute(&cores[0], puzzle_info, row_patterns, row_counts, col_patterns, col_counts);

    enum SolverState status = SOLVER_UNFINISHED;
    while ((status = ipcore_finish(&cores[0], puzzle_info)) == SOLVER_UNFINISHED)
        ;

    // Read propagated result from the solver core.
    memcpy(propagated_black, cores[0].out_black, line_size);
    memcpy(propagated_white, cores[0].out_white, line_size);

    if (status != SOLVER_STUCK) {
        memcpy(black, propagated_black, line_size);
        memcpy(white, propagated_white, line_size);

        switch (status) {
        case SOLVER_CONTRADICTION:
            return SEARCH_FAILED;

        case SOLVER_OK:
            return SEARCH_SOLVED;

        default:
            break;
        }
    }

    const struct CellRef choice =
        choose_unknown(propagated_black, propagated_white, puzzle_info->width);

    if (!choice.valid)
        return SEARCH_UNKNOWN;

    const line_t col_mask = 1U << choice.col;

    if (!choice.valid)
        /*
         * If an unknown cell couldn't be chosen, all cells must have assignments. In this case, one
         * assignment must be wrong, so backtrack.
         */
        return SEARCH_UNKNOWN;

    // Assume the unknown cell is black and recurse.

    assert((propagated_black[choice.row] & col_mask) == 0);
    propagated_black[choice.row] |= col_mask;

    const enum SearchResult black_branch_result = search_branch(
        puzzle_info, row_patterns, row_counts, col_patterns, col_counts, propagated_black,
        propagated_white, black, white, depth
    );

    propagated_black[choice.row] &= ~col_mask;

    if (black_branch_result == SEARCH_SOLVED)
        return SEARCH_SOLVED;

    // If the black assumption didn't yield anything, assume the unknown cell is white and recurse.

    assert((propagated_white[choice.row] & col_mask) == 0);
    propagated_white[choice.row] |= col_mask;

    const enum SearchResult white_branch_result = search_branch(
        puzzle_info, row_patterns, row_counts, col_patterns, col_counts, propagated_black,
        propagated_white, black, white, depth
    );

    propagated_white[choice.row] &= ~col_mask;

    if (white_branch_result == SEARCH_SOLVED)
        return SEARCH_SOLVED;

    /*
     * If neither search yielded something useful, the grid is either definitively unsolvable (if
     * the solver returned a contradiction), or the result is known (happens in case of exceeding
     * stack depth limits).
     */
    if (black_branch_result == SEARCH_UNKNOWN || white_branch_result == SEARCH_UNKNOWN)
        return SEARCH_UNKNOWN;

    return SEARCH_FAILED;
}

static unsigned int min_required_tail(
    const struct ClueData * const block,
    const unsigned int start_idx
) {
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
    const struct ClueData * const block,
    const unsigned int clue_idx,
    const unsigned int min_start_idx,
    const line_t partial_line,
    line_t * const pattern_out,
    extent_t next_pattern_idx
) {
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
    struct ClueData * const block
) {
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
    struct ClueData * const clues,
    const unsigned int clue_count
) {
    for (unsigned int clue_idx = 0; clue_idx < clue_count; ++clue_idx)
        counts[clue_idx] =
            generate_pattern(&dst[clue_idx * MAX_PATTERN_COUNT], puzzle_size, &clues[clue_idx]);
}

void solver_initialise_environment() {
    static uint32_t base_addresses[IPCORE_COUNT] = {
        XPAR_XSOLVER_TOPLEVEL_0_BASEADDR, XPAR_SOLVER_TOPLEVEL_1_BASEADDR
    };

    for (unsigned int core_idx = 0; core_idx < IPCORE_COUNT; ++core_idx)
        assert(ipcore_initialise(&cores[core_idx], base_addresses[core_idx]));
}

void solver_solve(
    struct Puzzle * const puzzle_info
) {
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

    Xil_DCacheFlushRange((UINTPTR)row_patterns, sizeof(row_patterns));
    Xil_DCacheFlushRange((UINTPTR)row_counts, sizeof(row_counts));
    Xil_DCacheFlushRange((UINTPTR)col_patterns, sizeof(col_patterns));
    Xil_DCacheFlushRange((UINTPTR)col_counts, sizeof(col_counts));

    // Attempt to solve the Nonogram.

    const enum SearchResult result = search(
        puzzle_info, row_patterns, row_counts, col_patterns, col_counts, out_black, out_white, 0
    );
    logging_printf("Search completed with status: %d", result);

    print_board(out_black, out_white, puzzle_info->width);

    // Populate the solution bitmap.

    xSemaphoreTake(puzzle_info->solution_semaphore, portMAX_DELAY);

    const line_t col_mask = (1U << puzzle_info->width) - 1U;
    for (extent_t row_idx = 0; row_idx < puzzle_info->height; ++row_idx)
        puzzle_info->solution_bitmap[row_idx] = out_black[row_idx] & col_mask;

    xSemaphoreGive(puzzle_info->solution_semaphore);
    puzzle_info->solved_state = result;
}
