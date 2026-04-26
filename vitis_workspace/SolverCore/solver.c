#include "solver.h"

uint32_t solver_toplevel(const uint32_t *ram, uint32_t *arg1, uint32_t *arg2, uint32_t *arg3, uint32_t *arg4)
{
    #pragma HLS INTERFACE m_axi port = ram offset = slave bundle = MAXI

    #pragma HLS INTERFACE s_axilite port = ram bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = arg1 bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = arg2 bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = arg3 bundle = AXILiteS
    #pragma HLS INTERFACE s_axilite port = arg4 bundle = AXILiteS

    #pragma HLS INTERFACE s_axilite port = return bundle = AXILiteS

    return *ram * 2;
}
