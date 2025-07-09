#ifndef PROJDEFS_H
#define PROJDEFS_H

typedef void (*TaskFunction_t)(void *arg); // 定义函数指针的别名，void (*TaskFunction_t)(void *)也对

// pd的意思是pre-defined，预先定义的
#define pdFALSE ((BaseType_t)0)
#define pdTRUE ((BaseType_t)1)

#define pdFAIL pdFALSE
#define pdPASS pdTRUE

#endif
