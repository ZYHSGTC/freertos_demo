#ifndef PORTMARCO_H
#define PORTMARCO_H
/*
 *   FreeRTOS 都会将标准的 C 数据类型用 typedef 重新取一个类型名
 */
#include <stdint.h> // 获取标准库的 int32_t 和 uint32_t
#include <stddef.h> // 获取标准库的 NULL 和 size_t

/*
 *  栈类型，栈单元的大小为 32 位，4字节
 */
#define portSTACK_TYPE uint32_t

/*
 * 表示这是一个无符号基础类型，常用于表示任务优先级、队列长度、信号量计数值等系统内核相关的无符号整数。
 *   U	    Unsigned（无符号）
 *   Base	基础类型（basic type），不依赖具体结构或平台
 *   Type_t	类型后缀，表明这是一个通过 typedef 定义的标准类型
 *   long    long的大小取决与平台，通常是32位或64位
 */
typedef portSTACK_TYPE StackType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;
/*
 * 使用 #define（非类型安全）
 *   #define TickType_t uint32_t
 *   实际只是文本替换，编译器并不认为 TickType_t 是一个独立的类型。
 *   你可以把它和 uint32_t 互换使用，即使逻辑上它们代表不同含义。
 *   缺乏语义约束，容易造成类型混用。
 * 使用 typedef（具备类型安全）
 *   typedef uint32_t TickType_t;
 *   TickType_t 被编译器视为 uint32_t 的别名，但它的出现增强了语义表达（例如表示系统节拍计数值）。
 *   在一些编译器或静态分析工具中，可以基于该类型进行更精确的检查。
 *   更利于后期重构和平台移植。
 */
#if (configUSE_16_BIT_TICKS == 1)
typedef uint16_t TickType_t;
#define portMAX_DELAY (TickType_t)0xffff
#else
typedef uint32_t TickType_t;
#define portMAX_DELAY (TickType_t)0xffffffffUL
#endif

// 触发 PendSV 中断
// ❌ __asm volatile("dsb");	仅防止 CPU 流水线乱序，但编译器可能仍然优化重排内存访问
// ✅ __asm volatile("dsb" ::: "memory");	同时防止编译器优化和 CPU 执行乱序，确保内存访问顺序不变
#define portYIELD()                                     \
    {                                                   \
        portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT; \
        __asm volatile("dsb" ::: "memory");             \
        __asm volatile("isb");                          \
    }

/** SCB->ICSR(Interrupt control and state register) 寄存器，
 * 28位PENDSVSET写1表示，将PendSV异常状态更改为“待处理(挂起)”
 * 前8位置VECTACTIVE，有任何非零值时表示当前正在执行的中断服务程序；
 */
#define portNVIC_INT_CTRL_REG (*((volatile uint32_t *)0xe000ed04))
#define portNVIC_PENDSVSET_BIT (1UL << 28UL)
#define portVECTACTIVE_MASK (0xFFUL)

// port.c 定义
extern void vPortEnterCritical(void);
extern void vPortExitCritical(void);

// 临界区宏
#define portSET_INTERRUPT_MASK_FROM_ISR() ulPortRaiseBASEPRI()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x) vPortSetBASEPRI(x)
#define portDISABLE_INTERRUPTS() vPortRaiseBASEPRI()
#define portENABLE_INTERRUPTS() vPortSetBASEPRI(0)
#define portENTER_CRITICAL_FROM_ISR() portSET_INTERRUPT_MASK_FROM_ISR()
#define portEXIT_CRITICAL_FROM_ISR(x) portCLEAR_INTERRUPT_MASK_FROM_ISR(x)
#define portENTER_CRITICAL() vPortEnterCritical()
#define portEXIT_CRITICAL() vPortExitCritical()

// 强制内联，也就是复制代码到调用处，为什么要加__attribute__
#ifndef portFORCE_INLINE
#define portFORCE_INLINE inline __attribute__((always_inline))
#endif

/** 开启临界区，设置BASEPRI
 * @note 不能用于中断，如果中断中使用，那中断结束后，basepri不能返回原值变成0
 *       1.进入临界区后发生中断，如果中断号低于basepri，即优先级更高
 *          意味着如果代码进入了临界区，同时发生此中断
 *          这个中断结束后，原临界区就失效了（basepri变了）
 *       2.如果中断号高于basepri，即优先级较低
 *          意味着现在中断应该被屏蔽，此时如果有高优先级中断过来，
 *          那高优先级中断运行完就不会返回低优先级中断，被卡死了
 */
portFORCE_INLINE static void vPortRaiseBASEPRI(void)
{
    uint32_t ulNewBASEPRI;
    __asm volatile(
        "mov %0, %1         \n"
        "msr basepri, %0    \n"
        "dsb                \n"
        "isb                \n"
        : "=r"(ulNewBASEPRI)
        : "i"(configMAX_SYSCALL_INTERRUPT_PRIORITY)
        : "memory");
}

/** 开启临界区，设置BASEPRI，返回原来的BASEPRI
 * @return 原来的BASEPRI
 */
portFORCE_INLINE static uint32_t ulPortRaiseBASEPRI(void)
{
    uint32_t ulOriginalBASEPRI, ulNewBASEPRI;
    __asm volatile(
        "mrs %0, basepri                \n"
        "mov %1, %2                     \n"
        "msr basepri, %1                \n"
        "dsb                            \n"
        "isb                            \n"
        : "=r"(ulOriginalBASEPRI), "=r"(ulNewBASEPRI)
        : "i"(configMAX_SYSCALL_INTERRUPT_PRIORITY)
        : "memory");
    return ulOriginalBASEPRI;
}

portFORCE_INLINE static void vPortSetBASEPRI(uint32_t ulNewMaskValue)
{
    // 为什么不需要dsb，isb，.word, volatile
    __asm volatile("msr basepri, %0" ::"r"(ulNewMaskValue) : "memory");
}

#endif
