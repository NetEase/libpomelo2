/**
 * Copyright (c) 2014 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "pb_util.h"

int pb_get_type(const char *type) {
    if (strcmp(type, "uInt32") == 0)
        return 1;
    if (strcmp(type, "int32") == 0)
        return 2;
    if (strcmp(type, "sInt32") == 0)
        return 3;
    if (strcmp(type, "float") == 0)
        return 4;
    if (strcmp(type, "double") == 0)
        return 5;
    if (strcmp(type, "string") == 0)
        return 6;
    // if (strcmp(type, "uInt64") == 0)
    //   return 5;
    // if (strcmp(type, "sInt64") == 0)
    //   return 5;
    return 0;
}

int pb_get_constant_type(const char *type) {
    if (strcmp(type, "uInt32") == 0 || strcmp(type, "sInt32") == 0
            || strcmp(type, "int32") == 0) {
        return 0;
    }
    if (strcmp(type, "double") == 0) {
        return 1;
    }
    if (strcmp(type, "string") == 0) {
        return 2;
    }
    if (strcmp(type, "float") == 0) {
        return 5;
    }
    return 2;
}
