#ifndef __TASK_TRACE_SUPERVISOR_USER_INPUTS_H__
#define __TASK_TRACE_SUPERVISOR_USER_INPUTS_H__

#include <stdbool.h>

/**
 * @brief Struct with all possible user inputs
 * 
 */
typedef struct {
    bool enableLogging;
} UserInputs;

int UserInputs_read(UserInputs *const me, int argc, char *argv[]);

#endif // __TASK_TRACE_SUPERVISOR_USER_INPUTS_H__