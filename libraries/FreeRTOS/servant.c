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


#define USE_STDPERIPH_DRIVER
#include "stm32f10x.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "list.h"
#include "queue.h"
#include "semphr.h"
#include "eventlist.h"
#include "servant.h"
#include "app.h"
#define xFunctionTimes 100
#define SENSOR_PRINT
#define SERVANT_PRINT
//#define RSERVANT_PRINT
//#define SENSOR_LET
#define SERVANT_LET
//#define RSERVANT_LET
//#define SENSOR_FUN
//#define SERVANT_FUN

static portBASE_TYPE xFutureModelTime;
xList * pxCurrentReadyList;         // record the xEventReadyList that R-Servant transit event just now
struct xParam pvParameters[NUMBEROFSERVANT];

extern xSemaphoreHandle xBinarySemaphore[NUMBEROFSERVANT];  // the semaphores which are used to trigger new servant to execute
extern xTaskHandle xTaskOfHandle[NUMBEROFSERVANT+1];         // record the handle of all S-Servant, the last one is for debugging R-Servant 
// record the relationship among servants excluding R-Servant
//extern portBASE_TYPE xRelation[NUMBEROFSERVANT][NUMBEROFSERVANT] ;
// the new xRelation table which is implemeted with sparse matrix
extern struct xRelationship xRelations; 

// the LET of all S-Servant
extern portTickType xLetOfServant[NUMBEROFSERVANT] ;

// In app.c, this is used to sepcify the function of Servant
//extern pvServantFunType xServantTable[NUMBEROFSERVANT];

// record the task id where the servant is in
extern portBASE_TYPE xTaskOfServant[NUMBEROFSERVANT];

// record the period of Task
extern portTickType xPeriodOfTask[NUMBEROFTASK];

/* create all semaphores which are used to triggered s-servant */
void vSemaphoreInitialise()
{
    portBASE_TYPE i;

    for( i = 0; i < NUMBEROFSERVANT; ++ i )
    {
        vSemaphoreCreateBinary(xBinarySemaphore[i]);
        /* when created, it is initialised to 1. So, we take it away.*/
        xSemaphoreTake(xBinarySemaphore[i], portMAX_DELAY);
    }
}

/*
 * Set all the parameters of S-Servants.
 *
 * */
void vParameterInitialise()
{
    portBASE_TYPE i;
    portBASE_TYPE xSource, xDest;
       
    // initialise the member of pvParameters
    for( i = 0; i < NUMBEROFSERVANT; ++ i )
    {
        pvParameters[i].xMyFlag = i;
        pvParameters[i].xNumOfIn = 0;
        pvParameters[i].xNumOfOut = 0;
        pvParameters[i].xLet = xLetOfServant[i]/portTICK_RATE_MS;
        //pvParameters[i].xFp = xServantTable[i];
        pvParameters[i].xTaskFlag = xTaskOfServant[i]; 
        pvParameters[i].xPeriod = xPeriodOfTask[xTaskOfServant[i]]/portTICK_RATE_MS;
    }

    // new edition with sparse matrix relation table
    for( i = 0; i < xRelations.xNumOfRelation; ++ i )
    {
        xSource = xRelations.xRelation[i].xInFlag;
        xDest   = xRelations.xRelation[i].xOutFlag;

        pvParameters[xSource].xOutFlag[pvParameters[xSource].xNumOfOut] = xDest;
        pvParameters[xSource].xNumOfOut ++;

        pvParameters[xDest].xInFlag[pvParameters[xDest].xNumOfIn] = xSource;
        pvParameters[xDest].xNumOfIn ++;
    }
}

void vTaskDelayLET()
{
    xTaskHandle pxTaskCurrentTCBLocal = xTaskGetCurrentTaskHandle();
    portTickType xStartTime = xTaskGetxStartTime( pxTaskCurrentTCBLocal );
    portTickType xOutputTime = xTaskGetxLet( pxTaskCurrentTCBLocal ) + xStartTime;
    portTickType xCurrentTime = xTaskGetTickCount();

    while( xCurrentTime <  xOutputTime)
    {
        xCurrentTime = xTaskGetTickCount();
    }
}

// receive all Events from sources servant, and return them through the point
void vEventReceiveAll( void * pvParameter, xEventHandle *pxEvent )
{
    portBASE_TYPE i ;

    portBASE_TYPE NUM = ((struct xParam *) pvParameter)->xNumOfIn;
    portBASE_TYPE xInFlag[NUM]; 
    portBASE_TYPE xMyFlag = ((struct xParam *) pvParameter)->xMyFlag;
    portTickType xCurrentTime;
   
    xSemaphoreTake( xBinarySemaphore[xMyFlag],portMAX_DELAY );

    xCurrentTime = xTaskGetTickCount(); 
    vTaskSetxStartTime( xTaskOfHandle[xMyFlag], xCurrentTime );

    for( i = 0; i < NUM; i ++ )
    {
        // get all the in flag
        xInFlag[i] = ((struct xParam *) pvParameter)->xInFlag[i];
        // receive all events which are created by source servants and return 
        // them back to current Servant through the inout point
        vEventReceive( &pxEvent[i], xTaskOfHandle[xInFlag[i]], pxCurrentReadyList );
    }
}

