/**
 * @file list.h
 * @brief FreeRTOS List Structures (stub for compilation)
 */

#ifndef LIST_H
#define LIST_H

#include "FreeRTOS.h"

struct xLIST_ITEM {
    TickType_t xItemValue;
    struct xLIST_ITEM *pxNext;
    struct xLIST_ITEM *pxPrevious;
    void *pvOwner;
    void *pvContainer;
};

typedef struct xLIST_ITEM ListItem_t;

struct xMINI_LIST_ITEM {
    TickType_t xItemValue;
    struct xLIST_ITEM *pxNext;
    struct xLIST_ITEM *pxPrevious;
};

typedef struct xMINI_LIST_ITEM MiniListItem_t;

typedef struct xLIST {
    UBaseType_t uxNumberOfItems;
    ListItem_t *pxIndex;
    MiniListItem_t xListEnd;
} List_t;

#define listSET_LIST_ITEM_OWNER(pxListItem, pxOwner)    ((pxListItem)->pvOwner = (void*)(pxOwner))
#define listGET_LIST_ITEM_OWNER(pxListItem)             ((pxListItem)->pvOwner)
#define listSET_LIST_ITEM_VALUE(pxListItem, xValue)     ((pxListItem)->xItemValue = (xValue))
#define listGET_LIST_ITEM_VALUE(pxListItem)             ((pxListItem)->xItemValue)
#define listGET_HEAD_ENTRY(pxList)                      ((pxList)->pxIndex->pxNext)

#define listGET_ITEM_VALUE_OF_HEAD_ENTRY(pxList)        (listGET_LIST_ITEM_VALUE(listGET_HEAD_ENTRY(pxList)))
#define listIS_CONTAINER_EMPTY(pxList)                  ((BaseType_t)((pxList)->uxNumberOfItems == (UBaseType_t)0))

void vListInitialise(List_t *pxList);
void vListInitialiseItem(ListItem_t *pxItem);
void vListInsert(List_t *pxList, ListItem_t *pxNewListItem);
void vListInsertEnd(List_t *pxList, ListItem_t *pxNewListItem);
UBaseType_t uxListRemove(ListItem_t *pxItemToRemove);

#endif /* LIST_H */
