#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H
/*用户设定宏*/

#define configMAX_TASK_NAME_LEN (16) // 任务名称最长长度
#define configMAX_PRIORITIES 5       // 任务队列允许的优先级数量
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 191  // 临界区时，允许中断优先级数>=11被屏蔽 191=0b10111111，高四位为11

#define configUSE_16_BIT_TICKS 0          // 允许使用32位时间片
#define configSUPPORT_STATIC_ALLOCATION 1 // 允许使用静态内存分配

#define xPortPendSVHandler PendSV_Handler   // 同中断向量表一样的名字
#define xPortSysTickHandler SysTick_Handler
#define vPortSVCHandler SVC_Handler

// 确认x是否为真。当x为假时，会调用configASSERT()，会调用taskDISABLE_INTERRUPTS()，然后进入死循环，不会返回
#define configASSERT( x )         \
    if((x) == 0)                  \
    {                             \
        taskDISABLE_INTERRUPTS(); \
        for( ; ; )                \
        ;                         \
    }

#endif
