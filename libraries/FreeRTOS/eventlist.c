/*
    FreeRTOS V7.1.1 - Copyright (C) 2012 Real Time Engineers Ltd.
	

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS tutorial books are available in pdf and paperback.        *
     *    Complete, revised, and edited pdf reference manuals are also       *
     *    available.                                                         *
     *                                                                       *
     *    Purchasing FreeRTOS documentation will not only help you, by       *
     *    ensuring you get running as quickly as possible and with an        *
     *    in-depth knowledge of how to use FreeRTOS, it will also help       *
     *    the FreeRTOS project to continue with its mission of providing     *
     *    professional grade, cross platform, de facto standard solutions    *
     *    for microcontrollers - completely free of charge!                  *
     *                                                                       *
     *    >>> See http://www.FreeRTOS.org/Documentation for details. <<<     *
     *                                                                       *
     *    Thank you for using FreeRTOS, and thank you for your support!      *
     *                                                                       *
    ***************************************************************************


    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
    >>>NOTE<<< The modification to the GPL is included to allow you to
    distribute a combined work that includes FreeRTOS without being obliged to
    provide the source code for proprietary components outside of the FreeRTOS
    kernel.  FreeRTOS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public
    License and the FreeRTOS license exception along with FreeRTOS; if not it
    can be viewed here: http://www.freertos.org/a00114.html and also obtained
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!

    ***************************************************************************
     *                                                                       *
     *    Having a problem?  Start by reading the FAQ "My application does   *
     *    not run, what could be wrong?                                      *
     *                                                                       *
     *    http://www.FreeRTOS.org/FAQHelp.html                               *
     *                                                                       *
    ***************************************************************************


    http://www.FreeRTOS.org - Documentation, training, latest information,
    license and contact details.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool.

    Real Time Engineers ltd license FreeRTOS to High Integrity Systems, who sell
    the code with commercial support, indemnification, and middleware, under
    the OpenRTOS brand: http://www.OpenRTOS.com.  High Integrity Systems also
    provide a safety engineered and independently SIL3 certified version under
    the SafeRTOS brand: http://www.SafeRTOS.com.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "StackMacros.h"
#include "eventlist.h"

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

static  portBASE_TYPE IS_FIRST_EVENT = (portBASE_TYPE)1; /* A flag to know whether the event Lists are initialised. */

/* the struct of event item*/
typedef struct eveEventControlBlock
{
    xTaskHandle pxSource;         /*< the S-Servant where the event item from >*/
    xTaskHandle pxDestination;    /*< the S-Servant where the event item to >*/
    struct timeStamp xTimeStamp;    /*< the time stamp used to sort the event item in the event list >*/
    xListItem  xEventListItem;       /*< connect the eveECB to the xEventList by struct list>*/
    struct eventData xData;
}eveECB;


/* Event lists must be initialised before the first time to create an event. */
PRIVILEGED_DATA static xList xEventIdleList;  /*< Idel events are created when system initialising and stored in this list. Any one that needs idle event can get event from this event and return it back with FIFO policy>*/
PRIVILEGED_DATA static xList xEventPool;  /*< Event Pool which store the events from S-Servant in terms of FIFO>*/
PRIVILEGED_DATA static  xList xEventList;                            /*< Event List is used to store the event item in a specific order which sended or received by S-Servant.>*/
PRIVILEGED_DATA static xList xEventExecutableList;   /*< store the executable event which satisfies the time requirement >*/
PRIVILEGED_DATA static xList xEventReadyList[configCPU_NUMBER];                       /*< Event list is used to store the ready to be received events. the configCPU_NUMBER is defined in freeRTOSConfig.h>*/
static volatile unsigned portBASE_TYPE xEventSerialNumber  = (portBASE_TYPE)0;       /* used to set the level of timestamp in event */


/* initialise the event lists including xEventList and xEventReadyList[configCPU_NUMBER]
*
* no param and no return value
* */

/* insert new event item into xEventList. */
static void prvEventListGenericInsert1( xListItem * pxNewListItem) PRIVILEGED_FUNCTION;

/* insert executable event into xEventExecutableList */
static void prvEventListGenericInsert1( xListItem * pxNewListItem) PRIVILEGED_FUNCTION;


/* 
 * compare the two timestamp with a specified sort algorithm.
 *
 * @param1: timestamp one
 * @param2: timestamp two
 * */
