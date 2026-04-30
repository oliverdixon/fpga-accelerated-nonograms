#include <stdbool.h>
#include <string.h>

#include "solver.h"

#define MAX_ITERATIONS (MAX_SIZE * MAX_SIZE)

enum RefinementResult
{
    REFINEMENT_UNCHANGED,
    REFINEMENT_CHANGED,
    REFINEMENT_CONTRADICTION
};

static line_t produce_line_mask(const extent_t col_idx)
{
    return (1U << col_idx) - 1U;
}

static line_t get_column_mask(
        const line_t * const row_masks,
        const extent_t row_count,
        const extent_t col_idx)
{
    line_t result = 0;

    for (extent_t row_idx = 0; row_idx < row_count; ++row_idx)
        if (row_masks[row_idx] & 1U << col_idx)
            result |= 1U << row_idx;

    return result;
}

static bool refine_line(
        const line_t * const patterns,
        const extent_t pattern_count,
        const extent_t column_extent,
        const line_t known_black,
        const line_t known_white,
        line_t * forced_black,
        line_t * forced_white)
{
    const line_t line_mask = produce_line_mask(column_extent);

    line_t black_disj = 0;
    line_t black_conj = line_mask;
    extent_t valid_count = 0;

    for (unsigned int pattern_idx = 0; pattern_idx < pattern_count; ++pattern_idx) {
        const line_t pattern = patterns[pattern_idx] & line_mask;
        if ((pattern & known_white) == 0 && (pattern & known_black) == known_black) {
            black_disj |= pattern;
            black_conj &= pattern;
            ++valid_count;
        }
    }

    if (valid_count == 0) {
        *forced_black = 0;
        *forced_white = 0;
        return false;
    }

    *forced_black = black_conj;
    *forced_white = ~black_disj & line_mask;
    return true;
}

static bool are_all_cells_known(
        const line_t * const black,
        const line_t * const white,
        const extent_t puzzle_size)
{
    const line_t col_mask = produce_line_mask(puzzle_size);

    for (extent_t row_idx = 0; row_idx < puzzle_size; ++row_idx)
        if (((black[row_idx] | white[row_idx]) & col_mask) != col_mask)
            return false;

    return true;
}

static bool is_line_valid(
        const line_t * const patterns,
        const extent_t pattern_count,
        const extent_t column_extent,
        const line_t known_black,
        const line_t known_white)
{
    const line_t col_mask = produce_line_mask(column_extent);

    for (unsigned int pattern_idx = 0; pattern_idx < pattern_count; ++pattern_idx) {
        const line_t pattern = patterns[pattern_idx] & col_mask;
        if ((pattern & known_white) == 0 && (pattern & known_black) == known_black)
            return true;
    }

    return false;
}

static bool is_board_valid(
        const line_t * const row_patterns,
        const extent_t * row_counts,
        const line_t * const col_patterns,
        const extent_t * const col_counts,
        const extent_t puzzle_size,
        const line_t * const black,
        const line_t * const white)
{
    // Check the rows.
    for (extent_t row_idx = 0; row_idx < MAX_SIZE; ++row_idx)
        if (row_idx < puzzle_size && !is_line_valid(&row_patterns[row_idx * MAX_PATTERN_COUNT],
                row_counts[row_idx], puzzle_size, black[row_idx], white[row_idx]))
            return false;

    // Check the columns (transposed into line masks).
    for (extent_t col_idx = 0; col_idx < MAX_SIZE; ++col_idx)
        if (col_idx < puzzle_size) {
            const line_t known_black = get_column_mask(black, puzzle_size, col_idx);
            const line_t known_white = get_column_mask(white, puzzle_size, col_idx);

            if (!is_line_valid(&col_patterns[col_idx * MAX_PATTERN_COUNT], col_counts[col_idx],
                    puzzle_size, known_black, known_white))
                return false;
        }

    return true;
}

static enum RefinementResult refine_row(
        const line_t * const row_patterns,
        const extent_t row_pattern_count,
        const extent_t puzzle_size,
        line_t * const out_black,
        line_t * const out_white)
{
    line_t forced_black;
    line_t forced_white;

    const line_t old_black = *out_black;
    const line_t old_white = *out_white;

    if (!refine_line(row_patterns, row_pattern_count, puzzle_size, old_black, old_white, &forced_black, &forced_white))
        return REFINEMENT_UNCHANGED;

    *out_black |= forced_black;
    *out_white |= forced_white;

    if (*out_black & *out_white)
        return REFINEMENT_CONTRADICTION;

    return (old_black == *out_black && old_white == *out_white) ? REFINEMENT_UNCHANGED : REFINEMENT_CHANGED;
}

