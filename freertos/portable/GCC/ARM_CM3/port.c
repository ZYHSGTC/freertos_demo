/*跟具体芯片有关的函数*/
#include "FreeRtos.h"
#include "task.h"

/** GCC 内联汇编基本结构
 *      __asm volatile (
 *         "assembly code here"
 *         : output operands        // 输出参数
 *         : input operands         // 输入参数
 *         : clobbered registers    // 被修改的寄存器
 *      );
 *  参数约束与修饰符
 *      约束	含义	                                    示例
 *      "r"	    使用任意通用寄存器              	        寄存器操作数
 *      "i"	    立即数（常量）	                            如 #0x20
 *      "m"	    内存地址	                                变量地址
 *      "l"	    低寄存器（r0~r7）	                        用于某些指令只支持低寄存器
 *      "I"	    适用于 MOV 指令的合法立即数范围（0~255）	特定用途
 * 
 *      修饰符	含义	                                    示例
 *      "=" 	该变量（不是只）只写（write-only）	        "=r" (x)
 *      "+" 	读写（read-write）	                        "+r" (x)
 *      "&" 	早期绑定（early clobber），表示这个输出不能和任何输入共享寄存器	"=&r" (x)
 *      "%" 	提示优化器，两个操作数可以交换顺序	        "%0, %1"
 * 
 *  例子
 *  1. "=r" (ulOriginalBASEPRI)：
 *      告诉 GCC，“我需要一个寄存器来保存输出结果”，并将该寄存器的值写回变量 ulOriginalBASEPRI。
 *      汇编执行完毕后，GCC 会自动将寄存器 %0 的值写入 ulOriginalBASEPRI 变量中。
 *  2. : clobbered registers  
 *       : "r0"：告诉编译器：“我在汇编里用了 r0 寄存器，可能改了它的值”；
 *               这样编译器就不会把其他变量放在 r0 上，也不会假设 r0 的值没变；
 *       : "memory"：“我写了任意内存地址”，所以不能重排前后内存访问；类似于插入一个编译器级别的内存屏障；
 *          a = 1;
 *          (1) __asm volatile("" ::: "memory");
 *          (2) __asm volatile("dsb" ::: "memory");
 *          b = 2;
 *          (1) 编译器不会把 b = 2 移到 a = 1 前面；保证变量写入顺序；但是 CPU 还是可以乱序执行。
 *          (2) 此时不仅编译器不会重排，而且 CPU 也会等待 a = 1 写入完成后才执行 b = 2。
 */
/** thumb指令解析
 *| 指令  | 全称（英文原文）                    | 中文含义                                  |
 *| ----- | ----------------------------------- | ----------------------------------------- |
 *|  msr  | **Move to Special Register**        | 通用寄存器（不能是立即数）写入特殊寄存器  |
 *|  mrs  | **Move from Special Register**      | 从特殊寄存器读取到通用寄存器              |
 *|  mov  | **Move Register**                   | 把立即数放到通用寄存器中                  |
 *|  ldr  | **Load Register**                   | 加载内存到通用寄存器                      |
 *|  str  | **Store Register**                  | 存储通用寄存器内容到内存                  |
 *| ldmia | **Load Multiple Increment After**   | 加载多个寄存器内容到内存，加感叹号才递增  |
 *|  orr  | **Or Register**                     | 按位或                                    |
 *|  bx   | **Branch Exchange**                 | 跳转到此地址，异常返回，改变指令执行模式  |
 *| stmdb | **Store Multiple Decrement Before** | 存储多个寄存器内容到内存，加感叹号才递减  |
 *| ldmia | **Load Multiple Increment After**   | 加载多个寄存器内容到内存，加感叹号才递增  |
 */
/** cpu核心寄存器介绍 （任务栈必须要压栈存有）
 *       1.r0-r12 是通用寄存器，可以在两种模式下自由使用；
 *       2.r13（SP）有两个版本：MSP（Main Stack Pointer）和 PSP（Process Stack Pointer）；
 *       3.r14（LR）在异常处理中有特殊用途（EXC_RETURN）；
 *          异常模式（svc,pendsv,systick），Handler模式：
 *              进入异常中断后，硬件自动将 r14 设置为 EXC_RETURN 值（如 0xFFFFFFFD）；
 *              异常处理完成后，通过 bx r14 或 mov pc, lr 触发异常返回；
 *              根据 r14 的值恢复运行状态（Thread/Handler Mode + 使用哪个 SP）
 *                  EXC_RETURN 	含义
 *                  0xFFFFFFF1	返回到 Handler Mode（使用当前栈指针）
 *                  0xFFFFFFF9	返回到 Thread Mode，并使用 MSP
 *                  0xFFFFFFFD	返回到 Thread Mode，并使用 PSP
 *          Thread Mode：
 *              保存pc运行后的返回地址；
 *       4.r15（PC）用于控制程序流；
 */