// <unexecutable, TimeStamp, deadline, level>
static portBASE_TYPE xCompareFunction1( const struct timeStamp t1, const struct timeStamp t2 );
// <executable, deadline, timestamp, level>
static portBASE_TYPE xCompareFunction2( const struct timeStamp t1, const struct timeStamp t2 );


/*
 * initialise the eventItem and ready for the insertion. Set the pvOwner and Container which is the member of List Item.
 * **/
static void vListIntialiseEventItem( xEventHandle pvOwner, xListItem * pxNewEventItem);

static void vEventSetxData( xEventHandle pxEvent, struct eventData xData);

static void vEventSetxTimeStamp( xEventHandle pxNewEvent );

static xList * pxGetReadyList( void );

void vInitialiseEventLists( portBASE_TYPE NumOfEvents)
{
    volatile portBASE_TYPE xCPU, i;
    eveECB * pxIdleEvents[NumOfEvents];
    eveECB * pxEndFlagEvent;

    vListInitialise( ( xList * ) &xEventIdleList);
    vListInitialise( ( xList * ) &xEventPool );
    vListInitialise( ( xList * ) &xEventList );
    vListInitialise( ( xList * ) &xEventExecutableList );

    for( i = 0; i < NumOfEvents; ++i )
    {
        pxIdleEvents[i] = (eveECB *) pvPortMalloc(sizeof(eveECB)); 
        vListIntialiseEventItem( pxIdleEvents[i], (xListItem *) & pxIdleEvents[i]->xEventListItem );
        vListInsertEnd(&xEventIdleList, &pxIdleEvents[i]->xEventListItem); 
    }

    // init the xEventReadyList[configCPU_NUMBER].
    for ( xCPU = 0; xCPU < configCPU_NUMBER; xCPU ++ )
    {
        vListInitialise( (xList * ) & xEventReadyList[xCPU] );
    }

    // Creating an End FLag Event and insert into the end of xEventList, which needs sorted events
    pxEndFlagEvent = (eveECB *) pvPortMalloc( sizeof( eveECB ) );
    if( pxEndFlagEvent != NULL )
    {
       // pxEndFlagEvent->pxSource = pxEndFlagEvent->pxDestination = NULL;
        // there may be some problem here because of this assignment way
        pxEndFlagEvent->xTimeStamp.xDeadline= portMAX_DELAY;
        pxEndFlagEvent->xTimeStamp.xTime = portMAX_DELAY;
        pxEndFlagEvent->xTimeStamp.xMicroStep = portMAX_DELAY;
        pxEndFlagEvent->xTimeStamp.xLevel = portMAX_DELAY;
        vListIntialiseEventItem( pxEndFlagEvent, (xListItem *) & pxEndFlagEvent->xEventListItem );
        vListInsertEnd(&xEventList, &pxEndFlagEvent->xEventListItem); 
    }

    // Creating an End FLag Event and insert into the end of xEventExecutableList, which needs sorted events
    pxEndFlagEvent = (eveECB *) pvPortMalloc( sizeof( eveECB ) );
    if( pxEndFlagEvent != NULL )
    {
        // there may be some problem here because of this assignment way
        pxEndFlagEvent->xTimeStamp.xDeadline= portMAX_DELAY;
        pxEndFlagEvent->xTimeStamp.xTime = portMAX_DELAY;
        pxEndFlagEvent->xTimeStamp.xMicroStep = portMAX_DELAY;
        pxEndFlagEvent->xTimeStamp.xLevel = portMAX_DELAY;
        vListIntialiseEventItem( pxEndFlagEvent, (xListItem *) & pxEndFlagEvent->xEventListItem );
        vListInsertEnd(&xEventExecutableList, &pxEndFlagEvent->xEventListItem); 
    }
}

/* unexecutable event comparison function is used in xEventList. 
 * The event with earlist timestamp will be proceeded first*/
static portBASE_TYPE xCompareFunction1( const struct timeStamp t1, const struct timeStamp t2 )
{
    if( t1.xTime < t2.xTime)
    {
        return pdTRUE;
    }
    else if( t1.xTime == t2.xTime)
    {
        if( t1.xDeadline < t2.xDeadline )
        {
            return pdTRUE;
        }
        else if( t1.xDeadline == t2.xDeadline )
        {
            if( t1.xLevel < t2.xLevel )
            {
                return pdTRUE;
            }
        }
    }
    return pdFALSE;
}

