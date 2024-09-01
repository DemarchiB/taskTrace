#include "Log.h"

#include <stdio.h>     // FILE, fopen, fclose, fprintf
#include <unistd.h>    // unlink
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>


static bool fileExists(const char *filename);
static void Log_createLogPathIfNotExist(const char *const path);
static void Log_prepareFileForLogging(const Log *const me);


void Log_init(Log *const me, const char *const path, const char *const fileName)
{
    me->enabled = false;

    strcpy(me->file, path);
    strcat(me->file, fileName);
    strcat(me->file, ".csv");
    
    Log_createLogPathIfNotExist(path);
    Log_prepareFileForLogging(me);
}

void Log_enable(Log *const me)
{
    me->enabled = true;
}

void Log_disable(Log *const me)
{
    me->enabled = false;
}

void Log_addNewPoint(Log *const me, const Telegram* const telegram, const MonitorMetrics* const monitorMetrics, bool isException)
{
    Data data;

    Data_fillTelegramData(&data, telegram);
    Data_fillMonitorMetricsData(&data, monitorMetrics);
    Data_setExceptionFlag(&data, isException);

    List_add(&me->list, &data);
}

void Log_generateIfNeeded(Log *const me)
{
    if (me->enabled == false) {
        return;
    }

    if (List_checkIfReadyToLog(&me->list) == false) {
        return;
    }
    
    Data data;
    
    FILE *file = fopen(me->file, "a");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    // Read while there is data on the list
    const int numColumns = Data_getNumberOfColumns();

    while (List_read(&me->list, &data) == 0) {
        // Escreve os valores das colunas no arquivo CSV
        for (int i = 0; i < numColumns; i++) {
            int64_t value = Data_getColumnValue(&data, i);
            fprintf(file, "%lld", (long long)value);
            if (i < numColumns - 1) {
                fprintf(file, ", "); // Adiciona a vírgula entre os valores das colunas
            }
        }
        fprintf(file, "\n");
    }

    // Fecha o arquivo
    fclose(file);
}

static bool fileExists(const char *filename)
{
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

static void Log_createLogPathIfNotExist(const char *const path)
{
    struct stat st = {0};

    // Check if already exist
    if (stat(path, &st) == -1) {
        // if not, try creating it
        if (mkdir(path, 0777)) {
            perror("Error creating log path");
            printf("TODO: The program is not able to create intermetiate folders, maybe that's the problem.\n");
        }
    }
}

static void Log_prepareFileForLogging(const Log *const me)
{
    // Check if the file already exist
    if (fileExists(me->file)) {
        // If so, delete it
        if (unlink(me->file) != 0) {
            perror("Error deleting file");
        }
    }

    // Create the log file (CSV)
    FILE *file = fopen(me->file, "w");
    if (file == NULL) {
        perror("Error creating file");
        return;
    }

    // Write the column headers
    const int numColumns = Data_getNumberOfColumns();
    
    for (int i = 0; i < numColumns; i++) {
        char columnName[100];
        Data_getColumnName(i, columnName);
        fprintf(file, "%s", columnName);
        if (i < numColumns - 1) {
            fprintf(file, ", "); // Add a comma between columns
        }
    }
    fprintf(file, "\n");

    fclose(file);
}
