#ifndef __TASK_TRACE_TELEGRAM_H__
#define __TASK_TRACE_TELEGRAM_H__

#include <stdint.h>
#include <sys/types.h>

#include <time.h>

typedef enum {
    TelegramCode_startWorkPoint,
    TelegramCode_stopWorkPoint,
    // These are not implemented yet, they will be available on future
    //TelegramCode_startPerfMark,
    //TelegramCode_middlePerfMark,
    //TelegramCode_endPerfMark,
    NumberOfTelegramCodes,
} TelegramCode;

typedef struct {
    pid_t pid;  // Is it really necessary since we have a buffer for each pid? I'll let it here for now
    TelegramCode code;
    struct timespec timestamp;
} Telegram;

#endif // __TASK_TRACE_TELEGRAM_H__