/* executable event comparison function is used in xEventExecutableList. 
 * The event with earlist deadline will be scheduled to execute first */
static portBASE_TYPE xCompareFunction2( const struct timeStamp t1, const struct timeStamp t2 )
{
    if( t1.xDeadline < t2.xDeadline)
    {
        return pdTRUE;
    }
    else if( t1.xDeadline == t2.xDeadline)
    {
        if( t1.xTime < t2.xTime )
        {
            return pdTRUE;
        }
        else if( t1.xTime == t2.xTime )
        {
            if( t1.xLevel < t2.xLevel )
            {
                return pdTRUE;
            }
        }
    }

    return pdFALSE;
}



xTaskHandle xEventGetpxSource( xEventHandle pxEvent )
{
    return ((eveECB *)pxEvent)->pxSource;
}

xTaskHandle xEventGetpxDestination( xEventHandle pxEvent)
{
    return ((eveECB *) pxEvent)->pxDestination;
}

struct timeStamp xEventGetxTimeStamp( xEventHandle pxEvent)
{
    return ((eveECB *) pxEvent)->xTimeStamp;
}

struct eventData xEventGetxData( xEventHandle pxEvent)
{
    return ((eveECB *) pxEvent)->xData;
}

static void vEventSetxTimeStamp( xEventHandle pxNewEvent )
{
    eveECB * pxEvent =(eveECB *) pxNewEvent;
    portTickType xDeadline = pxEvent->xData.xNextPeriod;

    /* EDF scheduling algorithm */
    pxEvent->xTimeStamp.xDeadline= xDeadline ;

    /*set the time of this event to be processed */
    if( pxEvent->xData.IS_LAST_SERVANT == 1 )
    {
        pxEvent->xTimeStamp.xTime = xDeadline;
    }
    else
    {
        pxEvent->xTimeStamp.xTime = pxEvent->xData.xTime ;
    }

    /*the microstep is not used now*/
    pxEvent->xTimeStamp.xMicroStep = 0;

    /* set the level of timestamp according to the topology sort result*/
    pxEvent->xTimeStamp.xLevel = xEventSerialNumber;

    xEventSerialNumber++;
}

static void vEventSetxData( xEventHandle pxEvent, struct eventData xData)
{
    ((eveECB *) pxEvent)->xData = xData;
}

/*
* Get the event ready list according to the multicore scheduling algorithm.
*
* @return the address of the event ready list.
* */
static xList * pxGetReadyList( void )
{
    return &xEventReadyList[0];
}


/* insert event to xEventList in terms of comparison function 1 */
static void prvEventListGenericInsert1( xListItem *pxNewListItem )
{
    volatile xListItem *pxIterator;
    struct timeStamp xTimeStampOfInsertion;
    xList * pxList = &xEventList;

    xTimeStampOfInsertion = xEventGetxTimeStamp(pxNewListItem->pvOwner);

    if( xTimeStampOfInsertion.xTime == portMAX_DELAY )
    {
        pxIterator = pxList->xListEnd.pxPrevious;
    }
    else
    {
        taskENTER_CRITICAL();
        // There must already be a End Flag Event with max timeStamp in xEventList.
        // The End Flag Event can be the last Event to be processed, and new events
        // are inserted before it in xEventList.
        // The End Flag Event has been inserted when xEventList is initialised.
        for( pxIterator = ( xListItem * ) &( pxList->xListEnd ); xCompareFunction1( xEventGetxTimeStamp( pxIterator->pxNext->pvOwner ), xTimeStampOfInsertion ); pxIterator = pxIterator->pxNext ) 
        {
        }
        taskEXIT_CRITICAL();
    }

    // insert the new event before a bigger one.
    pxNewListItem->pxNext = pxIterator->pxNext;
    pxNewListItem->pxNext->pxPrevious = ( volatile xListItem * ) pxNewListItem;
    pxNewListItem->pxPrevious = pxIterator;
    pxIterator->pxNext = ( volatile xListItem * ) pxNewListItem;

    pxNewListItem->pvContainer = ( void * ) pxList;

    ( pxList->uxNumberOfItems )++;
}

