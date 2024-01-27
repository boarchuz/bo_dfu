#ifndef BO_DFU_UTIL_H
#define BO_DFU_UTIL_H

#include "sdkconfig.h"

#ifndef ARRAY_SIZE
#   define ARRAY_SIZE(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#endif

#endif /* BO_DFU_UTIL_H */