// 栈初始化，加括号: 防止如 #define foo 1 << 2: (foo + 3) 被解读成 1 << (2 + 3) 的问题，实际应该是 (1<<2)+3
/* Cormex-M3 的 SCB_SHPR2/3 寄存器:分别用于设置系统异常中断（SVC,SYSTICK,PendSV）优先级。NVIC_IPRx用于设置外设中断优先级。两种中断可以互相打断。
 * port：表示这是与硬件端口（Port）相关的定义，FreeRTOS 里常用于和具体 CPU 架构有关的代码。
 * NVIC：Nested Vectored Interrupt Controller（嵌套向量中断控制器），Cortex-M 的核心模块，管理中断优先级和响应。
 * SHPR3：System Handler Priority Register 3，系统处理器中断的优先级寄存器（编号 3）
 * REG：表示这是一个寄存器定义（#define 出来的地址）
 */
#define portNVIC_SHPR2_REG (*((volatile uint32_t *)0xe000ed1c))
#define portNVIC_SHPR3_REG (*(volatile uint32_t *)0xe000ed20)
/*系统异常中断的最低优先级，数值越大优先级越低*/
#define portMIN_INTERRUPT_PRIORITY (255UL)
#define portNVIC_SYSTICK_PRI ((uint32_t)(portMIN_INTERRUPT_PRIORITY << 24UL))
#define portNVIC_PENDSV_PRI ((uint32_t)(portMIN_INTERRUPT_PRIORITY << 16UL))

/** xPSR（Program Status Register，程序状态寄存器）是 Cortex-M 系列内核中用于表示程序状态的重要寄存器，它是多个寄存器（APSR、IPSR、EPSR）的组合
 *  | 位数    | 名称                                     | 含义                              |
 *  | ------- | ---------------------------------------- | --------------------------------- |
 *  | 31      | N (Negative)                             | 运算结果为负数时置 1              |
 *  | 30      | Z (Zero)                                 | 运算结果为 0 时置 1               |
 *  | 29      | C (Carry)                                | 进位/借位标志                     |
 *  | 28      | V (Overflow)                             | 溢出标志                          |
 *  | 27      | Q (Saturation)                           | 饱和计算标志（通常用于 DSP 指令） |
 *  | 26-23   | Reserved                                 | 保留                              |
 *  | 24（T） | Thumb状态位为1，表示CPU处于Thumb 模式。  | cortex-m之后thumb模式             |
 *  | 22-10   | IT\[1:7], IT\[0]                         | 条件执行（Thumb IT 状态）         |
 *  | 9-8     | Reserved                                 | 保留                              |
 *  | 7-0     | T-bit, Exception number                  |                                   |
*/
#define portINITIAL_XPSR (0x01000000)

static void prvTaskExitError(void)
{
    for (;;)
    {
    }
}

static UBaseType_t uxCriticalNesting = 0xaaaaaaaa; // 表示临界区嵌套了多少层

/** 初始化任务栈顶指针
 * @brief  初始化任务栈
 *
 * @note 在任务的栈上填充任务函数与参数。按照psp弹栈顺序、cpu压栈顺序，填充16个寄存器的值。
 */
StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack,
                                   TaskFunction_t pxCode,
                                   void *pvParameters)
{
    pxTopOfStack--; // 一开始pxTopOfStack时八字节对齐，所以需要减一，在他下方开始压栈；
    *pxTopOfStack = portINITIAL_XPSR;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)pxCode; // R15 PC
    /*
    LR（Link Register，链接寄存器）
        当你调用一个函数时（比如 BL func）：
        CPU 会自动将“下一条指令地址”保存到 LR 寄存器中
        当函数执行结束时，执行 BX LR，就能跳回到调用者的位置继续执行

        也就是说 LR = 返回地址，如果任务不是无限循环，那他执行完就会跳到任务退出错误函数prvTaskExitError里执行循环
    */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)prvTaskExitError; // R14 LR
    pxTopOfStack -= 5;                             // R1~R3 默认为零
    *pxTopOfStack = (StackType_t)pvParameters;
    pxTopOfStack -= 8; // R4~R11 默认为零

    return pxTopOfStack;
}