/* insert event to xEventExecutableList in terms of comparison function 2 */
static void prvEventListGenericInsert2( xListItem *pxNewListItem )
{
    volatile xListItem *pxIterator;
    struct timeStamp xTimeStampOfInsertion;
    xList * pxList = &xEventExecutableList;

    xTimeStampOfInsertion = xEventGetxTimeStamp(pxNewListItem->pvOwner);

    if( xTimeStampOfInsertion.xTime == portMAX_DELAY )
    {
        pxIterator = pxList->xListEnd.pxPrevious;
    }
    else
    {
        taskENTER_CRITICAL();
        // There must already be a End Flag Event with max timeStamp in xEventList.
        // The End Flag Event can be the last Event to be processed, and new events
        // are inserted before it in xEventList.
        // The End Flag Event has been inserted when xEventList is initialised.
        for( pxIterator = ( xListItem * ) &( pxList->xListEnd ); xCompareFunction2( xEventGetxTimeStamp( pxIterator->pxNext->pvOwner ), xTimeStampOfInsertion ); pxIterator = pxIterator->pxNext ) 
        {
        }
        taskEXIT_CRITICAL();
    }

    // insert the new event before a bigger one.
    pxNewListItem->pxNext = pxIterator->pxNext;
    pxNewListItem->pxNext->pxPrevious = ( volatile xListItem * ) pxNewListItem;
    pxNewListItem->pxPrevious = pxIterator;
    pxIterator->pxNext = ( volatile xListItem * ) pxNewListItem;

    pxNewListItem->pvContainer = ( void * ) pxList;

    ( pxList->uxNumberOfItems )++;
}


static void vListIntialiseEventItem( xEventHandle pvOwner, xListItem * pxNewEventItem)
{
    /* set the pvOwner of the EventItem as a event*/
    listSET_LIST_ITEM_OWNER( pxNewEventItem, pvOwner );
    //pxNewEventItem->pvContainer = (void *) &xEventList;
}


void vEventGenericCreate( xTaskHandle pxDestination, struct eventData pdData)
{
    eveECB * pxNewEvent = NULL;

    /* using the pxCurrentTCB, current task should not be changed */
    taskENTER_CRITICAL();

    xTaskHandle pxCurrentTCBLocal = xTaskGetCurrentTaskHandle();

    // get new idle event 
    if( listCURRENT_LIST_LENGTH(&xEventIdleList) == 0 )
    {
        vPrintString(" No Idle Events available\n\r");
        return;
    }

    pxNewEvent = (eveECB *)xEventIdleList.xListEnd.pxNext->pvOwner;
    vListRemove( (xListItem *)&pxNewEvent->xEventListItem );
    if( pxNewEvent == NULL )
    {
        vPrintString("malloc for event stack failed\n\r");
    }
    if ( pxNewEvent != NULL )
    {
        pxNewEvent->pxSource = pxCurrentTCBLocal;
        pxNewEvent->pxDestination = pxDestination;

        /* initialise the time stamp of an event item according to the sort algorithm.*/
        vEventSetxData( pxNewEvent, pdData );

        vEventSetxTimeStamp( pxNewEvent );

        vListIntialiseEventItem( pxNewEvent, (xListItem *) &pxNewEvent->xEventListItem );

        // insert the event into eventpool with O(1)
        vListInsertEnd(&xEventPool, (xListItem *)& pxNewEvent->xEventListItem);
    }
    taskEXIT_CRITICAL();
}

portBASE_TYPE Is_Executable_Event_Arrive()
{
    xListItem * temp_pxEventListItem;
    portTickType xCurrentTime;
    struct timeStamp xTimeStamp;
    
    if(listCURRENT_LIST_LENGTH(&xEventList) > 1)
    {
        temp_pxEventListItem = (xListItem *)xEventList.xListEnd.pxNext;
        xTimeStamp = xEventGetxTimeStamp( (xEventHandle) (temp_pxEventListItem)->pvOwner );
        xCurrentTime = xTaskGetTickCount();

        // the event is executable
        if( xTimeStamp.xTime <= xCurrentTime )
        {
            return 1;
        }
    }
    return 0;
}

