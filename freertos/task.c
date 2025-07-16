#include <string.h>
#include "task.h"

#if (configUSE_PORT_OPTIMISED_TASK_SELECTION == 0)
#define taskRECORD_READY_PRIORITY(uxPriority) \
    do                                        \
    {                                         \
        if ((uxPriority) > uxTopUsedPriority) \
            uxTopUsedPriority = (uxPriority); \
    } while (0)
#define taskSELECT_HIGHEST_PRIORITY_TASK()                                              \
    do                                                                                  \
    {                                                                                   \
        UBaseType_t uxTopPriority = uxTopReadyPriority;                                 \
        while (listLIST_IS_EMPTY(&(pxReadyTasksLists[uxTopPriority])))                  \
        {                                                                               \
            configASSERT(uxTopPriority);                                                \
            --uxTopPriority;                                                            \
        }                                                                               \
        listGET_OWNER_OF_NEXT_ENTRY(pxCurrentTCB, &(pxReadyTasksLists[uxTopPriority])); \
        uxTopReadyPriority = uxTopPriority;                                             \
    } while (0)
#else  // 如果不是用硬件优化的方法确定目前最高优先级
// 将uxTopReadyPriority的二进制某一位置一，表示该优先级就绪队列出现了任务
#define taskRECORD_READY_PRIORITY(uxPriority) \
    portRECORD_READY_PRIORITY((uxPriority), uxTopReadyPriority)
/** 将优先级最高的就绪队列中的任务进行循环调用
 *  这个宏每次任务切换Yield都得调用到，
 *        每次systick中断切换任务都得调用到，
 *        至少每个1tick调用一次，
 *        若系统最高优先级不变，
 *        listGET_OWNER_OF_NEXT_ENTRY的迭代性质（pxIndex）会使同优先级的任务被迭代调用
 *        从而实现了时间片轮转调度，但是调度时间固定为1tick
 */
#define taskSELECT_HIGHEST_PRIORITY_TASK()                                              \
    do                                                                                  \
    {                                                                                   \
        UBaseType_t uxTopPriority;                                                      \
        portGET_HIGHEST_PRIORITY(uxTopPriority, uxTopReadyPriority);                    \
        configASSERT(listCURRENT_LIST_LENGTH(&(pxReadyTasksLists[uxTopPriority])) > 0); \
        listGET_OWNER_OF_NEXT_ENTRY(pxCurrentTCB, &(pxReadyTasksLists[uxTopPriority])); \
    } while (0)
#define taskRESET_READY_PRIORITY(uxPriority)                                               \
    do                                                                                     \
    {                                                                                      \
        if (listCURRENT_LIST_LENGTH(&(pxReadyTasksLists[(uxPriority)])) == (UBaseType_t)0) \
        {                                                                                  \
            portRESET_READY_PRIORITY((uxPriority), (uxTopReadyPriority));                  \
        }                                                                                  \
    } while (0)
#endif  // 用硬件优化的方法确定目前的最高优先级，同时还有更多的功能，比如确定某一优先级就绪队列是否存在任务

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
    volatile StackType_t *pxTopOfStack;         // 必须在结构体的顶部，任务启动与切换时会直接访问
    ListItem_t xStateListItem;                  // 挂载在就绪/阻塞/挂起队列的钩子
    ListItem_t xEventListItem;
    UBaseType_t uxPriority;                     // 任务优先级，最大值由freertos_config.h中configMAX_PRIORITIES设置
    StackType_t *pxStack;                       // 任务的栈顶指针
    char pcTaskName[configMAX_TASK_NAME_LEN];   // 任务名字
} tskTCB;             // 后面不是用tskTCB而是用TCB_t，为了版本兼容
typedef tskTCB TCB_t; // TCB_t 是系统私有不被外部使用的类型

// 这个指针式volatile的，不是其指向的内容是volatile的
TCB_t *volatile pxCurrentTCB = NULL;

// 就绪，阻塞队列
static List_t pxReadyTasksLists[configMAX_PRIORITIES];
static List_t xDelayedTaskList1, xDelayedTaskList2;
static List_t *volatile pxDelayedTaskList;
static List_t *volatile pxOverflowDelayedTaskList;                      

