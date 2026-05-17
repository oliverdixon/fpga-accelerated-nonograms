// clang-format Language: C

/**
 * @file
 * @brief Buffered task-safe logging interface
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#ifndef LOGGING_H
#define LOGGING_H

#include <stdbool.h>

/**
 * @brief Attempt to initialise the logging interface.
 * @return Could the logging interface be initialised?
 */
bool logging_initialise();

/**
 * @brief Post a formatted message to the logger.
 * @param fmt Standard <code>printf</code> format string.
 * @param ... Standard <code>printf</code> arguments i.a.w. the format string.
 */
void logging_printf(
    const char * fmt,
    ...
);

/**
 * @brief Post an unformatted string to the logger.
 * @param str The string to send to the logger.
 */
void logging_puts(const char * str);

#endif // LOGGING_H