void vEventDeleteAll( void * pvParameter, xEventHandle * pxEvent )
{
    portBASE_TYPE i ;
    portBASE_TYPE NUM = ((struct xParam *) pvParameter)->xNumOfIn;

    for( i = 0; i < NUM; ++i )
    {
        vEventDelete( pxEvent[i] );
    }
}


void vEventCreateAll( void * pvParameter, struct eventData *xDatas )
{
    portBASE_TYPE i ;
    portBASE_TYPE NUM = ((struct xParam *) pvParameter)->xNumOfOut;
    portBASE_TYPE xMyFlag = ((struct xParam *)pvParameter)->xMyFlag;
    portTickType xPeriod = ((struct xParam *) pvParameter)->xPeriod;
    portBASE_TYPE xFlags[NUM];

    for( i = 0; i < NUM; i ++ )
    {
        // get all flags of destination servants
        xFlags[i] = ((struct xParam *) pvParameter)->xOutFlag[i]; 

        // if current servant is the last one of task, then the event will be proceeded at the deadline
        if( xFlags[i] <= xMyFlag )
        {
            xDatas[i].IS_LAST_SERVANT = 1;
        }
        else
        {
            xDatas[i].IS_LAST_SERVANT = 0;
        }
        // create events which would be sent to destination servants.
        vEventCreate(xTaskOfHandle[xFlags[i]], xDatas[i]) ;
    }
}


/*
* Sensor servant, which is different from normal S-Servant and actuator, is implemeted 
* in a different way as a result. Sensor here is implemented as a periodic servant.
*
* @param pvParameter is parameter from programmer.
*
* */
void vSensor( void * pvParameter )
{
    portTickType xCurrentTime;

    portBASE_TYPE i;
    portBASE_TYPE IS_FIRST_TIME_TO_EXE = 1;

    /* store the paramter into stack of servant */
    void * pvMyParameter = pvParameter;

    portBASE_TYPE NUM = ((struct xParam *) pvMyParameter)->xNumOfOut;
    portBASE_TYPE xMyFlag = ((struct xParam *) pvMyParameter)->xMyFlag;
    portTickType xLet = ((struct xParam *) pvMyParameter)->xLet;
    portTickType xPeriod = ((struct xParam *) pvMyParameter)->xPeriod;
    pvServantFunType xMyFun = ((struct xParam *) pvMyParameter)->xFp;

    portBASE_TYPE xCount = 1;
    portTickType deadline = xCount * xPeriod;

    /* set the LET of Servant when it is created */
    vTaskSetxLet(xTaskOfHandle[xMyFlag], xLet);
    
    /* receive the events created by the last servant of this task. */
    xEventHandle pxEvent[NUM];
    
    /* create data for destination servants and initialise them */
    struct eventData xDatas[NUM];

    while(1)
    {

        if( IS_FIRST_TIME_TO_EXE == 1 )
        {
            /* Waiting for the start time of task period */
            xSemaphoreTake(xBinarySemaphore[xMyFlag], portMAX_DELAY);
#ifdef SENSOR_PRINT
            vPrintNumber( xMyFlag );
            vPrintNumber( deadline ); // ready time  of task 
#endif 
            xCurrentTime = xTaskGetTickCount();
#ifdef SENSOR_PRINT
            vPrintNumber( xCurrentTime ); // start time of task
#endif
            vTaskSetxStartTime( xTaskOfHandle[xMyFlag], xCurrentTime );

            IS_FIRST_TIME_TO_EXE = 0;
        }
        else
        {
            /* When system started, the first servant of every task is triggered to
             * execute by the init events created by tick hook. After that, the first
             * servant of every task is triggered by the last servant of corresponding task.
            * */
            vEventReceiveAll( pvMyParameter, pxEvent );

#ifdef SENSOR_PRINT
            vPrintNumber( xMyFlag );
            vPrintNumber( deadline ); // ready time of task 
#endif 
            xCurrentTime = xTaskGetTickCount();
#ifdef SENSOR_PRINT
            vPrintNumber( xCurrentTime ); // start time of task 
#endif
            vTaskSetxStartTime( xTaskOfHandle[xMyFlag], xCurrentTime );

            // do actuator and sensor
#ifdef SENSOR_FUN
            xMyFun( NULL, 0, xDatas, NUM);
#endif
            vEventDeleteAll(pvMyParameter, pxEvent);
        }

        // deadline is the start time of next period
        xCount ++;
        deadline = xCount * xPeriod; 

        for( i = 0; i < NUM; i ++ )
        {
            xDatas[i].xNextPeriod = deadline;
            xDatas[i].xTime = xCurrentTime + xLet;
            //xDatas[i].xTime = deadline -xPeriod + xLet;
        }
        // set the future model time
        xFutureModelTime = deadline - xPeriod + xLet;

        // create events for all destination servants of this servant. 
        vEventCreateAll( pvMyParameter, xDatas );

#ifdef SENSOR_LET
        vTaskDelayLET();
#endif
        
        if( xCurrentTime > xDatas[0].xNextPeriod )
        {
            vPrintNumber(xDatas[0].xNextPeriod);
            vPrintString("there are sensor missing deadline\n\r");
        }

#ifdef SENSOR_PRINT

        xCurrentTime = xTaskGetTickCount();
        vPrintNumber( xCurrentTime );  // finish time of sensor
        vPrintNumber( deadline );   // deadline of task
        vPrintNumber( ( xMyFlag + 10 ) * 3 );
#endif

        xCurrentTime = xTaskGetTickCount();
        if( xCurrentTime > 1000000 )
        {
            break;
        }
        // triggered R-Servant to execute 
        xSemaphoreGive( xBinarySemaphore[NUMBEROFSERVANT-1] );
    }
}

