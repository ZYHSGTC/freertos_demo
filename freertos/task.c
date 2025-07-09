#include <string.h>
#include "task.h"

/** 线程控制块
 *  @brief 线程控制块
 *  @param pxTopOfStack 栈顶指针 后面任务切换与任务启动都需要读取或修改这个值，
 *                      使用volatile修饰，防止异步（中断）发生时，没法访问到最新值，因此不能被编译器优化，每次都得对改成员进行内存访问
 *  @param xStateListItem 状态链表项，系统任务队列是架子，这个节点是钩子，这个TCB是衣服；
 *  @param xEventListItem 事件链表项，用于插入任务的事件等待队列（信号量、消息队列等）
 *  @param pxStack 栈初始指针，一初始化基本不变
 *  @param pcTaskName 任务名称，一初始化基本不变
 *  @note 主要包含任务名，栈信息（栈初始指针，栈顶指针），任务链表项钩子及其优先级，
 */
typedef struct tskTaskControlBlock
{
    volatile StackType_t *pxTopOfStack; //  必须在结构体的顶部，任务启动与切换时会直接访问
    ListItem_t xStateListItem;
    ListItem_t xEventListItem;
    UBaseType_t uxPriority;
    StackType_t *pxStack;
    char pcTaskName[configMAX_TASK_NAME_LEN];
} tskTCB; // 后面不是用tskTCB而是用TCB_t，为了版本兼容
typedef tskTCB TCB_t; // TCB_t 是系统私有不被外部使用的类型

// 这个指针式volatile的，不是其指向的内容是volatile的
TCB_t *volatile pxCurrentTCB = NULL;

// 就绪队列
static List_t pxReadyTasksLists[configMAX_PRIORITIES];

static volatile UBaseType_t uxCurrentNumberOfTasks = (UBaseType_t)0U;
static volatile BaseType_t xSchedulerRunning = pdFALSE;

/* 启动任务调度 */
void vTaskStartScheduler(void)
{
    xSchedulerRunning = pdTRUE;
    (void)xPortStartScheduler();
}

/* 初始化任务列表 */
static void prvInitialiseTaskLists(void)
{
    for (UBaseType_t uxPriority = (UBaseType_t)0U; uxPriority < (UBaseType_t)configMAX_PRIORITIES; uxPriority++)
    {
        vListInitialise(&(pxReadyTasksLists[uxPriority]));
    }
}

static void prvAddNewTaskToReadyList(TCB_t *pxNewTCB)
{
    uxCurrentNumberOfTasks++;
    if (pxCurrentTCB == NULL)
    {
        pxCurrentTCB = pxNewTCB;
        if (uxCurrentNumberOfTasks == (UBaseType_t)1)
        {
            prvInitialiseTaskLists();
        }
    }
    vListInsertEnd(&(pxReadyTasksLists[pxNewTCB->uxPriority]), &(pxNewTCB->xStateListItem));
}

/**
 * @brief 初始化第一个任务，初始化第一个任务的任务控制块
 *
 * @param pcName        任务名称
 *
 * @param uxStackDepth  任务栈大小
 * @param pxTaskCode    任务函数
 * @param pvParameters  任务参数
 *
 * @param uxPriority    任务优先级
 *
 * @param pxCreatedTask 用户使用任务控制块句柄地址
 * @param pxNewTCB      新任务控制块指针，要赋值
 *
 */
static void prvInitialiseNewTask(TaskFunction_t pxTaskCode,
                                 const char *const pcName,
                                 const StackType_t uxStackDepth,
                                 void *const pvParameters,
                                 UBaseType_t uxPriority,
                                 TaskHandle_t *const pxCreatedTask,
                                 TCB_t *pxNewTCB)
{
    // 初始化任务名
    if (pcName != NULL)
    {
        for (UBaseType_t i = (UBaseType_t)0; i < (UBaseType_t)configMAX_TASK_NAME_LEN; i++)
        {
            pxNewTCB->pcTaskName[i] = pcName[i];
            if (pcName[i] == (char)0x00)
            {
                break;
            }
        }
        pxNewTCB->pcTaskName[configMAX_TASK_NAME_LEN - 1U] = '\0'; //  字符串结束标志位
    }

    // 初始化栈顶指针
    StackType_t *pxTopOfStack;
    // cortex-m是portSTACK_GROWTH=-1<0，也就是栈从顶部开始，压栈指针减小，弹栈指针增加，可以选择另一种类型，这里没写
    pxTopOfStack = pxNewTCB->pxStack + (uxStackDepth - (StackType_t)1);
    // 使得pxTopOfStack能够整除8，即栈顶8字节对齐
    /*
    1. ARM EABI 标准要求
        ARM Embedded ABI（EABI）规定：所有函数调用时栈（SP）必须是 8 字节对齐的
        即使编译器默认只要求 4 字节对齐，为了调用标准库、浮点运算等，栈顶必须额外处理对齐
    2. Cortex-M 异常处理机制要求
        Cortex-M 硬件自动压栈 8 个寄存器（xPSR、PC、LR、R12、R0~R3）
        如果此时 PSP（Process Stack Pointer）未 8 字节对齐，会触发 硬件对齐 fault
        尤其当启用 FPU 时，还会额外自动压栈 S0–S15 和 FPSCR，这些是 8 字节对齐的结构体块
    3. 库函数调用稳定性
        某些编译器（如 ARMCC、GCC）生成的浮点函数或 memcpy, printf 等标准库，默认会假设 栈 8 字节对齐
        若不满足这个约定，调用库函数时就可能立即崩溃（HardFault）
    */
    pxTopOfStack = (StackType_t *)((uint32_t)pxTopOfStack & (~((uint32_t)0x0007)));
    pxNewTCB->pxTopOfStack = pxPortInitialiseStack(pxTopOfStack, pxTaskCode, pvParameters);

    // 初始化优先级
    pxNewTCB->uxPriority = uxPriority;

    // 初始化状态链表项(钩子)
    vListInitialiseItem(&(pxNewTCB->xStateListItem));
    listSE_LIST_ITEM_OWNER(&(pxNewTCB->xStateListItem), pxNewTCB);
    vListInitialiseItem(&(pxNewTCB->xEventListItem));
    listSE_LIST_ITEM_OWNER(&(pxNewTCB->xEventListItem), pxNewTCB);
    // 将用户用到的句柄指向新创建的TCB
    if (pxCreatedTask != NULL)
    {
        *pxCreatedTask = (TaskHandle_t)pxNewTCB;
    }
}