static enum RefinementResult refine_column(
        const line_t * const col_patterns,
        const extent_t col_pattern_count,
        const extent_t col_idx,
        const extent_t puzzle_size,
        line_t * const out_black,
        line_t * const out_white)
{
    const line_t known_black = get_column_mask(out_black, puzzle_size, col_idx);
    const line_t known_white = get_column_mask(out_white, puzzle_size, col_idx);

    line_t forced_black;
    line_t forced_white;

    if (!refine_line(col_patterns, col_pattern_count, puzzle_size, known_black, known_white,
            &forced_black, &forced_white))
        return REFINEMENT_CONTRADICTION;

    enum RefinementResult result = REFINEMENT_UNCHANGED;

    for (extent_t row_idx = 0; row_idx < puzzle_size; ++row_idx) {
        const line_t old_black = out_black[row_idx];
        const line_t old_white = out_white[row_idx];

        if (forced_black & 1U << row_idx)
            out_black[row_idx] |= 1U << col_idx;

        if (forced_white & 1U << row_idx)
            out_white[row_idx] |= 1U << col_idx;

        if (out_black[row_idx] & out_white[row_idx])
            return REFINEMENT_CONTRADICTION;

        if (old_black != out_black[row_idx] || old_white != out_white[row_idx])
            result = REFINEMENT_CHANGED;
    }

    return result;
}

uint32_t solver_toplevel(
        const line_t * const row_patterns,
        const extent_t * const row_counts,
        const line_t * const col_patterns,
        const extent_t * const col_counts,
        const extent_t puzzle_size,
        const line_t * const in_black,
        const line_t * const in_white,
        line_t * const out_black,
        line_t * const out_white)
{
    #pragma HLS INTERFACE m_axi port = row_patterns offset = slave bundle = MAXI depth = MAX_SIZE * MAX_PATTERN_COUNT
    #pragma HLS INTERFACE m_axi port = row_counts offset = slave bundle = MAXI depth = MAX_SIZE
    #pragma HLS INTERFACE m_axi port = col_patterns offset = slave bundle = MAXI depth = MAX_SIZE * MAX_PATTERN_COUNT
    #pragma HLS INTERFACE m_axi port = col_counts offset = slave bundle = MAXI depth = MAX_SIZE
    #pragma HLS INTERFACE m_axi port = in_black offset = slave bundle = MAXI depth = MAX_SIZE
    #pragma HLS INTERFACE m_axi port = in_white offset = slave bundle = MAXI depth = MAX_SIZE
    #pragma HLS INTERFACE m_axi port = out_black offset = slave bundle = MAXI depth = MAX_SIZE
    #pragma HLS INTERFACE m_axi port = out_white offset = slave bundle = MAXI depth = MAX_SIZE

    #pragma HLS INTERFACE s_axilite port = row_patterns bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = row_counts bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = col_patterns bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = col_counts bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = puzzle_size bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = in_black bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = in_white bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = out_black bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = out_white bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = return bundle = AXILiteS

    if (puzzle_size == 0 || puzzle_size > MAX_SIZE)
        return SOLVER_CONTRADICTION;

    line_t black[MAX_SIZE];
    line_t white[MAX_SIZE];

    memcpy(black, in_black, MAX_SIZE * sizeof(line_t));
    memcpy(white, in_white, MAX_SIZE * sizeof(line_t));

    for (unsigned int iter = 0; iter < MAX_ITERATIONS; ++iter) {
        bool changed = false;

        // Refine the rows.
        for (extent_t row_idx = 0; row_idx < puzzle_size; ++row_idx) {
            const enum RefinementResult result = refine_row(&row_patterns[row_idx * MAX_PATTERN_COUNT],
                row_counts[row_idx], puzzle_size, &black[row_idx], &white[row_idx]);

            if (result == REFINEMENT_CONTRADICTION)
                return SOLVER_CONTRADICTION;

            if (result == REFINEMENT_CHANGED)
                changed = true;
        }

        // Refine the columns.
        for (extent_t col_idx = 0; col_idx < puzzle_size; ++col_idx) {
            const enum RefinementResult result = refine_column(&col_patterns[col_idx * MAX_PATTERN_COUNT],
                col_counts[col_idx], col_idx, puzzle_size, black, white);

            if (result == REFINEMENT_CONTRADICTION)
                return SOLVER_CONTRADICTION;

            if (result == REFINEMENT_CHANGED)
                changed = true;
        }

        if (!changed)
            // The solver has converged, so jump out.
            break;
    }

    memcpy(out_black, black, MAX_SIZE * sizeof(line_t));
    memcpy(out_white, white, MAX_SIZE * sizeof(line_t));

    /*
     * If some cells are unknown, we're stuck. If all cells are assigned but the board isn't compliant with one of the
     * precomputed patterns, then we have a contradiction.
     */
    const bool are_cells_known = are_all_cells_known(black, white, puzzle_size);
    if (are_cells_known &&
            is_board_valid(row_patterns, row_counts, col_patterns, col_counts, puzzle_size, black, white))
        return SOLVER_OK;

    return are_cells_known ? SOLVER_CONTRADICTION : SOLVER_STUCK;
}