/*
*  Normal S-Servant is used to process the data of sensor and send them to actuator.
*
*  @param pvParameter is parameter from programmer
*
* */

void vServant( void * pvParameter )
{
    portBASE_TYPE i;
    void * pvMyParameter = pvParameter;
    portTickType xCurrentTime;
    
    portBASE_TYPE xNumOfIn = ((struct xParam *) pvMyParameter)->xNumOfIn;
    portBASE_TYPE xNumOfOut = ((struct xParam *) pvMyParameter)->xNumOfOut;
    portBASE_TYPE xMyFlag = ((struct xParam *) pvMyParameter)->xMyFlag;
    portTickType xPeriod = ((struct xParam *) pvMyParameter)->xPeriod;
    pvServantFunType xMyFun = ((struct xParam *) pvMyParameter)->xFp;

    xEventHandle pxEvent[xNumOfIn];
    struct eventData xDatas[xNumOfOut];

    /* get the LET of current servant */
    portTickType xLet = ((struct xParam *) pvMyParameter)->xLet;
    /* set the LET of current servant */
    vTaskSetxLet(xTaskOfHandle[xMyFlag], xLet);

    while(1)
    {
        vEventReceiveAll( pvMyParameter, pxEvent );

#ifdef SERVANT_PRINT
        vPrintNumber(xMyFlag);
#endif
        xCurrentTime = xTaskGetTickCountFromISR();
        vTaskSetxStartTime( xTaskOfHandle[xMyFlag], xCurrentTime );

        /* Here are coding for processing data of events */
        for( i = 0; i < xNumOfOut; i ++ )
        {
            xDatas[i] = xEventGetxData(pxEvent[i]);
            xDatas[i].xTime = xCurrentTime + xLet;
            //xDatas[i].xTime = xFutureModelTime + xLet;
            //xFutureModelTime = xDatas[i].xTime;
        }

#ifdef SERVANT_PRINT
        vPrintNumber( xDatas[0].xNextPeriod - xPeriod ); // ready time of task
        vPrintNumber( xCurrentTime );   // start time of current servant
#endif

#ifdef SERVANT_FUN
        for( i = 0; i < xFunctionTimes; ++ i )
        {
            xMyFun(pxEvent, xNumOfIn, xDatas, xNumOfOut);
        }
#endif

        vEventDeleteAll( pvMyParameter, pxEvent );        

        vEventCreateAll( pvMyParameter, xDatas );

        if( xCurrentTime > xDatas[0].xNextPeriod )
        {
            vPrintNumber(xDatas[0].xNextPeriod);
            vPrintString("there are servants missing deadline\n\r");
        }
#ifdef SERVANT_LET
        vTaskDelayLET();
#endif

#ifdef SERVANT_PRINT
        xCurrentTime = xTaskGetTickCount();
        vPrintNumber( xCurrentTime );   // finish time of current servant
        vPrintNumber( xDatas[0].xNextPeriod ); // deadline of task
        vPrintNumber( (xMyFlag + 10) * 3 );
#endif

        xCurrentTime = xTaskGetTickCount();
        if( xCurrentTime > 1000000 )
        {
            break;
        }

        // triggered R-Servant to execute 
        xSemaphoreGive( xBinarySemaphore[NUMBEROFSERVANT-1] );
    }
}