/**
 * @brief 私有函数，创建静态任务
 *
 * @param pcName            任务名称
 *
 * @param pxStackBuffer     任务栈缓冲区
 * @param uxStackDepth      任务栈大小
 *
 * @param pxTaskCode        任务函数
 * @param pvParameters      任务参数
 *
 * @param uxPriority        任务优先级
 *
 * @param pxTaskBuffer      任务控制块缓冲区，用户负责这块内存的生命周期
 * @param pxCreatedTask     用户使用的任务控制块句柄地址
 *
 * @note 将任务控制块指针pvNewTCB指向用户指定的pxCreatedTask静态内存。同时对该任务控制块的栈起始地址进行初始化。
 *       将用户分配好的缓冲区（buffer）给任务控制块。之后初始化任务控制
 */
static TCB_t *prvCreateStaticTask(TaskFunction_t pxTaskCode,
                                  const char *const pcName,
                                  const StackType_t uxStackDepth,
                                  void *const pvParameters,
                                  UBaseType_t uxPriority,
                                  StackType_t *const pxStackBuffer,
                                  StaticTask_t *const pxTaskBuffer,
                                  TaskHandle_t *const pxCreatedTask)
{
    // 使用pvNewTCB目的时每次不用(TCB_t*)pxTaskBuffer，源码中pxTaskBuffer是StaticTask_t类型，StaticTask_t是与TCB_t有同样结构的结构体，但是他的成员变量全是dummy，不会让用户知道其意义
    TCB_t *pvNewTCB;
    if ((pxStackBuffer != NULL) && (pxTaskBuffer != NULL)) // 静态分配的缓冲区存在，再执行下面操作
    {
        // 初始化缓冲区
        // 让pvNewTCB指向用户静态分配的缓冲区，TCB要存储到这个地方；
        pvNewTCB = (TCB_t *)pxTaskBuffer;
        // 清空pvNewTCB，或用户指定buffer(pxTaskBuffer)
        // 第一个void:避免 “返回值未使用” 的编译警告
        // 第二个void:memset 接收 void *；消除编译器类型警告
        // 0x00:十六进制的0000 0000，填充一字节
        memset((void *)pvNewTCB, 0x00, sizeof(TCB_t));
        pvNewTCB->pxStack = pxStackBuffer;

        // 缓冲区添加数据
        prvInitialiseNewTask(pxTaskCode, pcName, uxStackDepth, pvParameters, uxPriority, pxCreatedTask, pvNewTCB);
    }
    else
    {
        pvNewTCB = NULL;
    }
    return pvNewTCB;
}

/**
 * @brief 创建静态任务
 *
 * @param pcName            任务名称
 *
 * @param pxStackBuffer     任务栈缓冲区
 * @param uxStackDepth      任务栈大小      用于获取栈顶初始指针
 *
 * @param pxTaskCode        任务函数
 * @param pvParameters      任务参数        用于任务基本信息需要先压入栈
 *
 * @param uxPriority        任务优先级
 *
 * @param pxTaskBuffer      任务控制块缓冲区，用户负责这块内存的生命周期
 * @return xReturn 任务句柄，公共 API 抽象句柄用户只能拿它当不透明句柄，传给 vTaskSuspend() 等
 * @note xReturn==pxNewTCB==pxTaskBuffer，指向同一块TCB，pxNewTCB用于freertos内部使用
 */
TaskHandle_t xTaskCreateStatic(TaskFunction_t pxTaskCode,
                               const char *const pcName,
                               const StackType_t uxStackDepth,
                               void *const pvParameters,
                               UBaseType_t uxPriority,
                               StackType_t *const pxStackBuffer,
                               StaticTask_t *const pxTaskBuffer)
{
    TaskHandle_t xReturn = NULL;
    TCB_t *pxNewTCB;

    pxNewTCB = prvCreateStaticTask(pxTaskCode, pcName, uxStackDepth, pvParameters, uxPriority, pxStackBuffer, pxTaskBuffer, &xReturn);

    if (pxNewTCB != NULL)
    {
        prvAddNewTaskToReadyList(pxNewTCB);
    }
    return xReturn;
}

extern StaticTask_t Task1TCB;
extern StaticTask_t Task2TCB;
void vTaskSwitchContext(void)
{
    /* 两个任务轮流切换 */
    if (pxCurrentTCB == (TCB_t *)(&Task1TCB))
    {
        pxCurrentTCB = (TCB_t *)(&Task2TCB);
    }
    else
    {
        pxCurrentTCB = (TCB_t *)(&Task1TCB);
    }
}
