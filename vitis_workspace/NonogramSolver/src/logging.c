#include <FreeRTOS.h>
#include <queue.h>
#include <stdio.h>
#include <string.h>
#include <xil_printf.h>

#include "logging.h"

#define LOG_LINE_LENGTH 64
#define LOG_QUEUE_SIZE 16

struct LogMessage
{
    const char * task_name;
    char text[LOG_LINE_LENGTH];
};

static QueueHandle_t logging_queue;

static void write_log_task(
    void * const data
) {
    (void)data;

    struct LogMessage msg;

    while (1)
        if (xQueueReceive(logging_queue, &msg, portMAX_DELAY) == pdPASS)
            xil_printf(
                "[%s] %s\r\n", msg.task_name == NULL ? "<Unknown Task>" : msg.task_name, msg.text
            );

    vTaskDelete(NULL);
}

void logging_initialise() {
    logging_queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(struct LogMessage));
    xTaskCreate(&write_log_task, "write_log_task", 1024, NULL, 3, NULL);
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