void vR_Servant( void * pvParameter)
{
    portBASE_TYPE i, j;
    portBASE_TYPE xSource, xDest;
    portBASE_TYPE HAVE_TO_SEND_SEMAPHORE; // could the semaphore be sent? 1 means yes , 0 means no
    portBASE_TYPE xResult;

    portTickType xCurrentTime;
    void * pvMyParameter = pvParameter;

    portBASE_TYPE xMyFlag = ((struct xParam *) pvMyParameter)->xMyFlag;
    portTickType xLet = ((struct xParam *) pvMyParameter)->xLet;
    vTaskSetxLet(xTaskOfHandle[xMyFlag], xLet);

    xListItem * pxEventListItem;
    xTaskHandle destinationTCB, sourceTCB;

    while(1)
    {
        // waiting for events created by tick hook or S-Servant
        xSemaphoreTake( xBinarySemaphore[xMyFlag], portMAX_DELAY );
#ifdef RSERVANT_PRINT
        vPrintNumber( xMyFlag );
#endif
        
        xCurrentTime = xTaskGetTickCount();
        vTaskSetxStartTime( xTaskOfHandle[xMyFlag], xCurrentTime );

#ifdef RSERVANT_PRINT
        vPrintNumber( xCurrentTime );
#endif

        // init to zero
        HAVE_TO_SEND_SEMAPHORE = 0;

        // to see whether there is a servant need to be triggered.
        // This process could be preempted by Sensor servant.
        while(! HAVE_TO_SEND_SEMAPHORE)
        {
            /*
             * transit the event item, whose timestamp bigger or equal to current Tick Count, 
             * from xEventList to the idlest xEvestReadyList
             *
             * */
            xResult = xEventListTransit( &pxEventListItem, &pxCurrentReadyList);
            if( xResult == 0 )
            {
                // not time yet
                break;
            }
            else
            {
                // transmit success
            }

            destinationTCB = xEventGetpxDestination( pxEventListItem->pvOwner);
            sourceTCB = xEventGetpxSource( pxEventListItem->pvOwner );
            HAVE_TO_SEND_SEMAPHORE = 1;  // set default 1

            for( i = 0; i < xRelations.xNumOfRelation; ++ i )
            {
                xSource = xRelations.xRelation[i].xInFlag;
                xDest   = xRelations.xRelation[i].xOutFlag;

                if( destinationTCB == xTaskOfHandle[xDest] )
                {
                    // find the right relation
                    if( sourceTCB == xTaskOfHandle[xSource] )
                    {
                        if( xRelations.xRelation[i].xFlag == 2 )
                        {
                            vPrintString("Error: This event has arrived!!\n\r") ;
                            vEventDelete( (xEventHandle) pxEventListItem->pvOwner );
                        }
                        else
                        {
                            // set the relation to 2
                            xRelations.xRelation[i].xFlag = 2;
                        }
                    }
                    // find other relation which is relative to destinationtcb
                    else
                    {
                        // waiting for an events that is not arriving yet
                        if( xRelations.xRelation[i].xFlag == 1 )
                        {
                            HAVE_TO_SEND_SEMAPHORE = 0;
                        }
                    }
                }
            }
        } //  end inner while(1)

        // not time yet, R-Servant should be sleep until next period of any task
        if ( xResult == 0 )
        {
#ifdef RSERVANT_LET
            vTaskDelayLET();
#endif
#ifdef RSERVANT_PRINT
            xCurrentTime = xTaskGetTickCount();
            vPrintNumber( xCurrentTime );
            vPrintNumber( (xMyFlag + 10) * 3 );
#endif
        }
        else  // triggered specified servant to execute
        {
            // set all the relations whose destination S-Servant is xTaskOfHandle[i] to 1.
            for( i = 0; i < xRelations.xNumOfRelation; ++ i )
            {
                xDest = xRelations.xRelation[i].xOutFlag;
                if( destinationTCB == xTaskOfHandle[xDest] )
                {
                    xRelations.xRelation[i].xFlag = 1;
                    // record the number of destinationtcb in xTaskOfHandle array.
                    j = xDest;
                }
            }
#ifdef RSERVANT_LET
            vTaskDelayLET();
#endif

#ifdef RSERVANT_PRINT
            xCurrentTime = xTaskGetTickCount();
            vPrintNumber( xCurrentTime );
            vPrintNumber( (xMyFlag + 10) * 3 );
#endif
            // send semaphore to destinationtcb
            xSemaphoreGive( xBinarySemaphore[j] );
        }
    }
}
