#include "solver.h"

uint32_t solver_toplevel(
        const line_t * const row_patterns,
        const extent_t * const row_counts,
        const line_t * const col_patterns,
        const extent_t * const col_counts,
        const extent_t puzzle_size,
        line_t * const out_black,
        line_t * const out_white)
{
    #pragma HLS INTERFACE m_axi port = row_patterns offset = slave bundle = MAXI depth = MAX_SIZE * MAX_PATTERN_COUNT
    #pragma HLS INTERFACE m_axi port = row_counts offset = slave bundle = MAXI depth = MAX_SIZE
    #pragma HLS INTERFACE m_axi port = col_patterns offset = slave bundle = MAXI depth = MAX_SIZE * MAX_PATTERN_COUNT
    #pragma HLS INTERFACE m_axi port = col_counts offset = slave bundle = MAXI depth = MAX_SIZE
    #pragma HLS INTERFACE m_axi port = out_black offset = slave bundle = MAXI depth = MAX_SIZE
    #pragma HLS INTERFACE m_axi port = out_white offset = slave bundle = MAXI depth = MAX_SIZE

    #pragma HLS INTERFACE s_axilite port = row_patterns bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = row_counts bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = col_patterns bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = col_counts bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = puzzle_size bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = out_black bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = out_white bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = return bundle = AXILiteS

    line_t black[MAX_SIZE];
    line_t white[MAX_SIZE];

    #pragma HLS ARRAY_PARTITION variable = black complete
    #pragma HLS ARRAY_PARTITION variable = white complete

    for (extent_t row_idx = 0; row_idx < puzzle_size; ++row_idx) {
        black[row_idx] = 0;
        white[row_idx] = 0;
    }

    // TODO: algorithm.

    return puzzle_size;
}
