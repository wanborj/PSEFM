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
#include "app.h"

/* the include file of PapaBench */
#include "ppm.h"
#include "servo.h"
#include "spi_fbw.h"
//#include "inflight_calib.h"
#include "infrared.h"
#include "estimator.h"
#include "pid.h"
#include "link_fbw.h"
#include "gps.h"
#include "autopilot.h"
#include "nav.h"


xSemaphoreHandle xBinarySemaphore[NUMBEROFSERVANT];  // the semaphores which are used to trigger new servant to execute
xTaskHandle xTaskOfHandle[NUMBEROFSERVANT];         // record the handle of all S-Servant, the last one is for debugging R-Servant 
//portBASE_TYPE xTaskComplete[NUMBEROFTASK];  // record whether specified task completes execution
portTickType xPeriodOfTask[NUMBEROFTASK] =
{
    100 ,
    100 ,
    200 ,
    200 ,
    200 ,
    100 ,
    200 ,
    200 ,
    1000,
    1000,
    1000,
    1000,
    400
};

// the LET of all S-Servant (ms)
portTickType xLetOfServant[NUMBEROFSERVANT] = 
{ 
    13,    // sensor_0
    2,    // s_1
    4,    // s_2
    3,    // s_3 
    3,    // actuator_4 
    6,    // Sensor_5
    7,    // s_6
    7,    // s_7
    5,    // s_8
    3,    // s_9
    4,    // s_10
    6,    // Actuator_11
    2,    // Sensor_12
    2,    // s_13
    4,    // Actuator_14
    3,    // Sensor_15
    5,    // s_16
    4,    // s_17
    3,    // s_18
    6,    // s_19
    6,    // actuator_20
    1    // R-Servant 
};

portBASE_TYPE xTaskOfServant[NUMBEROFSERVANT] =
{
    0, 
    0,
    1,
    1,
    2,
    3,
    4,
    5,
    5,
    6,
    6,
    6, 
    7,
    8,
    8, 
    9,
    9,
    9,
    10,
    11,
    12, 
    -1
};

// record the relationship among servants excluding R-Servant
/*
struct sparseRelation
{
    portBASE_TYPE xInFlag;  // source servant
    portBASE_TYPE xOutFlag; // destination servant
    portBASE_TYPE xFlag;  // 1 means relation exist but without event, 2 means that there is a event
};
struct xRelationship
{
    portBASE_TYPE xNumOfRelation;   // the real number of relations
    struct sparseRelation xRelation[MAXRELATION];  // the number of effective relations among servant
};
*/
struct xRelationship xRelations = 
{
    21,
    {
        {0, 1, 1}, // task 1
        {1, 0, 1},
        {2, 3, 1},
        {3, 2, 1},
        {4, 4, 1}, // task 2
        {5, 5, 1},
        {6, 6, 1},
        {7, 8, 1},
        {8, 7, 1},
        {9,10, 1},
        {10,11,1},
        {11,9, 1}, 
        {12,12,1}, // task 3
        {13,14,1},
        {14,13,1},
        {15,16,1}, // task 4
        {16,17,1},
        {17,15,1},
        {18,18,1},
        {19,19,1},
        {20,20,1}
    }
};

/* explemented in main.c */
extern void to_autopilot_from_last_radio();
extern void check_mega128_values_task();
extern void check_failsafe_task();
//extern void inflight_calib(portBASE_TYPE mode_changed); // main_auto.c , we transfer this function into main.c because of bugs if not 
extern void radio_control_task(); 
extern void send_gps_pos();
extern void send_radIR();
extern void send_takeOff();

extern void  send_boot();
extern void  send_attitude();
extern void  send_adc();
extern void  send_settings();
extern void  send_desired();
extern void  send_bat();
extern void  send_climb();
extern void  send_mode();
extern void  send_debug();
extern void  send_nav_ref();

#define SUNNYBEIKE 1
#ifdef SUNNYBEIKE
/*
 * Task 1, including:
 * 1) sensor 0
 * 2) s_1
 * 3) s_2
 * 4) s_3
 * 5) actuator 4
 * */
void s_0(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    //vPrintString("Task 1 start ###############\n\r");
    last_radio_from_ppm(); // ppm.h
}
void s_1(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    //vPrintString("s_0\n\r");
    //servo_set();  // servo.h, this function is used by multitask which in terms of multiuse of Servant. And this
                    // is not implemented now. So cancel.
}
void s_2(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{

    //vPrintString("s_1\n\r");
    to_autopilot_from_last_radio(); // main.c
}

void s_3(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{

    spi_reset(); // spi_fbw.h
}
void s_4(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    //vPrintString("Task 1 end ###############\n\r");
    check_mega128_values_task(); // main.c
}

void s_5(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    //vPrintString("Task 2 start+++++++++++++++++++\n\r");
    servo_transmit(); //servo.h
}

void s_6(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    //vPrintString("s_2\n\r");
    check_failsafe_task(); // main.c
}
void s_7(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{

    //vPrintString("s_7\n\r");
     //inflight_calib(pdTRUE); // inflight_calib.h
     //inflight_calib(pdTRUE); // main_auto.c , we transfer this function into main.c because of bugs if not 
     radio_control_task();
}
void s_8(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    //check_mega128_values_task(); // link_autopilot.h
    ir_gain_calib(); //infrared.h
}
void s_9(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData)
{
    
    ir_update(); // infrared.h
}
void s_10(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{

    //vPrintString("s_3\n\r");
    //servo_transmit();  // servo.h
    estimator_update_state_infrared(); //estimator.h
}
void s_11(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    //vPrintString("Task 2 end +++++++++++++++++++\n\r");
    roll_pitch_pid_run(); // pid.h
}

void s_12(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    //vPrintString("Task 3 start---------------------\n\r");
    link_fbw_send(); //link_fbw.h
}
void s_13(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    
    //vPrintString("s_12\n\r");
    parse_gps_msg(); //gps.h
}

void s_14(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    //vPrintString("Task 3 end ----------------------\n\r");
    //use_gps_pos(); // autopilot.h  is not implemeted.
    send_gps_pos();
    send_radIR();
    send_takeOff();
}


void s_15(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    //vPrintString("Task 4 start********************\n\r");
    nav_home(); // nav.h
}
void s_16(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    
    //vPrintString("s_8\n\r");
    nav_update(); // nav.h
}

void s_17(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    course_pid_run(); // pid.h
}
void s_18(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    altitude_pid_run(); // pid.h
}
void s_19(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData)
{
    climb_pid_run(); // pid.h
}
void s_20(xEventHandle * pxEventArray, portBASE_TYPE NumOfEvent, struct eventData * pxDataArray, portBASE_TYPE NumOfData) 
{
    // main.c
    send_boot();
    send_attitude();
    send_adc();
    send_settings();
    send_desired();
    send_bat();
    send_climb();
    send_mode();
    send_debug();
    send_nav_ref();
}
#endif

// assigned the point of function into specified position of xServantTable.
pvServantFunType xServantTable[NUMBEROFSERVANT] = 
{
    &s_0, 
    &s_1,
    &s_2,
    &s_3,
    &s_4,
    &s_5,
    &s_6,
    &s_7,
    &s_8,
    &s_9,
    &s_10, 
    &s_11,
    &s_12,
    &s_13,
    &s_14,
    &s_15,
    &s_16,
    &s_17,
    &s_18,
    &s_19,
    &s_20,
    NULL 
};
