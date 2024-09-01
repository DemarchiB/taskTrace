#include "Data.h"

#include <string.h>


void Data_clean(Data *const me)
{
    memset(me, 0, sizeof(Data));
}

void Data_fillTelegramData(Data *const me, const Telegram* const telegram)
{
    memcpy(&me->telegramSent, telegram, sizeof(Telegram));
}

void Data_fillMonitorMetricsData(Data *const me, const MonitorMetrics* const monitorMetrics)
{
    memcpy(&me->monitorMetrics, monitorMetrics, sizeof(MonitorMetrics));
    me->monitorMetrics.lastCyclicTaskReadyTime = monitorMetrics->lastCyclicTaskReadyTime / (1000.0);
    me->monitorMetrics.maxLatency = monitorMetrics->maxLatency / (1000.0);
    me->monitorMetrics.minLatency = monitorMetrics->minLatency / (1000.0);
    me->monitorMetrics.lastLatency = monitorMetrics->lastLatency / (1000.0);
    me->monitorMetrics.deadlineLostCount = monitorMetrics->deadlineLostCount;
    me->monitorMetrics.runtimeOverrunCount = monitorMetrics->runtimeOverrunCount;
    me->monitorMetrics.taskDepletedCount = monitorMetrics->taskDepletedCount;
    me->monitorMetrics.cycleCount = monitorMetrics->cycleCount;
    me->monitorMetrics.lastStartExecutionTime = monitorMetrics->lastStartExecutionTime / (1000.0);
    me->monitorMetrics.lastStopExecutionTime = monitorMetrics->lastStopExecutionTime / (1000.0);
    me->monitorMetrics.lastET = monitorMetrics->lastET / (1000.0);
    me->monitorMetrics.minET = monitorMetrics->minET / (1000.0);
    me->monitorMetrics.WCET = monitorMetrics->WCET / (1000.0);
}

void Data_setExceptionFlag(Data *const me, bool value)
{
    me->isException = value;
}

bool Data_getExceptionFlag(const Data *const me)
{
    return (int64_t) me->isException;
}

/*
    These get column functions are hardcode here.
*/
int Data_getNumberOfColumns(void)
{
    return NumberOfColumns;
}

int Data_getColumnName(const int columnNumber, char *const name)
{
    if (name == NULL) {
        return -1;
    }

    if (columnNumber >= NumberOfColumns) {
        strcpy(name, "Invalid");
        return -2;
    }

    const DataColumnsName column = (DataColumnsName) columnNumber;
    switch (column) {
    // Telegram
    case ColumnName_pid:
        strcpy(name, "pid");
    break;
    case ColumnName_t1:
        strcpy(name, "t1 (ns)");
    break;
    case ColumnName_t2:
        strcpy(name, "t2 (ns)");
    break;
    // Monitor metrics
    case ColumnName_lastCyclicTaskReadyTime:
        strcpy(name, "lastCyclicTaskReadyTime (us)");
    break;
    case ColumnName_maxLatency:
        strcpy(name, "maxLatency (us)");
    break;
    case ColumnName_minLatency:
        strcpy(name, "minLatency (us)");
    break;
    case ColumnName_lastLatency:
        strcpy(name, "lastLatency (us)");
    break;
    case ColumnName_deadlineLostCount:
        strcpy(name, "deadlineLostCount");
    break;
    case ColumnName_runtimeOverrunCount:
        strcpy(name, "runtimeOverrunCount");
    break;
    case ColumnName_taskDepletedCount:
        strcpy(name, "taskDepletedCount");
    break;
    case ColumnName_cycleCount:
        strcpy(name, "cycleCount");
    break;
    case ColumnName_lastStartExecutionTime:
        strcpy(name, "lastStartExecutionTime (us)");
    break;
    case ColumnName_lastStopExecutionTime:
        strcpy(name, "lastStopExecutionTime (us)");
    break;
    case ColumnName_lastET:
        strcpy(name, "lastET (us)");
    break;
    case ColumnName_minET:
        strcpy(name, "minET (us)");
    break;
    case ColumnName_WCET:
        strcpy(name, "WCET (us)");
    break;
    // Others
    case ColumnName_isException:
        strcpy(name, "isException");
    break;
    default:
        return -2;  // Invalid, should naver reach here
    break;
    }

    return 0;
}

int64_t Data_getColumnValue(const Data *const me, const int columnNumber)
{
    if (columnNumber >= NumberOfColumns) {
        return 0;
    }

    const DataColumnsName column = (DataColumnsName) columnNumber;
    switch (column) {
    // Telegram
    case ColumnName_pid:
        return (int64_t) me->telegramSent.pid;
    break;
    case ColumnName_t1:
        return (int64_t) me->telegramSent.t1;
    break;
    case ColumnName_t2:
        return (int64_t) me->telegramSent.t2;
    break;
    // Monitor metrics
    case ColumnName_lastCyclicTaskReadyTime:
        return (int64_t) me->monitorMetrics.lastCyclicTaskReadyTime;
    break;
    case ColumnName_maxLatency:
        return (int64_t) me->monitorMetrics.maxLatency;
    break;
    case ColumnName_minLatency:
        return (int64_t) me->monitorMetrics.minLatency;
    break;
    case ColumnName_lastLatency:
        return (int64_t) me->monitorMetrics.lastLatency;
    break;
    case ColumnName_deadlineLostCount:
        return (int64_t) me->monitorMetrics.deadlineLostCount;
    break;
    case ColumnName_runtimeOverrunCount:
        return (int64_t) me->monitorMetrics.runtimeOverrunCount;
    break;
    case ColumnName_taskDepletedCount:
        return (int64_t) me->monitorMetrics.taskDepletedCount;
    break;
    case ColumnName_cycleCount:
        return (int64_t) me->monitorMetrics.cycleCount;
    break;
    case ColumnName_lastStartExecutionTime:
        return (int64_t) me->monitorMetrics.lastStartExecutionTime;
    break;
    case ColumnName_lastStopExecutionTime:
        return (int64_t) me->monitorMetrics.lastStopExecutionTime;
    break;
    case ColumnName_lastET:
        return (int64_t) me->monitorMetrics.lastET;
    break;
    case ColumnName_minET:
        return (int64_t) me->monitorMetrics.minET;
    break;
    case ColumnName_WCET:
        return (int64_t) me->monitorMetrics.WCET;
    break;
    // Others
    case ColumnName_isException:
        return (int64_t) me->isException;
    break;
    default:
        return 0;
    break;
    }
}