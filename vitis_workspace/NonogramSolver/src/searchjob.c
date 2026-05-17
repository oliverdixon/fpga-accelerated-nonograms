/**
 * @file
 * @brief DFS search job implementation
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "puzzle.h"
#include "searchjob.h"

/**
 * @brief Deferred work queue for the backtracking DFS, bounded above by the maximum search depth.
 */
static struct
{
    struct SearchJob jobs[MAX_SIZE * MAX_SIZE + 1]; /**< @brief Deferred work */
    extent_t count;                                 /**< @brief Number of deferred jobs */
} pending_jobs = {.count = 0};

void searchjob_populate(
    struct SearchJob * const job,
    const struct SearchJob * const parent_job,
    const struct Puzzle * const puzzle_info,
    const line_t * const inherited_black,
    const line_t * const inherited_white,
    const bool already_propagated
) {
    const size_t board_bytes = puzzle_info->width * sizeof(line_t);

    job->propagated = already_propagated;
    job->depth = parent_job->depth + (already_propagated ? 0 : 1);

    memcpy(job->black, inherited_black, board_bytes);
    memcpy(job->white, inherited_white, board_bytes);
}

bool searchjob_enqueue(
    const struct SearchJob * const job
) {
    if (pending_jobs.count >= (MAX_SIZE * MAX_SIZE) + 1)
        return false;

    pending_jobs.jobs[pending_jobs.count++] = *job;
    return true;
}

bool searchjob_dequeue(
    struct SearchJob * const job
) {
    if (pending_jobs.count == 0)
        return false;

    *job = pending_jobs.jobs[--pending_jobs.count];
    return true;
}
