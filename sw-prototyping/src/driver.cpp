/**
 * @file
 */

// ReSharper disable CppDFAConstantParameter

#include <cassert>
#include <cstring>
#include <iostream>
#include <ranges>
#include <vector>

#include "solver.h"

static unsigned min_required_tail(
        const std::vector<extent_t> &clues,
        const unsigned int start_idx)
{
    if (start_idx >= clues.size())
        return 0;

    unsigned int sum = 0;
    for (unsigned i = start_idx; i < clues.size(); ++i)
        sum += clues[i];

    sum += clues.size() - start_idx - 1;
    return sum;
}

/**
 * @brief Recursively build all valid patterns for the given clues.
 * @param puzzle_size The extent of the square puzzle.
 * @param clues The clues for the row or column.
 * @param clue_idx The current clue index in the given clues vector.
 * @param min_start_idx
 * @param partial_line The current line being built.
 * @param patterns_out The pattern being built recursively.
 */
static void generate_permutations_induction(
        const extent_t puzzle_size,
        const std::vector<extent_t> &clues,
        const unsigned int clue_idx,
        const unsigned int min_start_idx,
        const line_t partial_line,
        std::vector<line_t> &patterns_out)
{
    if (clue_idx == clues.size()) {
        // Base case: we've reached the end of the clues, so commit our current line.
        patterns_out.push_back(partial_line);
        return;
    }

    const unsigned int block_len = clues[clue_idx]; // The length of the target continuous block.
    const unsigned int latest_start_idx = puzzle_size - block_len - min_required_tail(clues, clue_idx + 1);

    for (unsigned int start_idx = min_start_idx; start_idx <= latest_start_idx; ++start_idx) {
        const line_t block_mask = ((1U << block_len) - 1) << start_idx;
        const unsigned next_idx = clue_idx + 1 == clues.size() ? start_idx + block_len : start_idx + block_len + 1;

        generate_permutations_induction(
            puzzle_size,
            clues,
            clue_idx + 1,
            next_idx,
            partial_line | block_mask,
            patterns_out
        );
    }
}

/**
 * @brief Base case/entry point for generating pattern permutations based on clues.
 * @param puzzle_size The extent of the square puzzle.
 * @param clues The clues for the row or column.
 * @return All valid patterns i.a.w. the given clues.
 */
static std::vector<line_t> generate_permutations(
        const extent_t puzzle_size,
        const std::vector<extent_t> &clues)
{
    std::vector<line_t> permutations;

    if (clues.empty()) {
        permutations.push_back(0);
        return permutations;
    }

    generate_permutations_induction(puzzle_size, clues, 0, 0, 0, permutations);
    return permutations;
}

static void compute_valid_patterns(
        line_t dst[MAX_SIZE][MAX_PATTERN_COUNT],
        extent_t counts[MAX_SIZE],
        const std::vector<std::vector<extent_t> > &clues,
        const extent_t puzzle_size)
{
    for (const auto [idx, clue] : std::views::enumerate(clues)) {
        // Generate all valid patterns for a given clue, check invariants, and copy to the destination.

        const std::vector<line_t> patterns = generate_permutations(puzzle_size, clue);
        assert(patterns.size() <= MAX_PATTERN_COUNT);
        counts[idx] = static_cast<extent_t>(patterns.size());

        for (auto [pattern_dst, pattern_src] : std::views::zip(dst[idx], patterns))
            pattern_dst = pattern_src;
    }
}

/**
 * @brief Serialise a puzzle.
 * @param black Black cell assignments.
 * @param white White cell assignments.
 * @param puzzle_size The extent of the square puzzle.
 */
static void print_board(
        const line_t black[MAX_SIZE],
        const line_t white[MAX_SIZE],
        const extent_t puzzle_size)
{
    for (extent_t row_idx = 0; row_idx < puzzle_size; ++row_idx) {
        for (extent_t col_idx = 0; col_idx < puzzle_size; ++col_idx)
            if (const line_t mask = 1U << col_idx; black[row_idx] & mask)
                std::cout << '#';
            else if (white[row_idx] & mask)
                std::cout << '.';
            else
                std::cout << '?';

        std::cout << '\n';
    }
}

/**
 * @brief The Nonogram solver prototype driver.
 * @return Zero
 */
int main()
{
    constexpr extent_t puzzle_size = 5;

    const std::vector<std::vector<extent_t> > row_clues = {
        {0},
        {2, 2},
        {5},
        {2, 2},
        {0}
    };

    const std::vector<std::vector<extent_t> > col_clues = {
        {3},
        {3},
        {1},
        {3},
        {3}
    };

    assert(row_clues.size() == puzzle_size);
    assert(col_clues.size() == puzzle_size);

    static line_t row_patterns[MAX_SIZE][MAX_PATTERN_COUNT];
    static line_t col_patterns[MAX_SIZE][MAX_PATTERN_COUNT];
    static extent_t row_counts[MAX_SIZE];
    static extent_t col_counts[MAX_SIZE];

    static line_t out_black[MAX_SIZE];
    static line_t out_white[MAX_SIZE];

    std::memset(row_patterns, 0, sizeof(row_patterns));
    std::memset(col_patterns, 0, sizeof(col_patterns));
    std::memset(row_counts, 0, sizeof(row_counts));
    std::memset(col_counts, 0, sizeof(col_counts));
    std::memset(out_black, 0, sizeof(out_black));
    std::memset(out_white, 0, sizeof(out_white));

    // I.a.w. the clues, precompute all valid row and column patterns.
    compute_valid_patterns(row_patterns, row_counts, row_clues, puzzle_size);
    compute_valid_patterns(col_patterns, col_counts, col_clues, puzzle_size);

    // ... then solve.
    const SolverState status = solve(
        row_patterns,
        row_counts,
        col_patterns,
        col_counts,
        puzzle_size,
        out_black,
        out_white
    );

    std::cout << "status = " << status << "\n\n";
    print_board(out_black, out_white, puzzle_size);

    return 0;
}
