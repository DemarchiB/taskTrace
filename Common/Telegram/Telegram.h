#ifndef __TASK_TRACE_TELEGRAM_H__
#define __TASK_TRACE_TELEGRAM_H__

#include <stdint.h>
#include <sys/types.h>

#include <time.h>

#define TELEGRAM_PERFMARK_OFFSET 100 // The offset from wich the performance markers will start

typedef enum {
    TelegramCode_cyclicTaskFirstReady,
    TelegramCode_startAndStopTime,
    TelegramCode_perfMark1Start = TELEGRAM_PERFMARK_OFFSET,
    TelegramCode_perfMark1End,
    NumberOfTelegramCodes,
} TelegramCode;


typedef struct {
    pid_t pid;  // Is it really necessary since we have a buffer for each pid? I'll let it here for now
    TelegramCode code;
    uint64_t t1;
    uint64_t t2;
} Telegram;

#endif // __TASK_TRACE_TELEGRAM_H__