static volatile UBaseType_t uxCurrentNumberOfTasks = (UBaseType_t)0U;   // 现在总任务数
static volatile UBaseType_t uxTopReadyPriority = tskIDLE_PRIORITY;      // 二进制中每一位置一表示由该优先级的就绪任务
static volatile BaseType_t xSchedulerRunning = pdFALSE;                 // 表示调度器是否已经玉兴
static volatile BaseType_t xNumOfOverflows = (BaseType_t)0;             // xTickCount 溢出次数
static volatile TickType_t xTickCount = (TickType_t)0U;                 // 系统滴答时钟，每次systick中断加一
static volatile TickType_t xNextTaskUnblockTime = (TickType_t)0U;       // 最小解阻塞时间，xTickCount计时到这个数需要解阻塞一些阻塞任务

static TaskHandle_t xIdleTaskHandle;

// vDelayTask调用的将运行态的任务转化成就绪态
static void prvAddCurrentTaskToDelayedList(const TickType_t xTicksToDelay)
{
    if (uxListRemove(&(pxCurrentTCB->xStateListItem)) == (UBaseType_t)0)
    {   // 将这个任务从就绪队列中移去，同时如果就绪队列为空后，将对应位的uxTopReadyPriority置零，表示该优先级没有任务了
        portRESET_READY_PRIORITY(pxCurrentTCB->uxPriority, uxTopReadyPriority);
    }

    // 设置该进入阻塞态的任务阻塞时间为现在的xTickCount加上该任务的阻塞时间，到时间后将在systick中断中被处理
    TickType_t xTimeToWake = xTickCount + xTicksToDelay;
    listSET_LIST_ITEM_VALUE(&(pxCurrentTCB->xStateListItem), xTimeToWake);

    if (xTimeToWake < xTickCount)
    {   // 如果解阻塞时间小于xTickCount，表示xTimeToWake出现了溢出，需要将任务加入溢出阻塞队列。
        // 等到xTickCount也溢出的时候，两个阻塞队列将调换，之后处理的将是这个溢出阻塞队列。
        // vListInsert 根据任务解阻塞时间排序，解阻塞时间越近，插入的位置跟靠头部。
        vListInsert(pxOverflowDelayedTaskList, &(pxCurrentTCB->xStateListItem));
        // xNextTaskUnblockTime设置由systick中断中调换溢出队列后设置
    }
    else
    {   // 如果解阻塞时间大于xTickCount，表示没有溢出，正常的设置xNextTaskUnblockTime
        vListInsert(pxDelayedTaskList, &(pxCurrentTCB->xStateListItem));
        if (xTimeToWake < xNextTaskUnblockTime)
        {   // 设置最小解阻塞时间
            xNextTaskUnblockTime = xTimeToWake;
        }
    }
}
// 将调用该函数的任务阻塞。即把他加入组设队列中，并且设置好最小阻塞时间
void vTaskDelay(const TickType_t xTicksToDelay)
{
    prvAddCurrentTaskToDelayedList(xTicksToDelay);
    taskYIELD();
}

/* 空闲任务 */
static void prvIdleTask(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
    }
}

/* 获取空闲任务的静态缓冲区地址与函数栈缓冲区地址 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, StackType_t *puxIdleTaskStackSize)
{ // static 变量在函数运行完也不会消失，是全局变量
    static StaticTask_t xIdleTaskTCB;                               // 空闲任务TCB缓冲区
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];   // 空闲任务函数栈缓冲区

    *ppxIdleTaskTCBBuffer = &(xIdleTaskTCB);
    *ppxIdleTaskStackBuffer = &(uxIdleTaskStack[0]);
    *puxIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/* 创建空闲任务 */
static BaseType_t prvCreateIdleTasks(void)
{
    BaseType_t xReturn = pdPASS;
    StaticTask_t *pxIdleTaskTCBBuffer = NULL;   // 空闲任务TCB缓冲区地址
    StackType_t *pxIdleTaskStackBuffer = NULL;  // 空闲任务函数栈缓冲区地址
    StackType_t uxIdleTaskStackSize;            // 空闲任务函数栈的大小
    // 获取空闲任务的静态缓冲区地址与函数栈缓冲区地址
    vApplicationGetIdleTaskMemory(&pxIdleTaskTCBBuffer, &pxIdleTaskStackBuffer, &uxIdleTaskStackSize);
    // 创建空闲任务
    xIdleTaskHandle = xTaskCreateStatic((TaskFunction_t)prvIdleTask,
                                        "IDLE",
                                        uxIdleTaskStackSize,
                                        NULL,
                                        tskIDLE_PRIORITY,
                                        pxIdleTaskStackBuffer,
                                        pxIdleTaskTCBBuffer);
    if (xIdleTaskHandle == NULL)
    {   // 若空闲任务创建失败
        xReturn = pdFAIL;
    }
    return xReturn;
}

