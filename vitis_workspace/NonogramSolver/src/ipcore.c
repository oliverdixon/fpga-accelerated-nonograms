#include "ipcore.h"

bool ipcore_initialise(struct IPCore * const ipcore, const uint32_t base_address)
{
    ipcore->busy = true;
    return XSolver_toplevel_Initialize(&ipcore->solver, base_address);
}
