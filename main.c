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
    vAppInitialise();
    vSemaphoreInitialise();
    vParameterInitialise(); 
    vInitialiseEventLists(NUMBEROFEVENTS);


    xTaskCreate( vR_Servant, "R-Servant", SERVANT_STACK_SIZE, (void *)&pvParameters[NUMBEROFSERVANT-1],tskIDLE_PRIORITY + 1, &xTaskOfHandle[NUMBEROFSERVANT-1]);

    portBASE_TYPE i,j;
    
    for( i = 0; i < NUMBEROFTASK; ++i )
    {
        xTaskCreate( vSensor, "Sensor", SERVANT_STACK_SIZE, (void *)&pvParameters[i*(xConcurrents + 1)],NUMBEROFTASK - i + 1, &xTaskOfHandle[i*(xConcurrents + 1)]);
        for( j = 1; j <= xConcurrents; ++j )
        {
            /* j is the number of concurrent servants in one task*/
            xTaskCreate( vServant, "servant", SERVANT_STACK_SIZE, (void *)&pvParameters[i*(xConcurrents + 1) + j],NUMBEROFTASK - i + 1, &xTaskOfHandle[i*(xConcurrents + 1) + j]);
        }
    }


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
    portBASE_TYPE i ;
    if( IS_INIT == 1 && xCurrentTime == 400 )
    {
        
        for( i = 0; i < NUMBEROFTASK; ++ i )
            xSemaphoreGive( xBinarySemaphore[i*xConcurrents] );

        IS_INIT = 0;
    }
    
    // send semaphore to R-Servant to triggered it to cope with events 
    // when time meeting the start time of task period
    if(Is_Executable_Event_Arrive())
    {
       xSemaphoreGive( xBinarySemaphore[NUMBEROFSERVANT-1] ); 
    }
}
