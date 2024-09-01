#ifndef __TASK_TRACE_SUPERVISOR_LOG_H__
#define __TASK_TRACE_SUPERVISOR_LOG_H__

#include "List/List.h"

typedef struct {
    List list;

    bool enabled;
    char file[250];
} Log;

void Log_init(Log *const me, const char *const path, const char *const fileName);
void Log_enable(Log *const me);
void Log_disable(Log *const me);

void Log_addNewPoint(Log *const me, const Telegram* const telegram, const MonitorMetrics* const monitorMetrics, bool isException);

void Log_generateIfNeeded(Log *const me);

#endif // __TASK_TRACE_SUPERVISOR_LOG_H__