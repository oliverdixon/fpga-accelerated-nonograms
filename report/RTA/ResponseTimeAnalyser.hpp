/**
 * @file
 * @brief Response-Time Algorithm interface and implementation
 * @date 2026-05-10
 * @author Exam Candidate Y3898772
 */

#ifndef RTA_RESPONSETIMEANALYSER_HPP
#define RTA_RESPONSETIMEANALYSER_HPP

#include <cassert>
#include <set>

#include "Task.hpp"

namespace RTA
{

/**
 * @class ResponseTimeAnalyser
 * @brief Collects RTA algorithms templated on the RM/DM ordering type.
 */
class ResponseTimeAnalyser
{
public:
    /**
     * @brief Use RTA to determine whether or not a Task set can be scheduled.
     * @tparam Comparator The comparator used to induce the relative priority of
     *  tasks.
     * @param tasks The priority-ordered set of Task objects.
     * @param ostream The optional output buffer for trace information, or
     *  <code>nullptr</code>.
     * @return Can the given Task set be scheduled?
     */
    template<typename Comparator>
    static bool rta(const std::set<Task, Comparator> &tasks,
                    std::ostream *const ostream = nullptr)
    {
        bool is_schedulable = true;

        for (auto task_it = tasks.begin(); task_it != tasks.end(); ++task_it) {
            const auto &task = *task_it;
            unsigned int current_wi = task.get_wcet();
            const std::string_view outer_step = task.name;

            for (unsigned int step = 0;; ++step) {
                /*
                 * Compute w_i^{(n+1)} = C_i + \sum_{j\in\hp{i}} \left\lceil
                 * \frac{w_i^{(n)}}{T_j} C_j \right\rceil.
                 */
                unsigned int next_wi = task.get_wcet();
                for (auto hp_it = tasks.begin(); hp_it != task_it; ++hp_it)
                    next_wi += ceil_div(current_wi, hp_it->period) *
                               hp_it->get_wcet();

                if (ostream != nullptr)
                    *ostream << std::format("Weight for step {}.{} is {}.\n",
                                            outer_step, step, next_wi);

                // If w_i^{(n+1)} = w_i^{(n)}, we're done.
                if (next_wi == current_wi) {
                    task.response_time = current_wi;
                    if (ostream != nullptr)
                        *ostream << std::format("Task {} converged on weight "
                                                "{} on step {}.{}.\n",
                                                task.name, current_wi,
                                                outer_step, step);
                    break;
                }

                // If w_i^{(n+1)} > T_i, we've missed our deadline.
                if (next_wi > task.deadline) {
                    if (ostream != nullptr)
                        *ostream << std::format(
                                "Task {} missed its deadline of "
                                "{}ms on step {}.{}.\n",
                                task.name, task.deadline, outer_step, step);
                    is_schedulable = false;
                    break;
                }

                current_wi = next_wi;
            }
        }

        return is_schedulable;
    }

    /**
     * @brief Maximise the WCET of the given Task in the Task set using brute
     *  force repeated application of RTA.
     * @tparam Comparator The comparator used to induce the relative priority of
     *  tasks.
     * @param tasks The priority-ordered Task set.
     * @param maximise_task The Task whose WCET to maximise.
     * @param ostream The mandatory output stream buffer to print trace
     *  information.
     * @return The maximised WCET.
     */
    template<typename Comparator>
    static unsigned int maximise_wcet(std::set<Task, Comparator> &tasks,
                                      const Task &maximise_task,
                                      std::ostream &ostream)
    {
        const auto initial_wcet = maximise_task.get_wcet();
        auto target_task_it = tasks.find(maximise_task);
        assert(target_task_it != tasks.end());

        const unsigned int deadline = target_task_it->deadline;
        unsigned int wcet = 0;

        for (; wcet < deadline; ++wcet) {
            /*
             * Replace the task's WCET with the new candidate and see if RTA
             * indicates that it's still schedulable.
             */
            auto node = tasks.extract(target_task_it);
            node.value().set_wcet(wcet);
            auto [inserted_task, success, _] = tasks.insert(std::move(node));
            assert(success);
            target_task_it = inserted_task;

            if (!rta(tasks)) {
                // We've reached the WCET upper bound.
                ostream << std::format("{} cannot be scheduled with WCET {}.\n",
                                       target_task_it->name, wcet);
                --wcet;
                break;
            }

            ostream << std::format("{} can be scheduled with WCET {}.\n",
                                   target_task_it->name, wcet);
        }

        // Reset the WCET to the original.
        auto node = tasks.extract(target_task_it);
        node.value().set_wcet(initial_wcet);
        auto [inserted_task, success, _] = tasks.insert(std::move(node));
        assert(success);

        return wcet;
    }

private:
    /**
     * @brief Perform integer ceiling division, i.e. @f$ \lceil x/y \rceil @f$.
     * @param x The dividend
     * @param y The divisor
     * @return The result of the expression @f$ \lceil x/y \rceil @f$.
     */
    static constexpr unsigned int ceil_div(const unsigned int x,
                                           const unsigned int y) noexcept
    {
        return (x + y - 1) / y;
    }
};

} // namespace RTA

#endif // RTA_RESPONSETIMEANALYSER_HPP