/* 启动任务调度 */
void vTaskStartScheduler(void)
{
    if (prvCreateIdleTasks() == pdPASS)
    {   // 若空闲任务创建成功
        portDISABLE_INTERRUPTS();               // 关中断，防止设置完systick后，发生systick中断，运行中断函数导致错误
        xNextTaskUnblockTime = portMAX_DELAY;   // 下次任务阻塞结束时间为最大
        xSchedulerRunning = pdTRUE;             // 表示开始启动调度器
        xTickCount = (TickType_t)0U;            // 初始化tickCount，systick一开始的调度次数为0
        (void)xPortStartScheduler();            // 启动任务调度
    }
}


// 将任务添加到就绪队列中，同时将uxTopReadyPriority所在优先级位置一，表示该优先级的就绪队列有任务了
#define prvAddTaskToReadyList(pxTCB)                                                           \
    do                                                                                         \
    {                                                                                          \
        taskRECORD_READY_PRIORITY((pxTCB)->uxPriority);                                        \
        vListInsertEnd(&(pxReadyTasksLists[(pxTCB)->uxPriority]), &((pxTCB)->xStateListItem)); \
    } while (0)


/* 初始化任务列表，包括所有优先级的队列，阻塞队列 */
static void prvInitialiseTaskLists(void)
{
    for (UBaseType_t uxPriority = (UBaseType_t)0U; uxPriority < (UBaseType_t)configMAX_PRIORITIES; uxPriority++)
    {
        vListInitialise(&(pxReadyTasksLists[uxPriority]));
    }
    // 初始化队列
    vListInitialise(&xDelayedTaskList1);
    vListInitialise(&xDelayedTaskList2);

    // 用volatile的指针指向两个阻塞队列，当出现xTickCount溢出时需要调换两个指针指向。
    pxDelayedTaskList = &xDelayedTaskList1;
    pxOverflowDelayedTaskList = &xDelayedTaskList2;
}

/* 将新创建的任务加入到就绪队列中，如果是第一次创建任务则初始化就绪队列 */
static void prvAddNewTaskToReadyList(TCB_t *pxNewTCB)
{
    // 确保中断不会在列表更新的时候访问列表，
    taskENTER_CRITICAL();
    {
        // 现存任务加一
        uxCurrentNumberOfTasks++;
        if (pxCurrentTCB == NULL)
        { // 若pxCurrentTCB还未初始化
            pxCurrentTCB = pxNewTCB;
            if (uxCurrentNumberOfTasks == (UBaseType_t)1)
            { // 若是第一个创建的任务，要初始化队列
                prvInitialiseTaskLists();
            }
        }
        else
        { // 若pxCurrentTCB已指向一个任务
            if (xSchedulerRunning == pdFALSE)
            { // 若调度器还未启动，那可以根据现在创建的任务优先级，让pxCurrentTCB指向优先级最高的最近一个任务
                if (pxCurrentTCB->uxPriority <= pxNewTCB->uxPriority)
                {
                    pxCurrentTCB = pxNewTCB;
                }
            }
        }
        // 将任务添加到就绪队列中，同时将uxTopReadyPriority所在优先级位置一，表示该优先级的就绪队列有任务了
        prvAddTaskToReadyList(pxNewTCB);
    }
    // 在调度器未开启前，uxCriticalNesting=0xaaaaaaaa，并不能关中断。basepri将一直保持0xbf,b=11,为进临界区设置。
    taskEXIT_CRITICAL();
}

