#include "List.h"

#include <string.h>

void List_init(List *const me)
{
    memset(me, 0, sizeof(List));
}

void List_clean(List *const me)
{
    List_init(me);
}

void List_add(List *const me, const Data *const data)
{
    memcpy(&me->dataList[me->listIndex++], data, sizeof(Data));

    if (me->listIndex >= NUMBER_OF_POINTS) {
        me->listIndex = 0;
    }

    if (me->numberOfDataOnBuffer < NUMBER_OF_POINTS) {
        me->numberOfDataOnBuffer++;
    }

    if (data->isException) {
        me->exceptionHappend = true;
    }
}

int List_read(List *const me, Data *const data)
{
    if (me->numberOfDataOnBuffer == 0) {
        return -1;
    }

    // Calcula o índice do dado mais antigo
    int readIndex = me->listIndex - me->numberOfDataOnBuffer;
    if (readIndex < 0) {
        readIndex += NUMBER_OF_POINTS;
    }

    memcpy(data, &me->dataList[readIndex], sizeof(Data));

    me->numberOfDataOnBuffer--;

    return 0;
}

bool List_checkIfReadyToLog(List *const me)
{
    if ((me->numberOfDataOnBuffer >= NUMBER_OF_POINTS) && (me->exceptionHappend)){
        me->exceptionHappend = false;
        return true;
    }

    return false;
}