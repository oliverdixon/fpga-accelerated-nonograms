# FPGA/FreeRTOS Nonogram Solver
## Solution Description
### Board Design
The board design, as viewed in Vivado, contains the following components:
 * ZYNQ7 processing system (dual-core ARM Cortex A9)
 * System reset and clock source
 * HDMI controller group
 * Two HLS line-solvers
 * Three AXI SmartConnects
   * Control SMC for slave signals to the solvers and HDMI controller;
   * Data SMC for master data exchanges between the first solver and HDMI
     controller; and
   * Data SMC for the second solver.
 * Inline concatenator for combining interrupt signals from the solvers into the
   ZYNQ7 fabric interrupt port.

The ZYNQ7 PS has two high-performance AXI interfaces to support data
transactions with each of the data AXI SMCs and a single master port for the
control SMC. It also has one EMIO timer to satisfy FreeRTOS requirements.

![A screenshot of the Vivado board design](https://github.com/oliverdixon/embs-assessment/blob/master/NonogramHW/board_design.png?raw=true)

### HLS Component
The HLS component refines the assignments for a single line until one of the
following conditions is met:
 * The solver converges on a black/white cell assignment;
 * The solver encounters a contradiction where a cell has been allocated both
   black and white; or
 * The solver exceeds the maximum number of allowed iterations.

During each iteration, the solver refines all rows followed by all columns. In
the latter case, columns are transposed and refined as rows. During refinement
of a single line, brute-force constraint propagation computes which cells which
are black in at least one valid pattern, and which cells are black in every
valid pattern, to only refine cells which are not already known to be forced to
a particular assignment.

The HLS IP core makes use of parallelism where possible. The line buffers are
annotated with `ARRAY_PARTITION` to allow parallel access to distinct regions of
the buffers, at the cost of slightly increased LUT and FF usage on the
synthesised design. The `ALLOCATION` directive is used on some HLS functions to
reduce hardware utilisation.

### Software Drivers
The ARM software driver and HLS test bench use an inductive (recursive) method
to precompute all valid patterns for all valid rows and columns. As the memory
required to store all valid pattern combinations is large in the maximum size of
the puzzle, a separate 600MiB section is defined by the linker script such that
the arenas can be statically allocated.

Following pattern computation, both drivers execute a depth-first search (DFS)
algorithm on the problem space. The DFS procedure begins by attempting an
initial HLS invocation, which will successfully derive assignments for all
Easy-tier and some Medium-tier puzzles. For other Medium-tier and all Hard-tier
puzzles, the HLS component will not be able to analytically derive assignments
for all cells on the first invocation: it requires a guess.

The software drivers find the first cell without an assignment (this a poor
heuristic) and duplicate the problem: one with the unassigned cell set to white,
and one with the unassigned cell set to black; the solvers are then executed on
each branch to derive a solution. This search function executes recursively, and
if the line-refiner reports a contradiction, this is propagated down the call
stack to prune invalid deductions.

Given the multiple HLS solvers available on the FPGA, the ARM driver allocates
each branch to a different core on the board. As each HLS core maintains its own
set of cell allocations, this allows the driver explore two differing branches
in parallel. If a single branch reports a contradiction, the driver backtracks
and iterates through the *deferred work* to explore sibling nodes of the failed
branch. 

Unique to the ARM driver is also the use of interrupts: instead of polling the
completion state of the HLS component, the Xilinx interrupt API is used to
install an interrupt service routine (ISR) which notifies the suspended FreeRTOS
solver task on completion of an HLS core. Once both have completed, the
assignments are reduced on the ARM and DFS continues as usual.

## Sample Screenshot
![A screenshot of a solved 20x20 Nonogram](https://github.com/oliverdixon/embs-assessment/blob/master/nonogram_screenshots/regular_22x22.jpg?raw=true)
