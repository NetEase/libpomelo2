/**
 * Copyright (c) 2014 Pomelo Team, NetEase Inc
 * MIT Licensed.
 */

#ifndef PC_TEST_H
#define PC_TEST_H

#include <stdlib.h>

#define PC_TEST_ASSERT(expr)                                          \
    do {                                                         \
        if (!(expr)) {                                           \
            fprintf(stderr,                                      \
                    "Assertion failed in %s on line %d: %s\n",   \
                    __FILE__,                                    \
                    __LINE__,                                    \
                    #expr);                                      \
            abort();                                             \
        }                                                        \
    } while (0)


#endif