/** SVC 中断处理函数
 * @brief SVC 中断处理函数
 */
void vPortSVCHandler(void)
{
    // 进入Handler模式
    __asm volatile
    (
        // 软件手动弹栈给cpu r4-r11通用寄存器，并设置psp在栈顶，异常退出后，自动弹栈后面八字进入cpu寄存器
        "ldr r3, pxCurrentTCBConst2             \n" // 获取当前任务控制块指针，pxCurrentTCBConst2 指向 pxCurrentTCB
        "ldr r1, [r3]                           \n" // 获取当前任务控制块
        "ldr r0, [r1]                           \n" // 获取当前任务控制块结构体首成员，栈顶指针
        "ldmia r0!, {r4-r11}                    \n" // 软件手动加载到cpu中的寄存器
        "msr psp, r0                            \n" // 设置psp指向当前任务栈顶，退出handle模式后，自动加载后面八个字到cpu寄存器
        "isb                                    \n" // 保证前面指令完成才能设置屏蔽中断

        // 设置basepri (Base Priority Register) 为零，表示所有优先级的中断都不屏蔽，操作系统启动前可能被设置为某些值需要改回来
        "mov r0, #0                             \n"
        "msr basepri, r0                        \n"
        /** 1.为什么用 mov
         * mov r0, #0 是一条立即数指令，不需要访问内存，效率高。
         * ldr r0, =0 是伪指令，实际会被汇编器翻译成：
         *      ldr r0, [pc, #offset]
         *        ...
         *        .word 0
         * 2.为什么用 msr basepri, r0
         *  msr 是寄存器到寄存器的指令
         */

        /** 退出SVC中断，返回到thread模式，ps指针为PSP；
         * 自动弹栈xPSR，PC（任务入口地址），R14，R12，R3，R2，R1，R0（任务的形参）入cpu寄存器
         * 根据PC指针，R0形参，与xPSR程序状态寄存器，跳转到对应任务运行
         */
        "orr r14, #0xd                          \n"
        "bx r14                                 \n" 

        // 任务控制块指针
        ".align 4                               \n"
        "pxCurrentTCBConst2: .word pxCurrentTCB \n"
        /**
         * .align n: alignment 对齐，表示将当前地址对齐到 2^n 字节边界。
         *      16字节对齐，保守做法？？？
         */
    );
}

/** 启动第一个任务
 * @brief 更新handler模式时用到的主堆栈指针MSP（Main Stack Pointer），使能中断与异常
 * @note cortex-m3 用compiler 6 gcc编译的汇编代码
 */
