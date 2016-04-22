/*
* this application is used to try to exam the EventList mechanism.
*
* what this application can do is to print the "hello world! I am from China" in a periodic way.
* The task to print the words is finished by one task which is composed of five S-servant include S-1,
* S-2, S-3, S-4, S-5. They print the words in a specified order and in a collaborative way in a finite 
* time duration.
* */


#define USE_STDPERIPH_DRIVER
#include "stm32f10x.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "eventlist.h"
#include "servant.h"
#include "app.h"

static portBASE_TYPE IS_INIT = 1;
static void setup_hardware( void );

extern struct xParam pvParameters[NUMBEROFSERVANT];

extern xSemaphoreHandle xBinarySemaphore[NUMBEROFSERVANT];  // the network topology
extern xTaskHandle xTaskOfHandle[NUMBEROFSERVANT];         // record the handle of all S-Servant, the last one is for debugging R-Servant

extern portTickType xPeriodOfTask[NUMBEROFTASK];

extern xTaskComplete[NUMBEROFTASK];


/*
struct xParam
{
    portBASE_TYPE xMyFlag;     // the flag of current servant
    portBASE_TYPE xNumOfIn;    // the number of source servants
    portBASE_TYPE xNumOfOut;   // the number of destination servants
    portBASE_TYPE xInFlag[MAXINDEGREE];  // the flags of source servants
    portBASE_TYPE xOutFlag[MAXOUTDEGREE]; // the flags of destination servants
    portTickType xLet; // the xLet of current servant
    pvServantFunType xFp;  // the implementation of current Servant
}; */

#define SERVANT_STACK_SIZE 128 
int main(void)
{
    init_led();
    init_rs232();
    enable_rs232_interrupts();
    enable_rs232();

    //vTaskCompleteInitialise();
    vSemaphoreInitialise();
    vParameterInitialise();

    xTaskCreate( vR_Servant, "R-Servant", SERVANT_STACK_SIZE, (void *)&pvParameters[NUMBEROFSERVANT-1],tskIDLE_PRIORITY + 1, &xTaskOfHandle[NUMBEROFSERVANT-1]);

    // task 1, 100ms,
    xTaskCreate( vSensor, "Sensor 0", SERVANT_STACK_SIZE, (void *)&pvParameters[0], tskIDLE_PRIORITY + 15, &xTaskOfHandle[0]);
    xTaskCreate( vServant, "Servant 1", SERVANT_STACK_SIZE, (void *)&pvParameters[1], tskIDLE_PRIORITY + 15, &xTaskOfHandle[1]);

    // task 2 100ms,
    xTaskCreate( vSensor, "Sensor 2", SERVANT_STACK_SIZE, (void *)&pvParameters[2], tskIDLE_PRIORITY + 14, &xTaskOfHandle[2]);
    xTaskCreate( vServant, "Servant 3", SERVANT_STACK_SIZE, (void *)&pvParameters[3], tskIDLE_PRIORITY + 14,&xTaskOfHandle[3]);

    // task 6 100ms,
    xTaskCreate( vSensor, "Sensor 7", SERVANT_STACK_SIZE, (void *)&pvParameters[7], tskIDLE_PRIORITY + 13, &xTaskOfHandle[7]);
    xTaskCreate( vServant, "Servant 8", SERVANT_STACK_SIZE, (void *)&pvParameters[8], tskIDLE_PRIORITY + 13,&xTaskOfHandle[8]);

    // task 3, 200ms, 
    xTaskCreate( vSensor, "Sensor 4", SERVANT_STACK_SIZE, (void *)&pvParameters[4], tskIDLE_PRIORITY + 12, &xTaskOfHandle[4]); 

    // task 4, 200ms
    xTaskCreate( vSensor, "Sensor 5", SERVANT_STACK_SIZE, (void *)&pvParameters[5], tskIDLE_PRIORITY + 11, &xTaskOfHandle[5]); 

    // task 5, 200ms
    xTaskCreate( vSensor, "Sensor 6", SERVANT_STACK_SIZE, (void *)&pvParameters[6], tskIDLE_PRIORITY + 10, &xTaskOfHandle[6]); 

    // task 7, 200ms
    xTaskCreate( vSensor, "Sensor 9", SERVANT_STACK_SIZE, (void *)&pvParameters[9], tskIDLE_PRIORITY + 9, &xTaskOfHandle[9]); 
    xTaskCreate( vServant, "Servant 10", SERVANT_STACK_SIZE, (void *)&pvParameters[10], tskIDLE_PRIORITY + 9,&xTaskOfHandle[10]);
    xTaskCreate( vServant, "Servant 11", SERVANT_STACK_SIZE, (void *)&pvParameters[11], tskIDLE_PRIORITY + 9,&xTaskOfHandle[11]);

    // task 8 200ms
    xTaskCreate( vSensor, "Sensor 12", SERVANT_STACK_SIZE, (void *)&pvParameters[12], tskIDLE_PRIORITY + 8, &xTaskOfHandle[12]); 

    // task 13 400ms
    xTaskCreate( vSensor, "Sensor 20", SERVANT_STACK_SIZE, (void *)&pvParameters[20], tskIDLE_PRIORITY + 7, &xTaskOfHandle[20]); 

    // task 9 1000ms
    xTaskCreate( vSensor, "Sensor 13", SERVANT_STACK_SIZE, (void *)&pvParameters[13], tskIDLE_PRIORITY + 6, &xTaskOfHandle[13]); 
    xTaskCreate( vServant, "Servant 14", SERVANT_STACK_SIZE, (void *)&pvParameters[14], tskIDLE_PRIORITY + 6,&xTaskOfHandle[14]);

    // task 10 1000ms
    xTaskCreate( vSensor, "Sensor 15", SERVANT_STACK_SIZE, (void *)&pvParameters[15], tskIDLE_PRIORITY + 5, &xTaskOfHandle[15]); 
    xTaskCreate( vServant, "Servant 16", SERVANT_STACK_SIZE, (void *)&pvParameters[16], tskIDLE_PRIORITY + 5,&xTaskOfHandle[16]);
    xTaskCreate( vServant, "Servant 17", SERVANT_STACK_SIZE, (void *)&pvParameters[17], tskIDLE_PRIORITY + 5,&xTaskOfHandle[17]);

    // task 11 1000ms
    xTaskCreate( vSensor, "Sensor 18", SERVANT_STACK_SIZE, (void *)&pvParameters[18], tskIDLE_PRIORITY + 4, &xTaskOfHandle[18]); 

    // task 12 1000ms
    xTaskCreate( vSensor, "Sensor 19", SERVANT_STACK_SIZE, (void *)&pvParameters[19], tskIDLE_PRIORITY + 3, &xTaskOfHandle[19]); 


    /* Start running the task. */
    vTaskStartScheduler();

    return 0;
}

