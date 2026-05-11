/**
 * @file
 * @brief Task description for the Response-Time Algorithm
 * @date 2026-05-10
 * @author Exam Candidate Y3898772
 */

#ifndef RTA_TASK_HPP
#define RTA_TASK_HPP

#include <format>

namespace RTA
{

/**
 * @class Task
 * @brief Represents a single named Task with an interval-defined execution
 * time, period, deadline, and optional response time (to be computed with RTA).
 */
class Task
{
public:
    const std::string name;
    std::pair<unsigned int, unsigned int> exec_time;
    const unsigned int period;
    const unsigned int deadline;

    mutable std::optional<unsigned int> response_time{};

    /**
     * @brief Serialise the Task details to the given output buffer.
     * @param ostream The destination buffer.
     * @param task The Task to serialise.
     * @return The destination buffer, populated with a human-readable
     * description of the Task.
     */
    friend std::ostream &operator<<(std::ostream &ostream, const Task &task)
    {
        ostream << std::format("{:4}\t[{:3}, {:3}]\t{:6}\t{:8}\t", task.name,
                               task.exec_time.first, task.exec_time.second,
                               task.period, task.deadline);

        if (task.response_time.has_value())
            ostream << std::format("{:8}", *task.response_time);
        else
            ostream << " Unknown";

        return ostream;
    }

    /**
     * @brief Prints the header for the Task table to the given output buffer.
     * @param ostream The destination buffer.
     * @return The destination buffer, populated with the Task table header.
     */
    static std::ostream &print_header(std::ostream &ostream)
    {
        ostream << "Task\tExecution\tPeriod\tDeadline\tResponse\n"
                << "----\t---------\t------\t--------\t--------\n";

        return ostream;
    }

    /**
     * @brief Gets the worst-case execution time.
     * @return The worst-case execution time (WCET) of the Task.
     */
    [[nodiscard]] unsigned int get_wcet() const noexcept
    {
        return std::max(exec_time.first, exec_time.second);
    }

    /**
     * @brief Sets the worst-case execution time.
     * @param new_wcet The new WCET.
     */
    void set_wcet(const unsigned int new_wcet) noexcept
    {
        exec_time.second = new_wcet;
    }
};

/**
 * @struct RMTaskComparator
 * @brief Implements a rate-monotonic-scheduling comparator for Task
 */
struct RMTaskComparator
{
    /**
     * @brief Determines if the LHS Task is of lesser priority than the RHS Task
     * @param lhs_task The LHS Task
     * @param rhs_task The RHS Task
     * @return Is the LHS Task of lesser priority than the RHS Task? The name is
     * used to tie-break.
     */
    bool operator()(const Task &lhs_task, const Task &rhs_task) const noexcept
    {
        if (lhs_task.period != rhs_task.period)
            return lhs_task.period < rhs_task.period;

        return lhs_task.name < rhs_task.name;
    }
};

/**
 * @struct DMTaskComparator
 * @brief Implements a deadline-monotonic-scheduling comparator for Task
 */
struct DMTaskComparator
{
    /**
     * @brief Determines if the LHS Task is of lesser priority than the RHS Task
     * @param lhs_task The LHS Task
     * @param rhs_task The RHS Task
     * @return Is the LHS Task of lesser priority than the RHS Task? The name is
     * used to tie-break.
     */
    bool operator()(const Task &lhs_task, const Task &rhs_task) const noexcept
    {
        if (lhs_task.deadline != rhs_task.deadline)
            return lhs_task.deadline < rhs_task.deadline;

        return lhs_task.name < rhs_task.name;
    }
};

} // namespace RTA

#endif // RTA_TASK_HPP
