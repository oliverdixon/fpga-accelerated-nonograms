/**
 * @file
 * @brief Driver for the Response-Time Algorithm
 * @date 2026-05-10
 * @author Exam Candidate Y3898772
 */

#include <iostream>
#include <ranges>
#include <set>

#include "ResponseTimeAnalyser.hpp"
#include "Task.hpp"

/**
 * @brief Entry point and driver of the RTA procedure.
 * @return Zero if the task set can be scheduled, 1 otherwise.
 */
int main()
{
    // clang-format off

    // For DM priority ordering, use DMTaskComparator.
    std::set<RTA::Task, RTA::RMTaskComparator> tasks{
        {"tau_1", {5, 20}, 500, 400},
        {"tau_2", {7, 14}, 100, 100},
        {"tau_3", {1, 5}, 50, 50},
        {"tau_4", {1, 6}, 25, 25},
        {"tau_5", {18, 35}, 200, 200},
        {"tau_6", {27, 53}, 1000, 250},
    };
    // clang-format on

    // Execute Response-Time Algorithm
    const auto is_schedulable =
            RTA::ResponseTimeAnalyser::rta(tasks, &std::cout);

    // Print post-RTA task set and schedulability summary.
    std::cout << '\n';
    RTA::Task::print_header(std::cout);
    for (const auto &task: tasks)
        std::cout << task << '\n';
    std::cout << std::endl;

    if (is_schedulable) {
        std::cout << "TASK SET CAN BE SCHEDULED" << std::endl;
        return 0;
    }

    std::cout << "TASK SET CANNOT BE SCHEDULED" << std::endl;

    // Now maximise the lowest-priority task WCET
    const auto maximised_wcet = RTA::ResponseTimeAnalyser::maximise_wcet(
            tasks, *tasks.rbegin(), std::cout);
    std::cout << "Maximised WCET: " << maximised_wcet << std::endl;


    return 1;
}
