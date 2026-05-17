/**
 * @file
 * @brief Buffered task-safe logging implementation
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#include <FreeRTOS.h>
#include <queue.h>
#include <stdio.h>
#include <string.h>
#include <xil_printf.h>

#include "logging.h"

#define LOG_LINE_LENGTH (64) /**< @brief Maximum line length in bytes of any (formatted or unformatted) log message.   \
                              */
#define LOG_QUEUE_SIZE (16)  /**< @brief Number of buffered log messages. */

/**
 * @struct LogMessage
 * @brief A message to send to the global logger.
 */
struct LogMessage
{
    const char * task_name;     /**< @brief The FreeRTOS task name from which the message originates. */
    char text[LOG_LINE_LENGTH]; /**< @brief The text of the message to log. */
};

static QueueHandle_t logging_queue; /**< @brief The FreeRTOS queue for log messages. */

/**
 * @brief FreeRTOS task to receive log messages from the global queue and produce them on the serial line.
 * @param data Unused task payload.
 */
static void write_log_task(
    // ReSharper disable once CppParameterMayBeConstPtrOrRef - Signature imposed by FreeRTOS
    void * const data
) {
    (void)data;

    struct LogMessage msg;

    // ReSharper disable once CppDFAEndlessLoop
    while (1)
        if (xQueueReceive(logging_queue, &msg, portMAX_DELAY) == pdPASS)
            xil_printf("[%s] %s\r\n", msg.task_name == NULL ? "<Unknown Task>" : msg.task_name, msg.text);

    // ReSharper disable once CppDFAUnreachableCode
    vTaskDelete(NULL);
}

bool logging_initialise() {
    logging_queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(struct LogMessage));
    return logging_queue != NULL && xTaskCreate(&write_log_task, "write_log_task", 1024, NULL, 3, NULL) == pdPASS;
}

void logging_printf(
    const char * const fmt,
    ...
) {
    if (logging_queue == NULL)
        return;

    struct LogMessage msg = {.task_name = pcTaskGetTaskName(NULL)};

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg.text, LOG_LINE_LENGTH, fmt, args);
    va_end(args);
    msg.text[LOG_LINE_LENGTH - 1] = '\0';

    xQueueSend(logging_queue, &msg, 0);
}

void logging_puts(
    const char * const str
) {
    if (logging_queue == NULL)
        return;

    struct LogMessage msg = {.task_name = pcTaskGetTaskName(NULL)};

    strncpy(msg.text, str, LOG_LINE_LENGTH);
    msg.text[LOG_LINE_LENGTH - 1] = '\0';

    xQueueSend(logging_queue, &msg, 0);
}
