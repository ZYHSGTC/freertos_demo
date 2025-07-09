#ifndef FREERTOS_H
#define FREERTOS_H
#include <stdint.h>     // 获取标准库的 uint_8,int32_t 和 uint32_t
#include <stddef.h>     // 获取标准库的 NULL 和 size_t

#include "freertos_config.h"
#include "projdefs.h"   //  必须在引入portable.h之前
#include "portable.h"

struct xSTATIC_LIST_ITEM
{
    TickType_t xDummy2;
    void * pvDummy3[4];
};
typedef struct xSTATIC_LIST_ITEM StaticListItem_t;

typedef struct xSTATIC_TCB
{
    void * pxDummy1;
    StaticListItem_t xDummy3[2];
    UBaseType_t uxDummy5;
    void * pxDummy6;
    uint8_t ucDummy7[ configMAX_TASK_NAME_LEN ];
} StaticTask_t;

#endif
