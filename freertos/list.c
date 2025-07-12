#include "list.h"

/*
 * @brief 初始化节点
 * @param pxItem 节点指针
 * @discription 节点初始化，初始化只需要让这个节点不属于任何链表，即将pvContainer设置为NULL，使用链表操作时必须调用它？
 */
void vListInitialiseItem(ListItem_t *const pxItem)
{
    pxItem->pvContainer = NULL;
};

/*
 * @brief 初始化链表
 * @param pxList 链表指针
 * @discription 链表初始化或者说根节点初始化，包括设置节点数为0，索引指针指向末尾节点，末尾节点指向自身，表示只有一个末尾节点（其在根节点中）
 */
void vListInitialise(List_t *const pxList)
{
    pxList->pxIndex = (ListItem_t *)&(pxList->xListEnd); // 赋值item指针索引前三个变量，后面的pvOwner，pvContainer未定义

    pxList->uxNumberOfItems = (UBaseType_t)0U; // 0U中U表示无符号数，而不是0一样的默认为int的数值

    pxList->xListEnd.xItemValue = portMAX_DELAY;                 // 将链表最后一个节点的辅助排序的值设置为最大，确保该节点就是链表的最后节点
    pxList->xListEnd.pxNext = (ListItem_t *)&(pxList->xListEnd); // 让末尾节点前后指向自身
    pxList->xListEnd.pxPrevious = (ListItem_t *)&(pxList->xListEnd);
    // 只是指针赋值，后面可以强制转化，变成对应类型的数据，如MiniListItem_t ListItem_t，
    // MiniListItem_t指针赋值ListItem_t指针时，ListItem_t给只有在访问共享前缀变量时才是安全的
}

/*
 * @brief 插入节点
 * @param pxList 链表指针
 * @param pxNewListItem 节点指针
 * @discription  不是插在末尾，插入新节点到 pxIndex 前面，常用于非排序插入；
 */
void vListInsertEnd(List_t *const pxList, ListItem_t *const pxNewListItem)
{
    ListItem_t *const pxIndex = pxList->pxIndex;

    pxNewListItem->pvContainer = (void *)pxList; // 根节点地址赋值新节点的容器指针，表示新节点是属于链表

    pxNewListItem->pxNext = pxIndex; // 节点插入
    pxNewListItem->pxPrevious = pxIndex->pxPrevious;
    pxIndex->pxPrevious->pxNext = pxNewListItem;
    pxIndex->pxPrevious = pxNewListItem;

    (pxList->uxNumberOfItems)++;
}

/*
 * @brief 插入节点
 * @param pxList 链表指针
 * @param pxNewListItem 节点指针
 * @discription 按照辅助值从小到大排序，末尾节点辅助值最大，如果有两个节点的值相同，则新节点在旧节点的后面插入
 */
void vListInsert(List_t *const pxList, ListItem_t *const pxNewListItem)
{
    ListItem_t *pxIterator;
    if (pxNewListItem->xItemValue == portMAX_DELAY)
    { // 如果辅助值极大，那插入根节点之前，可视作加在链表末尾。
        pxIterator = (ListItem_t *)pxList->xListEnd.pxPrevious;
    }
    else
    {
        for (pxIterator = (ListItem_t *)&pxList->xListEnd; pxIterator->pxNext->xItemValue <= pxNewListItem->xItemValue; pxIterator = pxIterator->pxNext)
        {
            /*
             * for(a;b;c){}: a->[b->{}->c]->[b->{}->c]->[b->{}->c]->......->b!->跳出
             * 迭代器指向从根节点开始，如果下一个节点的辅助值小于等于新节点的辅助值，则迭代器指向下一个节点。
             * 直到找到一个节点，其下一个节点的辅助值大于新节点的辅助值，跳出循环，迭代器指向该节点（其辅助值小于新节点的辅助值）
             */
        }
    }

    pxNewListItem->pxNext = pxIterator->pxNext;
    pxNewListItem->pxPrevious = pxIterator;
    pxIterator->pxNext->pxPrevious = pxNewListItem;
    pxIterator->pxNext = pxNewListItem;

    pxNewListItem->pvContainer = (void *)pxList;

    (pxList->uxNumberOfItems)++;
}

/*
 * @brief 删除节点
 * @param pxItemToRemove 节点指针
 * @return 删除的节点数量
 * @discription 删除节点，并返回删除的节点数量
 */
UBaseType_t uxListRemove(ListItem_t *const pxItemToRemove)
{
    List_t *const pxList = (List_t *)pxItemToRemove->pvContainer; // 获取节点所属的链表

    pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;     // 删除节点
    pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious; // 删除节点

    if (pxList->pxIndex == pxItemToRemove) // 如果删除的节点是索引节点，将索引节点指向前一个节点
    {
        pxList->pxIndex = pxItemToRemove->pxPrevious;
    }

    pxItemToRemove->pvContainer = NULL; // 删除节点

    (pxList->uxNumberOfItems)--; // 列表项数量减一

    return pxList->uxNumberOfItems; // 返回列表项数量
}
