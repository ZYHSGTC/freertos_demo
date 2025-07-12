#ifndef TASK_H
#define TASK_H

#include "list.h"

struct tskTaskControlBlock;                       // 详细定义在task.c中，因为里面有敏感信息，不能被随意修改访问
typedef struct tskTaskControlBlock *TaskHandle_t; // 指向任务控制块的指针，用户会用到

#define taskYIELD() portYIELD()
/** 临界区函数，使用时加上大括号：
 * ✅ 安全性	避免宏展开带来的语法歧义
 * ✅ 可读性	明确表示临界区范围
 * ✅ 维护性	方便后续插入临时变量或调试信息
 * 
 * 场景	                            是否需要临界区	原因
 * 修改就绪队列	                    是	            防止pendsv任务切换中断中和主流程同时修改
 * 修改 pxCurrentTCB	            是	            关键变量，影响上下文切换
 * 修改事件队列	                    是	            防止中断与任务并发访问
 * 调用 vListInsertEnd() 等链表操作	是	            链表操作不是原子的
 * 修改全局计数器                   是	            多线程/中断访问导致 race condition
*/
#define taskDISABLE_INTERRUPTS() portDISABLE_INTERRUPTS()
#define taskENABLE_INTERRUPTS() portENABLE_INTERRUPTS()
#define taskENTER_CRITICAL() portENTER_CRITICAL()
#define taskENTER_CRITICAL_FROM_ISR() portENTER_CRITICAL_FROM_ISR()
#define taskEXIT_CRITICAL() portEXIT_CRITICAL()
#define taskEXIT_CRITICAL_FROM_ISR(x) portEXIT_CRITICAL_FROM_ISR(x)

/* 阻塞延时 */
void vTaskDelay(const TickType_t xTicksToDelay);

/* 启动任务调度 */
void vTaskStartScheduler(void);
/* 任务切换 */
void vTaskSwitchContext(void);
/* 延时计时*/
BaseType_t xTaskIncrementTick(void);

#if (configSUPPORT_STATIC_ALLOCATION == 1)
/**
 * @brief 创建静态任务
 *
 * @param pxTaskCode        任务函数，是一个void(void*)的函数指针+
 * @param pcName            任务名
 * @param uxStackDepth      任务栈大小，多少个字（StackType_t/uint32_t）
 * @param pvParameters      任务函数的参数
 * @param uxPriority        任务优先级
 * @param puxStackBuffer    任务栈缓冲区的起始地址，大小为 uxStackDepth * sizeof(StackType_t)
 * @param pxTaskBuffer      任务控制块缓冲区
 * @return TaskHandle_t 任务句柄
 */
TaskHandle_t xTaskCreateStatic(TaskFunction_t pxTaskCode,
                               const char *const pcName,
                               const StackType_t uxStackDepth,
                               void *const pvParameters,
                               UBaseType_t uxPriority,
                               StackType_t *const pxStackBuffer,
                               StaticTask_t *const pxTaskBuffer);
#endif

#endif