/* 在已经分配好缓冲区的TCB的基础上，根据用户提供的任务信息，初始化TCB*/
static void prvInitialiseNewTask(TaskFunction_t pxTaskCode,         // 任务函数
                                 const char *const pcName,          // 任务名
                                 const StackType_t uxStackDepth,    // 函数栈大小
                                 void *const pvParameters,          // 任务参数
                                 UBaseType_t uxPriority,            // 任务优先级
                                 TaskHandle_t *const pxCreatedTask, // 任务句柄
                                 TCB_t *pxNewTCB)                   // 任务控制块
{
    /* 初始化任务名 */
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

    /* 初始化栈顶指针 */
    /** 解释
     * 1) cortex-m 是 portSTACK_GROWTH = -1 < 0，
     *  也函数栈底是是高地址开始的，压栈指针减小，弹栈指针增加
     * 2) pxTopOfStack能够整除8，即栈顶8字节对齐
     *
     *  1. ARM EABI 标准要求
     *      ARM Embedded ABI（EABI）规定：所有函数调用时栈（SP）必须是 8 字节对齐的
     *      即使编译器默认只要求 4 字节对齐，为了调用标准库、浮点运算等，栈顶必须额外处理对齐
     *  2. Cortex-M 异常处理机制要求
     *      Cortex-M 硬件自动压栈 8 个寄存器（xPSR、PC、LR、R12、R0~R3）
     *      如果此时 PSP（Process Stack Pointer）未 8 字节对齐，会触发 硬件对齐 fault
     *      尤其当启用 FPU 时，还会额外自动压栈 S0–S15 和 FPSCR，这些是 8 字节对齐的结构体块
     */
    StackType_t *pxTopOfStack = pxNewTCB->pxStack + (uxStackDepth - (StackType_t)1);
    pxTopOfStack = (StackType_t *)((uint32_t)pxTopOfStack & (~((uint32_t)0x0007)));
    pxNewTCB->pxTopOfStack = pxPortInitialiseStack(pxTopOfStack, pxTaskCode, pvParameters);

    /* 初始化优先级*/
    if (uxPriority >= (UBaseType_t)configMAX_PRIORITIES)
    {
        uxPriority = (UBaseType_t)configMAX_PRIORITIES - 1;
    }
    pxNewTCB->uxPriority = uxPriority;

    /* 初始化状态链表项(钩子)的所有链表与所有任务 */
    vListInitialiseItem(&(pxNewTCB->xStateListItem));
    listSE_LIST_ITEM_OWNER(&(pxNewTCB->xStateListItem), pxNewTCB);
    vListInitialiseItem(&(pxNewTCB->xEventListItem));
    listSE_LIST_ITEM_OWNER(&(pxNewTCB->xEventListItem), pxNewTCB);

    /* 将用户用到的句柄指向新创建的TCB */
    if (pxCreatedTask != NULL)
    {
        *pxCreatedTask = (TaskHandle_t)pxNewTCB;
    }
}

/* 私有函数，创建静态任务*/
static TCB_t *prvCreateStaticTask(TaskFunction_t pxTaskCode,         // 任务的函数指针，entry
                                  const char *const pcName,          // 任务名
                                  const StackType_t uxStackDepth,    // 任务栈深度
                                  void *const pvParameters,          // 任务函数的参数(void *)
                                  UBaseType_t uxPriority,            // 任务优先级
                                  StackType_t *const pxStackBuffer,  // 静态任务栈缓冲区
                                  StaticTask_t *const pxTaskBuffer,  // 静态任务控制块缓冲区
                                  TaskHandle_t *const pxCreatedTask) // 创建的任务句柄
{
    // 使用pvNewTCB目的时每次不用(TCB_t*)pxTaskBuffer，StaticTask_t是与TCB_t有同样结构的结构体，但是他的成员变量全是dummy，不会让用户知道其意义
    TCB_t *pvNewTCB;
    if ((pxStackBuffer != NULL) && (pxTaskBuffer != NULL))
    {                                                  // 静态分配的缓冲区存在，再执行下面操作
        pvNewTCB = (TCB_t *)pxTaskBuffer;              // 让pvNewTCB指向用户静态分配的TCB缓冲区，TCB要存储到这个地方；
        memset((void *)pvNewTCB, 0x00, sizeof(TCB_t)); // 清空pvNewTCB，或用户指定buffer(pxTaskBuffer)
        pvNewTCB->pxStack = pxStackBuffer;             // 让TCB的栈指针指向用户静态分配的栈缓冲区，任务函数的栈在这个地址

        // 缓冲区添加数据
        prvInitialiseNewTask(pxTaskCode, pcName, uxStackDepth, pvParameters, uxPriority, pxCreatedTask, pvNewTCB);
    }
    else
    { // 如果用户没有自己静态分配的缓冲区
        pvNewTCB = NULL;
    }
    return pvNewTCB;
}

/* 用户用的，创建一个静态任务*/
TaskHandle_t xTaskCreateStatic(TaskFunction_t pxTaskCode,        // 任务函数指针，void(void *)
                               const char *const pcName,         // 任务名称
                               const StackType_t uxStackDepth,   // 任务栈深度
                               void *const pvParameters,         // 任务函数的参数(void *)
                               UBaseType_t uxPriority,           // 任务优先级，优先级的数量由configMAX_PRIORITIES决定
                               StackType_t *const pxStackBuffer, // 任务栈起始指针
                               StaticTask_t *const pxTaskBuffer) // 静态任务控制块的存储区域/缓冲区
{
    TaskHandle_t xReturn = NULL; // 返回的句柄，实际上是TCB_t *指针
    TCB_t *pxNewTCB;             // 任务控制块的操作指针，方便操作，StaticTask_t，TaskHandle_t都是隐藏结构体内部变量的数据结构，给用户使用。

    // 私有的任务控制块函数创建函数
    pxNewTCB = prvCreateStaticTask(pxTaskCode, pcName, uxStackDepth, pvParameters, uxPriority, pxStackBuffer, pxTaskBuffer, &xReturn);

    if (pxNewTCB != NULL)
    { // 任务如果创建成功，就加入到就绪队列中
        prvAddNewTaskToReadyList(pxNewTCB);
    }
    return xReturn;
}