void myTraceCreate      (){
}

void myTraceSwitchedIn  (){
}

void myTraceSwitchedOut	(){
}

/*
inline float myTraceGetTick(){
	// 0xE000E014 -> Systick reload value
	// 0xE000E018 -> Systick current value
	return ((float)((*(unsigned long *)0xE000E014)-(*(unsigned long *)0xE000E018)))/(*(unsigned long *)0xE000E014);
}

inline unsigned long myTraceGetTimeMillisecond(){
	return (xTaskGetTickCountFromISR() + myTraceGetTick()) * 1000 / configTICK_RATE_HZ;
}
*/

/* time tick hook which is used to triggered every sensor of corresponding task to execute at
 * specified time according to their period.
 *
 * if there is any task need to be triggered at this time,
 * tick hook function would send semaphore to them.
 * */
void vApplicationTickHook( void )
{
    portTickType xCurrentTime = xTaskGetTickCount();
    if( IS_INIT == 1 && xCurrentTime == 100 )
    {
        xSemaphoreGive( xBinarySemaphore[0] );
        xSemaphoreGive( xBinarySemaphore[2] );
        xSemaphoreGive( xBinarySemaphore[7] );
        xSemaphoreGive( xBinarySemaphore[4] );
        xSemaphoreGive( xBinarySemaphore[5] );
        xSemaphoreGive( xBinarySemaphore[6] );
        xSemaphoreGive( xBinarySemaphore[9] );
        xSemaphoreGive( xBinarySemaphore[12] );
        xSemaphoreGive( xBinarySemaphore[13] );
        xSemaphoreGive( xBinarySemaphore[15] );
        xSemaphoreGive( xBinarySemaphore[18] );
        xSemaphoreGive( xBinarySemaphore[19] );
        xSemaphoreGive( xBinarySemaphore[20] );
        IS_INIT = 0;
    }
    
    // send semaphore to R-Servant to triggered it to cope with events 
    // when time meeting the start time of task period
    if( xCurrentTime >= xPeriodOfTask[0] * 2 )
    {
        if( xCurrentTime % xPeriodOfTask[0] == 0 || 
            xCurrentTime % xPeriodOfTask[2] == 0 ||
            xCurrentTime % xPeriodOfTask[12] == 0 ||
            xCurrentTime % xPeriodOfTask[8] == 0)
        {
           xSemaphoreGive( xBinarySemaphore[21] ); 
        }
    }
}
