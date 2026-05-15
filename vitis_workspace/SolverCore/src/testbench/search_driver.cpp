/**
 * @file
 * @brief Backtracking and search implementation, intended to be executed by the ARM Cortex.
 * @author Oliver Dixon <od641@york.ac.uk>
 * @date 2026-04-30
 */

#include <cassert>
#include <cstring>
#include <iostream>

#include "search_driver.hpp"

#define MAX_SEARCH_DEPTH (1024)

namespace {

struct CellRef
{
    extent_t row;
    extent_t col;
    bool valid;
};

/**
 * @brief Produce a reference to the first encountered cell without an assignment.
 * @param black Black cell assignments.
 * @param white White cell assignments.
 * @param puzzle_extent Extent of the square puzzle grid.
 * @return A CellRef containing indices of the first-encountered unassigned cell; if the
 *  <code>valid</code> flag is not set, the indices are garbage.
 * @post If the returned CellRef is valid, the row and column indices refer to a puzzle square that
 *  is not assigned.
 */
CellRef choose_unknown(
    const line_t * const black,
    const line_t * const white,
    const extent_t puzzle_extent
) {
    CellRef choice = {.valid = false};

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

/**
 * @brief Helper to recurse down a single branch by preparing and maintaining the propagation lines.
 * @param row_patterns Precomputed valid row patterns.
 * @param row_counts Indexed map for sizes of each row pattern.
 * @param col_patterns Precomputed valid column patterns.
 * @param col_counts Indexed map for sizes of each column pattern.
 * @param puzzle_size The extent of the square puzzle.
 * @param in_black Input black cell assignments.
 * @param in_white Input white cell assignments.
 * @param out_black Output black cell assignments, updated iff the search produced a valid solution.
 * @param out_white Output white cell assignments, updated iff the search produced a valid solution.
 * @param depth Recursion depth: initialise to zero.
 * @return Result of the search down the set branch.
 */
SearchResult search_branch(
    const line_t * const row_patterns,
    const extent_t * const row_counts,
    const line_t * const col_patterns,
    const extent_t * const col_counts,
    const extent_t puzzle_size,
    const line_t * const in_black,
    const line_t * const in_white,
    line_t * const out_black,
    line_t * const out_white,
    const unsigned int depth
) {
    line_t propagated_black[MAX_SIZE];
    line_t propagated_white[MAX_SIZE];

    std::memcpy(propagated_black, in_black, puzzle_size * sizeof(line_t));
    std::memcpy(propagated_white, in_white, puzzle_size * sizeof(line_t));

    const SearchResult result = search(
        row_patterns, row_counts, col_patterns, col_counts, puzzle_size, propagated_black,
        propagated_white, depth + 1
    );

    if (result == SEARCH_SOLVED) {
        std::memcpy(out_black, propagated_black, puzzle_size * sizeof(line_t));
        std::memcpy(out_white, propagated_white, puzzle_size * sizeof(line_t));
        return SEARCH_SOLVED;
    }

    return result;
}

/**
 * @brief Serialise a puzzle.
 * @param black Black cell assignments.
 * @param white White cell assignments.
 * @param puzzle_size The extent of the square puzzle.
 */
void print_board(
    const line_t black[MAX_SIZE],
    const line_t white[MAX_SIZE],
    const extent_t puzzle_size
) {
    for (extent_t row_idx = 0; row_idx < puzzle_size; ++row_idx) {
        for (extent_t col_idx = 0; col_idx < puzzle_size; ++col_idx) {
            const line_t mask = 1U << col_idx;

            if (black[row_idx] & mask)
                std::cout << '#';
            else if (white[row_idx] & mask)
                std::cout << '.';
            else
                std::cout << '?';
        }

        std::cout << '\n';
    }
}

} // namespace

SearchResult search(
    const line_t * const row_patterns,
    const extent_t * const row_counts,
    const line_t * const col_patterns,
    const extent_t * const col_counts,
    const extent_t puzzle_size,
    line_t * const black,
    line_t * const white,
    const unsigned int depth
) {
    if (depth > MAX_SEARCH_DEPTH)
        return SEARCH_UNKNOWN;

    std::cout << "Called with depth " << depth << std::endl;
    print_board(black, white, puzzle_size);

    line_t propagated_black[MAX_SIZE];
    line_t propagated_white[MAX_SIZE];

    // Do an initial solve attempt with the input grid assignments to see if we have a trivial case.

    const auto status = static_cast<enum SolverState>(solver_toplevel(
        row_patterns, row_counts, col_patterns, col_counts, puzzle_size, black, white,
        propagated_black, propagated_white
    ));

    if (status == SOLVER_CONTRADICTION)
        return SEARCH_FAILED;

    if (status == SOLVER_OK) {
        std::memcpy(black, propagated_black, puzzle_size * sizeof(line_t));
        std::memcpy(white, propagated_white, puzzle_size * sizeof(line_t));
        return SEARCH_SOLVED;
    }

    /*
     * If there's no trivial solution, do a search. Use a simple heuristic (first observed cell
     * without an assignment in either black or white) and pivot there: first assume black; reset;
     * then assume white; reset.
     */

    const CellRef choice = choose_unknown(propagated_black, propagated_white, puzzle_size);
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

    const SearchResult first_result = search_branch(
        row_patterns, row_counts, col_patterns, col_counts, puzzle_size, propagated_black,
        propagated_white, black, white, depth
    );

    propagated_black[choice.row] &= ~col_mask;

    if (first_result == SEARCH_SOLVED)
        return SEARCH_SOLVED;

    // If the black assumption didn't yield anything, assume the unknown cell is white and recurse.

    assert((propagated_white[choice.row] & col_mask) == 0);
    propagated_white[choice.row] |= col_mask;

    const SearchResult second_result = search_branch(
        row_patterns, row_counts, col_patterns, col_counts, puzzle_size, propagated_black,
        propagated_white, black, white, depth
    );

    propagated_white[choice.row] &= ~col_mask;

    if (second_result == SEARCH_SOLVED)
        return SEARCH_SOLVED;

    /*
     * If neither search yielded something useful, the grid is either definitively unsolvable (if
     * the solver returned a contradiction), or the result is known (happens in case of exceeding
     * stack depth limits).
     */

    if (first_result == SEARCH_UNKNOWN || second_result == SEARCH_UNKNOWN)
        return SEARCH_UNKNOWN;

    return SEARCH_FAILED;
}
