/**
 * @file
 * @brief Backtracking and search interface, intended to be executed by the ARM Cortex.
 * @author Oliver Dixon <od641@york.ac.uk>
 * @date 2026-04-30
 */

#ifndef SW_PROTOTYPING_SEARCH_DRIVER_HPP
#define SW_PROTOTYPING_SEARCH_DRIVER_HPP

#include "../solver.h"

/**
 * @enum SearchResult
 * @brief The result of an exhaustive search.
 */
enum SearchResult
{
    SEARCH_SOLVED, /**< @brief The search solved the puzzle with satisfactory assignments. */
    SEARCH_FAILED, /**< @brief The search failed due an inconsistent puzzle. */
    SEARCH_UNKNOWN /**< @brief The search couldn't determine a result, typically due to timeout. */
};

/**
 * @brief Attempt to solve the Nonogram described by the given clues using backtracking and search.
 * @param row_patterns Precomputed valid row patterns.
 * @param row_counts Indexed map for sizes of each row pattern.
 * @param col_patterns Precomputed valid column patterns.
 * @param col_counts Indexed map for sizes of each column pattern.
 * @param puzzle_size The extent of the square puzzle.
 * @param black Input/output black cell assignments.
 * @param white Input/output white cell assignments.
 * @param depth Recursive call depth: initialise to zero.
 * @return The result of the search: either the Nonogram was solved (satisfactory assignments),
 *  failed (provable contradiction), or was unknown (due to timeout).
 */
SearchResult search(
    const line_t * row_patterns,
    const extent_t * row_counts,
    const line_t * col_patterns,
    const extent_t * col_counts,
    extent_t puzzle_size,
    line_t * black,
    line_t * white,
    unsigned int depth
);

#endif // SW_PROTOTYPING_SEARCH_DRIVER_HPP
