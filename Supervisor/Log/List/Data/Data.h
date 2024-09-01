#ifndef __TASK_TRACE_SUPERVISOR_LOG_LIST_DATA_H__
#define __TASK_TRACE_SUPERVISOR_LOG_LIST_DATA_H__

#include "../../../../Common/Telegram/Telegram.h"
#include "../../../Monitor/MonitorMetrics/MonitorMetrics.h"

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    // Telegram
    ColumnName_pid = 0,
    ColumnName_t1,
    ColumnName_t2,
    // Monitor metrics
    ColumnName_lastCyclicTaskReadyTime,
    ColumnName_maxLatency,
    ColumnName_minLatency,
    ColumnName_lastLatency,
    ColumnName_deadlineLostCount,
    ColumnName_runtimeOverrunCount,
    ColumnName_taskDepletedCount,
    ColumnName_cycleCount,
    ColumnName_lastStartExecutionTime,
    ColumnName_lastStopExecutionTime,
    ColumnName_lastET,
    ColumnName_minET,
    ColumnName_WCET,
    // Others
    ColumnName_isException,
    NumberOfColumns,
} DataColumnsName;

typedef struct {
    Telegram telegramSent;
    MonitorMetrics monitorMetrics;
    bool isException;
} Data;

void Data_clean(Data *const me);

void Data_fillTelegramData(Data *const me, const Telegram* const telegram);
void Data_fillMonitorMetricsData(Data *const me, const MonitorMetrics* const monitorMetrics);

void Data_setExceptionFlag(Data *const me, bool value);
bool Data_getExceptionFlag(const Data *const me);

int Data_getNumberOfColumns(void);
int Data_getColumnName(const int columnNumber, char *const name);
int64_t Data_getColumnValue(const Data *const me, const int columnNumber);

#endif // __TASK_TRACE_SUPERVISOR_LOG_LIST_DATA_H__