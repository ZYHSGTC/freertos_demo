#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H
/*用户设定宏*/

// 仿真器时钟为12MHz
#define configCPU_CLOCK_HZ ((unsigned long)12000000)
#define configTICK_RATE_HZ 100

// 任务栈最小长度
#define configMINIMAL_STACK_SIZE 128             
#define configMAX_TASK_NAME_LEN (16)             // 任务名称最长长度
#define configMAX_PRIORITIES 5                   // 任务队列允许的优先级数量
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 191 // 临界区时，允许中断优先级数>=11被屏蔽 191=0b10111111，高四位为11


#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configUSE_16_BIT_TICKS 0          // 允许使用32位时间片
#define configSUPPORT_STATIC_ALLOCATION 1 // 允许使用静态内存分配
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1 // 允许使用cortex-m3相关寄存器优化的任务选择
#define xPortPendSVHandler PendSV_Handler // 同中断向量表一样的名字
#define xPortSysTickHandler SysTick_Handler
#define vPortSVCHandler SVC_Handler

// 确认x是否为真。当x为假时，会调用configASSERT()，会调用taskDISABLE_INTERRUPTS()，然后进入死循环，不会返回
#define configASSERT(x)           \
    if ((x) == 0)                 \
    {                             \
        taskDISABLE_INTERRUPTS(); \
        for (;;)                  \
            ;                     \
    }

#endif