/* An API to transfer all executable Event Items from xEventList to xEventExecutableList.
* Then, choose the first executable event item in xEventExecutableList to proceed, which means
* transfer the executable to specific xEventReadyList according to the condition of CPU*/
portBASE_TYPE xEventListGenericTransit( xListItem ** pxEventListItem, xList ** pxCurrentReadyList)
{
    xListItem * temp_pxEventListItem;
    struct timeStamp xTimeStamp;
    portTickType xCurrentTime;

    // transmit events from event pool to xEventList according to the xCompareFunction1
    while(listCURRENT_LIST_LENGTH( &xEventPool ) != 0)
    {
        temp_pxEventListItem = (xListItem *) xEventPool.xListEnd.pxNext;    
        vListRemove(temp_pxEventListItem);
        //how to call this funciton: vEventListInsert( newListItem ). This function add the new item into the xEventList as default
        prvEventListGenericInsert1( temp_pxEventListItem ); 
    }

    // transmit the executable event from xEventList to xEventExecutableList 
    while( listCURRENT_LIST_LENGTH( &xEventList ) > 1 )
    {
        temp_pxEventListItem = (xListItem *)xEventList.xListEnd.pxNext;
        xTimeStamp = xEventGetxTimeStamp( (xEventHandle) (temp_pxEventListItem)->pvOwner );
        xCurrentTime = xTaskGetTickCount();

        // the event is executable
        if( xTimeStamp.xTime <= xCurrentTime )
        {
            taskENTER_CRITICAL();
            /* remove pxListItem from xEventList */ 
            vListRemove(temp_pxEventListItem);
            /* insert the executable event into the xEventExecutableList*/
            prvEventListGenericInsert2(temp_pxEventListItem);
            taskEXIT_CRITICAL();
        }
        else
        {
          // no executable event in xEventList
           break; 
        }
    }

    // if no executable event exists, then return NULL and information about not time yet
    if( listCURRENT_LIST_LENGTH(& xEventExecutableList) == 1 )
    {
        *pxCurrentReadyList = NULL;
        *pxEventListItem = NULL;
        return 0;
    }
    // transmit the first executable event from xEventExecutableList to specific xEventReadyList
    else
    {
        *pxCurrentReadyList = pxGetReadyList();
        *pxEventListItem = (xListItem *) xEventExecutableList.xListEnd.pxNext;
        
        taskENTER_CRITICAL();
        /* remove pxListItem from xEventList */ 
        vListRemove(*pxEventListItem);
        /* insert the pxListItem into the specified pxList */
        vListInsertEnd(*pxCurrentReadyList, *pxEventListItem);
        taskEXIT_CRITICAL();
    }

    return 1;
}

void vEventGenericReceive( xEventHandle * pxEvent, xTaskHandle pxSource, xList * pxList )
{
    xList * const pxConstList = pxList; 
    if (pxList == &xEventReadyList[0])
    {
        //successful.
        //helloworld();
    }

    if( listLIST_IS_EMPTY( pxList ) )
    {
        *pxEvent = NULL;
        return;
    }

    volatile xListItem * pxFlag = pxConstList->xListEnd.pxNext;


    taskENTER_CRITICAL();

    xTaskHandle pxCurrentTCBLocal = xTaskGetCurrentTaskHandle();

    /* look up in the specified xEventReadyList. */
    for(; pxFlag != (xListItem *) ( pxConstList->xListEnd.pxPrevious); pxFlag = pxFlag->pxNext)
    {
        if( xEventGetpxSource((xEventHandle) pxFlag->pvOwner) == pxSource && xEventGetpxDestination((xEventHandle) pxFlag->pvOwner) == pxCurrentTCBLocal )
        {
            *pxEvent = pxFlag->pvOwner;
            vListRemove((xListItem *)pxFlag);
            taskEXIT_CRITICAL();
            return;
        }
    }

    /* we can't forget the last one item of the list. */
    if( xEventGetpxSource((xEventHandle) pxFlag->pvOwner) == pxSource && xEventGetpxDestination((xEventHandle) pxFlag->pvOwner) == pxCurrentTCBLocal )
    {
        *pxEvent = pxFlag->pvOwner;
        vListRemove((xListItem *)pxFlag);
        taskEXIT_CRITICAL();
        return;
    }
    else
    {
        // cannot find the specified event.
        *pxEvent = NULL;
    }

    taskEXIT_CRITICAL();
}

void vEventGenericDelete( xEventHandle xEvent)
{
    taskENTER_CRITICAL();

    xListItem * pxEventItem = &((eveECB *)xEvent)->xEventListItem;
    vListRemove (pxEventItem);
    vListInsertEnd( &xEventIdleList, pxEventItem );

    taskEXIT_CRITICAL();
}