static void prvPortStartFirstTask(void)
{
    // __asm volatile：告诉编译器插入原样汇编代码，不要优化掉；
    __asm volatile
    (
        // 初始化或更新MSP值，返回到上电复位的状态，即为主堆栈栈低
        /**ldr r0, =0xE000ED08 为什么有 =
         *      在 Thumb 模式下，立即数加载受限制（通常 ≤ 8~12 bit）
         *      他时告诉汇编器 请将值 0xE000ED08 放到某个内存区域，然后让 ldr r0, [...] 去读取它
         */
        " ldr r0, =0xE000ED08    \n" // 该地址是 System Control Block (SCB) 中的 VTOR（Vector Table Offset Register）的地址。VTOR 指向中断向量表的基地址。
        " ldr r0, [r0]           \n" // 读取 SCB_VTOR 的值，获取中断向量表的基地址。即vector_table，一般是0x00000000
        " ldr r0, [r0]           \n" // 读取中断向量表第一项，即第一个中断向量地址。指向系统主堆栈。
        /** 中断向量表格式
        *   __attribute__ ((section(".isr_vector")))
        *   const uint32_t *vector_table[] = {
        *       (uint32_t *)_estack, // 向量表第0项：MSP 初始化值
        *       Reset_Handler,       // 向量表第1项：复位中断入口地址
        *       NMI_Handler,
        *       HardFault_Handler,
        *       ...
        };*/
        " msr msp, r0            \n"

        // 使能全局中断
        " cpsie i                \n" // Change Processor State - Interrupt Enable：使能 IRQ 中断
        " cpsie f                \n" // Change Processor State - Fast Interrupt Request Enable，使能 FIQ 中断（Cortex-M 不支持，通常无效，作为保留指令存在）。
        " dsb                    \n" // Data Synchronization Barrier，前面内存数据储存加载完成之后，且对外可见在执行下面
        " isb                    \n" // Instruction Synchronization Barrier，刷新流水线，使后续指令在执行前能使用最新的处理器状态。
                                     // 后两句保证，中断使能写入并执行与MSP寄存器的值设置完毕

        // 软件触发中断，进入系统调用 0（SVC）异常处理，内核启动触发，只触发一次
        " svc 0                  \n" // Supervisor Call，产生 SVC（系统调用）异常，切入 RTOS 的 SVC_Handler
        " nop                    \n" // 空指令，常用于对齐或占位，方便调试或断点设置
                                     // 有时插入 nop 可以避免某些流水线异常，尤其是中断切换前。
        // 常量池指令，将之前 ldr =0xE000ED08 的大立即数常量放置于此处
        ".ltorg                 \n" // literal pool origin，相当于.word 0xE000ED08, ldr r0, =0xE000ED08会在这里读值
    );
}

/** 启动调度器
 * @brief 启动调度器
 *
 * @note 此函数会返回，如果返回则说明调度器启动失败
 */
BaseType_t xPortStartScheduler(void)
{
    /*🔹1. 为什么 PendSV 要设为最低优先级？
        PendSV 是用来做任务上下文切换的中断。
        没有特定的时间精度要求；只在没有更高优先级中断需要执行的时候再执行就行了。

        ✅ 所以设为最低优先级，可以保证：
        所有更高优先级的中断执行完再切任务；
        避免上下文切换过程打断紧急的中断服务；
        保证系统实时性和响应性。

    🔹2. 为什么 SysTick 也要设为最低优先级？
        SysTick 是系统节拍中断（tick timer），它每隔一个周期产生中断，用来触发任务调度等。
        它的主要作用是：让操作系统有节奏地判断是否需要进行任务切换。

        ✅ 设置为最低优先级的理由是：
        它不应该打断高优先级的中断（比如外设中断或 DMA 完成）；
        它只是“定时器通知”，并不是真正紧急的中断；
        如果高优先级中断正在处理，tick 可以稍后处理，不影响调度正确性。
    🔹3. 为什么要把 SVCall 设为最高优先级？
        SVCall（Supervisor Call）是用户任务通过 `svc` 指令进入内核的机制，
        在 FreeRTOS 中，它用于首次启动任务或执行特权切换时的处理。

        ✅ 设置为最高优先级的理由是：
        它涉及重要的系统管理操作（如首次任务启动）；
        如果 SVCall 被延迟，可能导致系统无法及时启动或切换；
        要保证它不会被其他中断（比如 SysTick 或外设中断）打断；
        避免在进入临界内核逻辑前发生上下文混乱。

        ⚠️ 注意：SVCall 是“软中断”，只有在执行 `svc` 指令时才会触发，
        并不是周期性发生的，不会影响系统实时性。
    */
    portNVIC_SHPR3_REG |= portNVIC_PENDSV_PRI;
    portNVIC_SHPR3_REG |= portNVIC_SYSTICK_PRI;
    portNVIC_SHPR2_REG = 0;

    uxCriticalNesting = 0;
    prvPortStartFirstTask();
    return pdFALSE;
}

/** 进入临界区
 * @brief 进入临界区
 */
void vPortEnterCritical(void)
{
    portDISABLE_INTERRUPTS();
    uxCriticalNesting++;
    if( uxCriticalNesting == 1 )       
    {
        // 保证不在中断中使用第一次临界区，不然出错
        // 原因是如果在中断外进入临界区，basepri将一直为11；临界区也将在回到第一层时退出
        //       如果在>=11的优先级的中断中进入临界区，那被<11优先级的中断抢占后将无法正常中断返回
        //       如果在<11的优先级中断中进入临界区，则不会出现问题
        //       但是11是用户定义的，操作系统只能完全禁止在中断中首次进入临界区
        configASSERT((portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK) == 0);
    }
}

