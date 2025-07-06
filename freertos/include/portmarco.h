#ifndef PORTMARCO_H
#define PORTMARCO_H
/*
*   FreeRTOS 都会将标准的 C 数据类型用 typedef 重新取一个类型名
*/
#include <stdint.h> 
#include <stddef.h>


/*
* 表示这是一个无符号基础类型，常用于表示任务优先级、队列长度、信号量计数值等系统内核相关的无符号整数。
*   U	    Unsigned（无符号）
*   Base	基础类型（basic type），不依赖具体结构或平台
*   Type_t	类型后缀，表明这是一个通过 typedef 定义的标准类型
*   long    long的大小取决与平台，通常是32位或64位
*/
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
    #define portMAX_DELAY (TickType_t) 0xffff
#else
    typedef uint32_t TickType_t;
    #define portMAX_DELAY (TickType_t) 0xffffffffUL
#endif

#endif