#ifndef __TASK_TRACE_SUPERVISOR_LOG_LIST_H__
#define __TASK_TRACE_SUPERVISOR_LOG_LIST_H__

#include "Data/Data.h"

#define NUMBER_OF_POINTS 5

typedef struct {
    Data dataList[NUMBER_OF_POINTS];

    int numberOfDataOnBuffer;
    int listIndex;

    bool exceptionHappend;
} List;

void List_init(List *const me);
void List_clean(List *const me);

void List_add(List *const me, const Data *const data);
int List_read(List *const me, Data *const data);

bool List_checkIfReadyToLog(List *const me);

#endif // __TASK_TRACE_SUPERVISOR_LOG_LIST_H__