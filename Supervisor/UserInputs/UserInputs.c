#include "UserInputs.h"

#include <string.h>
#include <stdio.h>

int UserInputs_read(UserInputs *const me, int argc, char *argv[])
{
    memset(me, 0, sizeof(UserInputs));

    if (argc < 2) {
        printf("Supervisor: Error, user must pass at least the log configuration.\n");
        printf("Usage: sudo Supervisor --log on\n");
        printf("   or: sudo Supervisor --log off\n");
        return -1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--log") == 0) {
            // Check if there is a value after "--log"
            if (i + 1 < argc) {
                if (strcmp(argv[i + 1], "on") == 0) {
                    me->enableLogging = 1;
                } else if (strcmp(argv[i + 1], "off") == 0) {
                    me->enableLogging = 0;
                } else {
                    printf("Supervisor: Invalid value for \"--log\"\n");
                    printf("Supervisor: Valid values \"on\" and \"off\"\n");
                    return -2;
                }
                i++; // jump to the next argument.
            } else {
                printf("Supervisor: missing value for \"--log\"\n");
                return -2;
            }
        }
        else if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
            printf("Usage: sudo Supervisor --log on\n");
            printf("   or: sudo Supervisor --log off\n");
            return -4; // Return error just to avoid the supervisor to move on
        }
    }

    return 0;
}