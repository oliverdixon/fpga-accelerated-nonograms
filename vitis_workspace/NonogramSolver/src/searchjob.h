/**
 * @file
 * @brief DFS search job interface
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#ifndef SEARCHJOB_H
#define SEARCHJOB_H

#include "../../SolverCore/src/solver_params.h"

/**
 * @struct SearchJob
 * @brief Represents the state of a single job assigned to an IP core during the dual-core DFS procedure.
 */
struct SearchJob
{
    line_t black[MAX_SIZE] __attribute__((aligned(64))); /**< @brief Black cell assignments */
    line_t white[MAX_SIZE] __attribute__((aligned(64))); /**< @brief White cell assignments */
    unsigned int depth; /**< @brief Depth of the job in the DFS tree */
    bool propagated; /**< @brief Indicates propagation state of the job */
};

struct Puzzle;

/**
 * @brief Populate the given job as a child of the given parent job.
 * @param job The target IP core management structure.
 * @param parent_job The parent of the job being populated.
 * @param puzzle_info The information of the Puzzle being solved.
 * @param inherited_black Inherited black cell assignments from the parent DFS search node.
 * @param inherited_white Inherited white cell assignments from the parent DFS search node.
 * @param already_propagated Has the given job already been propagated?
 */
void searchjob_populate(
    struct SearchJob *job,
    const struct SearchJob * parent_job,
    const struct Puzzle * puzzle_info,
    const line_t * inherited_black,
    const line_t * inherited_white,
    bool already_propagated
);

/**
 * @brief Enqueue the given job into the internal deferred work queue.
 * @param job The job to enqueue.
 * @return Was the given job enqueued?
 */
bool searchjob_enqueue(const struct SearchJob * job);

/**
 * @brief Dequeue the topmost job from the internal deferred work queue into the given node.
 * @param job The destination node for the topmost deferred job.
 * @return Was the job dequeued into the destination node?
 */
bool searchjob_dequeue(struct SearchJob * job);

#endif // SEARCHJOB_H