void vTaskSwitchContext(void)
{
    taskSELECT_HIGHEST_PRIORITY_TASK();
}

// 设置最小解阻塞时间，根据阻塞队列是否为空，或阻塞队列头个任务(解阻塞时间最小)来确定
static void prvResetNextTaskUnblockTime(void)
{
    if (listLIST_IS_EMPTY(pxDelayedTaskList))
    {
        xNextTaskUnblockTime = portMAX_DELAY;
    }
    else
    {
        xNextTaskUnblockTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY(pxDelayedTaskList);
    }
}

// 延时队列切换
#define taskSWITCH_DELAYED_LISTS()                            \
    do                                                        \
    {                                                         \
        List_t *pxTemp;                                       \
        configASSERT((listLIST_IS_EMPTY(pxDelayedTaskList))); \
                                                              \
        pxTemp = pxDelayedTaskList;                           \
        pxDelayedTaskList = pxOverflowDelayedTaskList;        \
        pxOverflowDelayedTaskList = pxTemp;                   \
        xNumOfOverflows = (BaseType_t)(xNumOfOverflows + 1);  \
        prvResetNextTaskUnblockTime();                        \
    } while (0)
// 每次systick中断都会调用此函数。设置最小解阻塞时间，并把解阻塞时间小于xTickCount的任务切换为就绪状态，返回值是是否进行任务切换
BaseType_t xTaskIncrementTick(void)
{
    BaseType_t xSwitchRequired = pdFALSE;   // 是否进行切换标志位
    xTickCount++;                           // 系统总滴答次数
    if (xTickCount == (TickType_t)0U)       // 若滴答次数变为零，表示溢出了
    {
        taskSWITCH_DELAYED_LISTS();         // 进行延时队列的调换，切换的溢出延迟队列做新延迟队列
    }
    if (xTickCount >= xNextTaskUnblockTime) // 若最近的延时任务延时到期
    {
        for (;;)                            // 把所有延时到期的任务都加到就绪队列中
        {
            if (listLIST_IS_EMPTY(pxDelayedTaskList))   
            {   // 若延时队列为空，意味else把所有阻塞任务都处理完了，现在没有阻塞任务了，那等待解阻塞的时间设为最大。
                xNextTaskUnblockTime = portMAX_DELAY;   
                break;
            }
            else
            {   // 延时队列不为空，获取时间上最近要解阻塞的任务进行处理
                TCB_t *pxTCB = (TCB_t *)listGET_OWNER_OF_HEAD_ENTRY(pxDelayedTaskList);
                TickType_t xItemValue = listGET_LIST_ITEM_VALUE(&(pxTCB->xStateListItem));

                if (xTickCount < xItemValue)
                {   // 如果最近的要解阻塞的任务时间还没到，那把最小解阻塞时间设置为阻塞队列中最小解阻塞时间
                    xNextTaskUnblockTime = xItemValue;
                    break;
                }

                // 最近的要解阻塞的任务时间已经到了，总阻塞队列中删除这个任务并加入到就绪队列中
                (void)uxListRemove(&(pxTCB->xStateListItem));
                prvAddTaskToReadyList(pxTCB);

                #if (configUSE_PREEMPTION == 1)
                {   // 优先级调度
                    if (pxTCB->uxPriority >= pxCurrentTCB->uxPriority)
                    {   // 如果新加入的任务优先级大于等于现在任务优先级，那么需要进行任务切换
                        xSwitchRequired = pdTRUE;
                    }
                }
                #endif /* configUSE_PREEMPTION */
            }
        }
    } /* xConstTickCount >= xNextTaskUnblockTime */

    #if ((configUSE_PREEMPTION == 1) && (configUSE_TIME_SLICING == 1))
    {   // 时间片轮转调度
        if (listCURRENT_LIST_LENGTH(&(pxReadyTasksLists[pxCurrentTCB->uxPriority])) > (UBaseType_t)1)
        {   // 如果当前优先级的就绪队列总有多个任务，那么每一次systick都需要进行时间片轮转调度
            xSwitchRequired = pdTRUE;
        }
    }
    #endif /* ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) ) */
    return xSwitchRequired;
}
