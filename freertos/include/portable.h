#ifndef PORTABLE_H
#define PORTABLE_H
#include "portmacro.h"

/**
 * @brief 初始化栈顶指针，栈一开始为空，需要填入数据，用于后面填入cpu寄存器
 *
 * @param pxTopOfStack 栈顶指针
 * @param pxCode 函数入口地址
 * @param pvParameters 函数参数
 *
 * @return StackType_t* 栈顶指针
 */
StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack,
                                   TaskFunction_t pxCode,
                                   void *pvParameters);
/**
 * @brief 启动调度器
 *
 * @note 此函数会返回，如果返回则说明调度器启动失败
 */
BaseType_t xPortStartScheduler(void);

#endif