/** 退出临界区，回到临界区第一层的时候才真正执行。但还是得和vPortEnterCritical成对调用
 * @brief 退出临界区
 * 
 * @note 不在第一层临界区不会使能中断，不会真正退出临界区，也就是不会讲basepri置0；
 */
void vPortExitCritical(void)
{
    // 如果当前临界区嵌套为0，即没有进入临界区，则出现错误
    configASSERT(uxCriticalNesting);
    uxCriticalNesting--;
    if(uxCriticalNesting == 0)
    {
        portENABLE_INTERRUPTS();
    }
}

/** PendSV 中断处理函数*/
void xPortPendSVHandler()
{
    /**
     * Pending Supervisor Call 一个系统异常（System Exception），编号为 14。
     * 它本质上是一个可以被软件挂起的异常，常用于 任务上下文切换
     *
     * SCB_ICSR的PENDSTCLR置一后，PendSV异常挂起，其他高优先级中断结束后会执行这个中断
     */
    __asm volatile
    (
        /** 手动压栈保存上下文，r4-r11。压栈完成保存栈顶指针到当前任务的任务块中
         *
         * 进入pendsv中断前硬件会自动根据ps(进入中断前是thread模式，ps = PSP)
         * 压栈xPSR，PC（任务入口地址），R14，R12，R3，R2，R1，R0（任务的形参）
         *
         * 我们需要将后面的8个参数手动压栈
         */
        "mrs r0, psp                            \n"
        "isb                                    \n"

        "ldr r3, pxCurrentTCBConst              \n" // 获取当前任务控制块指针，pxCurrentTCBConst2 指向 pxCurrentTCB
        "ldr r2, [r3]                           \n" // 获取当前任务控制块

        "stmdb r0!, {r4-r11}                    \n" // 手动压栈 r4-r11
        "str r0, [r2]                           \n" // 保存栈顶指针保证下次切换任务还能获得正确的栈顶
        

        /** 切换任务，改变pxCurrentTCB
         * 由于后面会运行保存vTaskSwitchContext，r3,r14可能被修改
         *  所以保存pxCurrentTCBConst2，之后不用再ldr
         *      保存r14，也就是进入异常是的值0xFFFFFFFD
         */
        "stmdb sp!, {r3, r14}                   \n" // 根据MSP(目前是Handler模式)压栈
        "mov r0, %0                             \n" // 屏蔽低优先级中断，%0是占位符(如printf)，由::"i"后面的常量替换
        "msr basepri, r0                        \n" // 关中断，进入临界区，\muOs与rtthread是全部关掉
        "bl vTaskSwitchContext                  \n" // 执行CurrentTCB的切换 
        "mov r0, #0                             \n" // 所有中断不屏蔽
        "msr basepri, r0                        \n" // 开中断，出临界区
        "ldmia sp!, {r3, r14}                   \n"


        // 软件手动弹栈给cpu r4-r11通用寄存器，并设置psp在栈顶，异常退出后，自动弹栈后面八字进入cpu寄存器
        "ldr r1, [r3]                           \n" // 获取当前任务控制块
        "ldr r0, [r1]                           \n" // 获取当前任务控制块结构体首成员，栈顶指针
        "ldmia r0!, {r4-r11}                    \n" // 软件手动加载到cpu中的寄存器
        "msr psp, r0                            \n" // 设置psp指向当前任务栈顶，退出handle模式后，自动加载后面八个字到cpu寄存器
        "isb                                    \n" // 保证前面指令完成才能设置屏蔽中断

        /** 退出PendSV中断， r14进入中断时就是#0xd，返回时候还是到thread模式，ps指针为PSP；
         * 自动弹栈xPSR，PC（任务入口地址），R14，R12，R3，R2，R1，R0（任务的形参）入cpu寄存器
         * 根据PC指针，R0形参，与xPSR程序状态寄存器，跳转到对应任务运行
         */
        "bx r14                                 \n"

        // 任务控制块指针
        ".align 4                               \n"
        "pxCurrentTCBConst: .word pxCurrentTCB  \n"
        :
        :"i"(configMAX_SYSCALL_INTERRUPT_PRIORITY) 
        );
}
