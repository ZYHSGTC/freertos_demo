#ifndef LIST_H
#define LIST_H
#include "FreeRtos.h"
#include "portmacro.h"

/*
 * @brief 双向链表节点
 * 描述:
 * x: 在freertos中表示特定的数据类型
 * v：void
 * p：指针
 * pv： void*
 */
typedef struct xLIST_ITEM
{
    TickType_t xItemValue;         // 辅助值，用于排序
    struct xLIST_ITEM *pxNext;     // 指向下一个节点
    struct xLIST_ITEM *pxPrevious; // 指向前一个节点
    void *pvOwner;                 // 节点所属者
    void *pvContainer;             // 节点所属的链表
} ListItem_t;

/*
 * @brief 双向链表迷你节点
 * 最轻量与简单的钩子，减少内存占用
 */
typedef struct xMINI_LIST_ITEM
{
    TickType_t xItemValue;
    struct xMINI_LIST_ITEM *pxNext;
    struct xMINI_LIST_ITEM *pxPrevious;
} MiniListItem_t;

/*
 * @brief 根节点
 * @discription 链表根节点
 * xListEnd是一个“虚拟节点”
 *   它并不属于任何任务或容器（即 pvOwner = NULL）；
 *   它不参与实际数据存储；
 *   它只是用来辅助实现链表操作的“锚点”，使得链表操作无需频繁判断边界条件。
 */
typedef struct xLIST
{
    UBaseType_t uxNumberOfItems;
    ListItem_t *pxIndex;     // 遍历链表的“游标”，初始指向 xListEnd，但可以在遍历时移动。
    MiniListItem_t xListEnd; // 哨兵节点，作为链表的边界标记，保证链表始终有头有尾，便于操作。
} List_t;

/*
 * @brief 初始化节点的拥有者
 * @param ListItem_t*与Owner(TCB_t)类型指针
 */
#define listSE_LIST_ITEM_OWNER(pxListItem, pxOwner) \
    ((pxListItem)->pvOwner = (void *)pxOwner)

/*
 * @brief 获取节点的拥有者
 * @param ListItem_t*
 * @return Owner(TCB_t)类型指针
 */
#define listGET_LIST_ITEM_OWNER(pxListItem) \
    ((pxListItem)->pvOwner)

/*
 * @brief 初始化节点排序辅助值
 * @param ListItem_t*，TickType_t
 */
#define listSET_LIST_ITEM_VALUE(pxListItem, xValue) \
    ((pxListItem)->xItemValue = (xValue))

/*
 * @brief 获取节点排序辅助值
 * @param ListItem_t*
 * @return TickType_t
 */
#define listGET_LIST_ITEM_VALUE(pxListItem) \
    ((pxListItem)->xItemValue)

/*
 * @brief 获取链表头节点的节点计数器的值
 * @param pxList*
 * @return TickType_t
 */
#define listGET_ITEM_VALUE_OF_HEAD_ENTRY(pxList) \
    ((((pxList)->xListEnd).pxNext)->xItemValue)

/*
 * @brief 获取链表头节点的指针
 * @param pxList*
 * @return ListItem_t*
 */
#define listGET_HEAD_ENTRY(pxList) \
    (((pxList)->xListEnd).pxNext)

/*
 * @brief 获取节点的下一个节点
 * @param ListItem_t*
 * @return ListItem_t*
 */
#define listGET_NEXT(pxListItem) \
    ((pxListItem)->pxNext)

/*
 * @brief 获取链表的根节点（末尾节点）
 * @param pxList*
 * @return ListItem_t*
 * @note const 指针是禁止修改的，返回结果是哨兵节点，没有pvOwner，pvContainer
 */
#define listGET_END_MARKER(pxList) \
    ((ListItem_t const *)(&((pxList)->xListEnd)))

/*
 * @brief 判断链表是否为空
 * @param pxList*
 * @return BaseType_t
 */
#define listLIST_IS_EMPTY(pxList) \
    ((BaseType_t)((pxList)->uxNumberOfItems == 0U))

/*
 * @brief 获取链表的节点数
 * @param pxList*
 * @return UBaseType_t
 */
#define listCURRENT_LIST_LENGTH(pxList) \
    ((pxList)->uxNumberOfItems)

/*
 * @brief * 获取链表Index指向节点的下一个节点的 OWNER，即 TCB，用于遍历链表获得所有Owner
 * @param pxList*
 * @return UBaseType_t
 * @note do { ... } while (0) 的作用: 1.  宏内定义的变量不影响外层 2. 可以用分号结尾，模拟函数调用 3. 形成整体，直接用于if下
 * @warning 对于空链表调用这个函数会导致访问未定义指针的问题
 */
#define listGET_OWNER_OF_NEXT_ENTRY(pxTCB, pxList)                                    \
    do                                                                                \
    {                                                                                 \
        List_t const *pxConstList = (pxList);                                         \
        pxConstList->pxIndex = (pxConstList)->pxIndex->pxNext;                        \
        if ((void *)((pxConstList)->pxIndex) == (void *)(&((pxConstList)->xListEnd))) \
        {                                                                             \
            (pxConstList)->pxIndex = (pxConstList)->pxIndex->pxNext;                  \
        }                                                                             \
        (pxTCB) = (pxConstList)->pxIndex->pvOwner;                                    \
    } while (0)

/*
 * @brief 初始化节点
 * @param pxItem 节点指针
 * @discription 节点初始化，初始化只需要让这个节点不属于任何链表，即将pvContainer设置为NULL，使用链表操作时必须调用它？
 * 写法	含义
 *   ListItem_t * ptr	指针变量，指向一个 ListItem_t，指针和指向内容都可变
 *   const ListItem_t * ptr	指向常量的指针，指向内容不可修改，指针本身可变
 *   ListItem_t * const ptr	常量指针，指针本身不可变，指向内容可修改，相当于引用，做提示作用；如果用ListItem_t * ptr也不会错误，因为函数传递的是形参。
 *   const ListItem_t * const ptr	常量指针指向常量，都不能改
 */
void vListInitialiseItem(ListItem_t *const pxItem);

/*
 * @brief 初始化链表
 * @param pxList 链表指针
 * @discription 链表初始化或者说根节点初始化，包括设置节点数为0，索引指针指向末尾节点，末尾节点指向自身，表示只有一个末尾节点（其在根节点中）
 */
void vListInitialise(List_t *const pxList);

/*
 * @brief 插入节点
 * @param pxList 链表指针
 * @param pxNewListItem 节点指针
 * @discription 不是插在末尾，插入新节点到 pxIndex 前面，常用于非排序插入；
 * @warning 链表必须先初始化，防止访问未初始化指针
 */
void vListInsertEnd(List_t *const pxList, ListItem_t *const pxNewListItem);

/*
 * @brief 插入节点
 * @param pxList 链表指针
 * @param pxNewListItem 节点指针
 * @discription 按照辅助值从小到大排序，末尾节点辅助值最大，如果有两个节点的值相同，则新节点在旧节点的后面插入，插入前List必须先初始化，防止访问为初始化指针
 * @warning 链表必须先初始化，防止访问未初始化指针
 */
void vListInsert(List_t *const pxList, ListItem_t *const pxNewListItem);

/*
 * @brief 删除节点
 * @param pxItemToRemove 节点指针
 * @return 删除的节点数量
 * @discription 删除节点，并返回删除后的节点数量，只能删除已有的节点
 * @warning 不能删除根节点，删除根节点，会使链表节点数相比实际节点数小一；同时会访问未定义指针pvContainer；
 */
UBaseType_t uxListRemove(ListItem_t *const pxItemToRemove);

#endif
