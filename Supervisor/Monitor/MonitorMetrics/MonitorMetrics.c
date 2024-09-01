#include "MonitorMetrics.h"

#include <string.h>


void MonitorMetrics_init(MonitorMetrics *const me)
{
    memset(me, 0, sizeof(MonitorMetrics));

    me->minLatency = (uint64_t) -1;
    me->minET = (uint64_t) -1;
}
