/**
 * @file
 * @brief Software driver entry point, intended to be executed by the ARM Cortex.
 * @author Oliver Dixon <od641@york.ac.uk>
 * @date 2026-04-30
 */

// ReSharper disable CppDFAConstantParameter

#include "search_driver.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

/**
 * @brief Serialise a puzzle.
 * @param black Black cell assignments.
 * @param white White cell assignments.
 * @param puzzle_size The extent of the square puzzle.
 */
static void print_board(
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

static unsigned int min_required_tail(
    const std::vector<extent_t> & block,
    const unsigned int start_idx
) {
    if (start_idx >= block.size())
        return 0;

    unsigned int sum = 0;
    for (unsigned i = start_idx; i < block.size(); ++i)
        sum += block[i];

    sum += block.size() - start_idx - 1;
    return sum;
}

/**
 * @brief Recursively build all valid patterns for the given clues.
 * @param puzzle_size The extent of the square puzzle.
 * @param block The clue block for the row or column.
 * @param clue_idx The current clue index in the given clues vector.
 * @param min_start_idx
 * @param partial_line The current line being built.
 * @param patterns_out The pattern being built recursively.
 */
static void generate_pattern_induction(
    const extent_t puzzle_size,
    const std::vector<extent_t> & block,
    const unsigned int clue_idx,
    const unsigned int min_start_idx,
    const line_t partial_line,
    std::vector<line_t> & patterns_out
) {
    if (clue_idx == block.size()) {
        // Base case: we've reached the end of the clues, so commit our current line.
        patterns_out.push_back(partial_line);
        return;
    }

    const unsigned int block_len = block[clue_idx]; // The length of the target continuous block.
    const unsigned int latest_start_idx =
        puzzle_size - block_len - min_required_tail(block, clue_idx + 1);

    for (unsigned int start_idx = min_start_idx; start_idx <= latest_start_idx; ++start_idx) {
        const line_t block_mask = ((1U << block_len) - 1) << start_idx;
        const unsigned int next_idx =
            clue_idx + 1 == block.size() ? start_idx + block_len : start_idx + block_len + 1;

        generate_pattern_induction(
            puzzle_size, block, clue_idx + 1, next_idx, partial_line | block_mask, patterns_out
        );
    }
}

/**
 * @brief Base case/entry point for generating pattern permutations based on clues.
 * @param puzzle_size The extent of the square puzzle.
 * @param block The clue block for the row or column.
 * @return All valid patterns i.a.w. the given clues.
 */
static std::vector<line_t> generate_pattern(
    const extent_t puzzle_size,
    const std::vector<extent_t> & block
) {
    std::vector<line_t> pattern;

    if (block.empty()) {
        pattern.push_back(0);
        return pattern;
    }

    generate_pattern_induction(puzzle_size, block, 0, 0, 0, pattern);
    return pattern;
}

static void compute_valid_patterns(
    line_t dst[MAX_SIZE * MAX_PATTERN_COUNT],
    extent_t counts[MAX_SIZE],
    const std::vector<std::vector<extent_t>> & clues,
    const extent_t puzzle_size
) {
    unsigned int idx = 0;

    for (const auto & clue : clues) {
        // Generate all valid patterns for a given clue, check invariants, and copy to the
        // destination.

        const std::vector<line_t> patterns = generate_pattern(puzzle_size, clue);
        assert(patterns.size() <= MAX_PATTERN_COUNT);
        counts[idx] = static_cast<extent_t>(patterns.size());

        for (unsigned int pattern_idx = 0; pattern_idx < patterns.size(); ++pattern_idx)
            dst[idx * MAX_PATTERN_COUNT + pattern_idx] = patterns[pattern_idx];

        ++idx;
    }
}

/**
 * @brief The Nonogram solver prototype driver.
 * @return Zero
 */
int main() {
    constexpr extent_t puzzle_size = 20;

    const std::vector<std::vector<extent_t>> row_clues = {
        {01, 01, 01, 01, 01, 01},
        {01, 01, 01, 02, 01, 01, 01},
        {04, 01, 02, 01, 04},
        {06},
        {
            01,
            01,
            01,
            01,
        },
        {01, 01, 02, 01, 01},
        {01, 01, 02, 04, 02, 01, 01},
        {01, 01, 02, 01, 01},
        {01, 01, 02, 02, 01, 01},
        {
            01,
            02,
            02,
            01,
        },
        {
            01,
            02,
            02,
            01,
        },
        {01, 01, 02, 02, 01, 01},
        {01, 01, 02, 01, 01},
        {01, 01, 02, 04, 02, 01, 01},
        {01, 01, 02, 01, 01},
        {
            01,
            01,
            01,
            01,
        },
        {06},
        {04, 01, 02, 01, 04},
        {01, 01, 01, 02, 01, 01, 01},
        {01, 01, 01, 01, 01, 01}
    };

    const std::vector<std::vector<extent_t>> col_clues = {
        {00},
        {03, 01, 01, 01, 01, 03},
        {
            01,
            01,
            01,
            01,
        },
        {03, 02, 02, 02, 03},
        {01, 01},
        {03, 03},
        {01, 01, 01, 01, 01, 01, 01, 01},
        {01, 01, 01, 04, 01, 01, 01},
        {02, 01, 04, 01, 02},
        {
            03,
            03,
            03,
            03,
        },
        {
            03,
            03,
            03,
            03,
        },
        {02, 01, 04, 01, 02},
        {01, 01, 01, 04, 01, 01, 01},
        {01, 01, 01, 01, 01, 01, 01, 01},
        {03, 03},
        {01, 01},
        {03, 02, 02, 02, 03},
        {01, 01, 01, 01},
        {03, 01, 01, 01, 01, 03},
        {00}
    };

    assert(row_clues.size() == puzzle_size);
    assert(col_clues.size() == puzzle_size);

    static line_t row_patterns[MAX_SIZE * MAX_PATTERN_COUNT];
    static line_t col_patterns[MAX_SIZE * MAX_PATTERN_COUNT];
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
    const SearchResult status = search(
        row_patterns, row_counts, col_patterns, col_counts, puzzle_size, out_black, out_white, 0
    );

    std::cout << "status = " << status << "\n\n";
    print_board(out_black, out_white, puzzle_size);

    return 0;
}
