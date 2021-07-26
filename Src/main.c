
//----------------------------------------
// BIOS header files
//----------------------------------------
#include <xdc/std.h>  						//mandatory - have to include first, for BIOS types
#include <ti/sysbios/BIOS.h> 				//mandatory - if you call APIs like BIOS_start()
#include <xdc/runtime/Log.h>				//needed for any Log_info() call
#include <xdc/cfg/global.h> 				//header file for statically defined objects/handles

#include <ti/sysbios/knl/Queue.h>
#include <xdc/runtime/System.h>
//-----------------------------------------
// MSP430 Header Files
//-----------------------------------------
#include <driverlib.h>

//-----------------------------------------
// MSP430 MCLK frequency settings
// Used to set MCLK frequency
// Ratio = MCLK/FLLREF = 8192KHz/32KHz
//-----------------------------------------
#define MCLK_DESIRED_FREQUENCY_IN_KHZ  8000                      			// 8MHz
#define MCLK_FLLREF_RATIO              MCLK_DESIRED_FREQUENCY_IN_KHZ / ( UCS_REFOCLK_FREQUENCY / 1024 )    // Ratio = 250

#define GPIO_ALL	GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3| \
					GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7

#define VSG_NUM 3 															// Number of Vessel Priorities (Effectively number of Vessel Tasks!)
#define MAX_NAME_LEN 15 													// Maximum Length of Vessel Name String
#define RED GPIO_PORT_P1, GPIO_PIN0 										// Red LED
#define GREEN GPIO_PORT_P4, GPIO_PIN7 										// Green LED
#define redLedToggle(times) ledToggle(RED, (times))							// redLedToggle MACRO function
#define greenLedToggle(times) ledToggle(GREEN, (times))						// greenLedToggle MACRO function
#define gradLedToggleHelper(led) ({GPIO_toggleOutputOnPin(led); delay();})  // Helper MACRO function for gradation function
#define NUM_OF_MAP_POINTS 8													// Number of map points
#define INIT_VESSELS_BLINKS 2												// Starting NumOfBlinks value
#define MAINT_SLEEP_TIME 6000												// Maintenance sleep time- 3s

//-----------------------------------------
// Prototypes
//-----------------------------------------

// Message Structure for the Queue communication between each vessel Task and ledSrvTask.
typedef struct InitQMsg{
	Queue_Elem	elem;
	UInt	eventBitmask;
	UInt	ledToBlink;
	UInt    numOfBlinks;
} InitQMsg;

// Data structure for the vessels, this data is passed to the map points.
typedef struct Data{
	Int vesselIndx;
	InitQMsg msg;
} Data;

// Enum structure for distinguishing between the different led toggling functions.
typedef enum{
	both_e,
	red_e,
	green_e,
	grad_e
} LED_TO_BLINK;

typedef void (*MapPointsPtr) (Int*, Data*);

// The hardware_init function
void hardware_init(void);

// Handler function for "Time Sharing" Clock - should ellapse every 1/2 millisecond
void tsClockHandler(void);

// Handler function for all Vessel Tasks
void sailAway(UArg arg0, UArg arg1);

// Handler function for maintShip (Maintenance Ship)
void maintainCanal(UArg arg0, UArg arg1);

// Led Toggling Functions
void ledToggle(uint8_t selectedPort, uint16_t selectedPins, int times);
void bothLedToggle(int times);
void gradationLedToggle(int times);

// Handler function for ledSrvTask
void ledSrvTaskHandler(void);

// The Non-Blocking delay function (see Project Description File)
void delay(void);

// Handler function for Opposite priority barrier Clock
void bOppClockHandler(void);

// Handler function for FIFO barrier Clock
void bFifoClockHandler(void);

// Helper function for initializing the message of the vessels
void initMsg(Data *data);

// Opposite priority barrier function
void oppPriBarrier(Int *curMapPoint, Data *data);

// Canal crossing barrier function
void canalCrossing(Int *curMapPoint, Data *data);

// FIFO barrier function
void fifoBarrier(Int *curMapPoint, Data *data);

// Unload point function
void unloadPoint(Int *curMapPoint, Data *data);

// Helper function for printing log messages
void printMessages(char* msg, volatile const char* msgArg1, Int msgArg2, Int msgArg3);

// Handler function for stopMaintSwi
void stopMaintSwiHandler(void);

// Helper function for requesting LED services from ledSrvTaskHandler
void reqLEDService(Int *curMapPoint, Data *data);

//-----------------------------------------
// Globals
//-----------------------------------------

volatile const UInt vesselPriorities[VSG_NUM] = {5, 3, 2};
volatile const char vesselNames[VSG_NUM][MAX_NAME_LEN] = {"OilTanker", "CargoVessel", "MailVessel"};
volatile Task_Handle vesselHandles[VSG_NUM];
volatile const LED_TO_BLINK ledToBlinkArr[VSG_NUM + 1] = {both_e, red_e, green_e, grad_e};
volatile const MapPointsPtr mapPointsArr[NUM_OF_MAP_POINTS] = {oppPriBarrier, canalCrossing, fifoBarrier, reqLEDService, oppPriBarrier, canalCrossing, fifoBarrier, reqLEDService};
volatile const Int mapPointsBlinks[NUM_OF_MAP_POINTS] = {0, 4, 0, 6, 0, 5, 0, 7};
volatile Bool stopMaintShip;
volatile UInt oppPriBriCounter;
volatile UInt fifoBriCounter;

//---------------------------------------------------------------------------
// main()
//---------------------------------------------------------------------------
void main(void)
{
	hardware_init();

	stopMaintShip = FALSE;
	oppPriBriCounter = 0;
	fifoBriCounter = 0;

	Swi_Params swiParms;
	Swi_Params_init(&swiParms);
	swiParms.trigger = VSG_NUM;
	Swi_setAttrs(stopMaintSwi, NULL, &swiParms);

	BIOS_start();
}

void hardware_init(void)
{
	// Disable the Watchdog Timer (important, as this is enabled by default)
	WDT_A_hold( WDT_A_BASE );

	// Set MCLK frequency to 8192 KHz
	// First, set DCO FLLREF to 32KHz = REF0
	UCS_initClockSignal(UCS_FLLREF, UCS_REFOCLK_SELECT, UCS_CLOCK_DIVIDER_1);
	// Second, Set Ratio and Desired MCLK Frequency = 8192KHz and initialize DCO
	UCS_initFLLSettle(MCLK_DESIRED_FREQUENCY_IN_KHZ, MCLK_FLLREF_RATIO);

	// Set GPIO ports to low-level outputs
	GPIO_setAsOutputPin( GPIO_PORT_P1, GPIO_ALL );
	GPIO_setAsOutputPin( GPIO_PORT_P2, GPIO_ALL );
	GPIO_setAsOutputPin( GPIO_PORT_P3, GPIO_ALL );
	GPIO_setAsOutputPin( GPIO_PORT_P4, GPIO_ALL );
	GPIO_setAsOutputPin( GPIO_PORT_P5, GPIO_ALL );
	GPIO_setAsOutputPin( GPIO_PORT_P6, GPIO_ALL );
	GPIO_setAsOutputPin( GPIO_PORT_PJ, GPIO_ALL );

	GPIO_setOutputLowOnPin( GPIO_PORT_P1, GPIO_ALL );
	GPIO_setOutputLowOnPin( GPIO_PORT_P2, GPIO_ALL );
	GPIO_setOutputLowOnPin( GPIO_PORT_P3, GPIO_ALL );
	GPIO_setOutputLowOnPin( GPIO_PORT_P4, GPIO_ALL );
	GPIO_setOutputLowOnPin( GPIO_PORT_P5, GPIO_ALL );
	GPIO_setOutputLowOnPin( GPIO_PORT_P6, GPIO_ALL );
	GPIO_setOutputLowOnPin( GPIO_PORT_PJ, GPIO_ALL );
}

/*---------------------------------------------------------------------------
Function name: sailAway
Description: The vessels function
Input: UArg arg0, UArg arg1
Output: None
Algorithm: Prolog- 	   1. Calculate "vesselIndex" generically to insert vessel's handler
		   	   	   	  	  to "vesselHandles" array.
		   	   	   	   2. Call "initMsg" function, each vessel initialize their
		   	   	   	   	  "InitQMsg" message there.
		   Life Cycle- Iterates through the map points.
		   Epilog- 	   1. Remove msg from queue.
		   	   	   	   2. Decreases "stopMaintSwi" trigger and finish.
---------------------------------------------------------------------------*/
void sailAway(UArg arg0, UArg arg1)
{
	//Prolog
	Data data;
	Int curMapPoint;
	data.vesselIndx = (Int)arg0 % (VSG_NUM + 1) - 1;
	vesselHandles[data.vesselIndx] = Task_self();
	initMsg(&data);

	//Life Cycle
	for(curMapPoint = 0 ; curMapPoint < NUM_OF_MAP_POINTS ; curMapPoint++)
	{
		printMessages("Vessel %s entered map point %u; Priority = %u", vesselNames[data.vesselIndx], curMapPoint + 1, Task_getPri(vesselHandles[data.vesselIndx]));
		mapPointsArr[curMapPoint](&curMapPoint, &data);
	}

	//Epilog
	Queue_remove(&data.msg.elem);
	Swi_dec(stopMaintSwi);
	Log_info1("Vessel %s Task Terminated", (IArg)vesselNames[data.vesselIndx]);
}

/*---------------------------------------------------------------------------
Function name: initMsg
Description: Initializes vesssel's message
Input: Data *data
Output: None
Algorithm: 1. Configure "eventBitmask" according to "vesselIndx".
		   2. Configure "ledToBlink" according to "ledToBlinkArr".
		   3. Configure "numOfBlinks" to 2.
		   4. Insert the message to "ledSpecQ" queue.
---------------------------------------------------------------------------*/
void initMsg(Data *data)
{
	data->msg.eventBitmask = 1 << data->vesselIndx;
	data->msg.ledToBlink = ledToBlinkArr[data->vesselIndx];
	data->msg.numOfBlinks = INIT_VESSELS_BLINKS;
	Queue_enqueue(ledSpecQ, &data->msg.elem);
}

/*---------------------------------------------------------------------------
Function name: maintainCanal
Description: The maintenance function
Input: UArg arg0, UArg arg1
Output: None
Algorithm: Prolog- 	   1. Calculate "vesselIndx" generically for the message.
		   	   	   	   2. Call "initMsg" function to initialize "InitQMsg"
		   	   	   	      message there.
		   Life Cycle- 1. Sleep for 3s.
		   	   	   	   2. Check if need to stop, if yes- breaks.
		   	   	   	   3. Post "vesselReqEvt" with the "eventBitmask".
		   Epilog- 	   1. Stop timer.
		   	   	   	   2. Remove msg from queue.
		   	   	   	   3. Take care the last vessel is finished.
		   	   	   	   4. Post to "vesselReqEvt" in order to let him finish also.
---------------------------------------------------------------------------*/
void maintainCanal(UArg arg0, UArg arg1)
{
	//Prolog
	Data data;
	data.vesselIndx = VSG_NUM;
	initMsg(&data);

	//Life Cycle
	while(TRUE)
	{
		Task_sleep(MAINT_SLEEP_TIME);
		if(stopMaintShip)
			break;
		Event_post(vesselReqEvt, data.msg.eventBitmask);
	}

	//Epilog
	Clock_stop(timeSharingClk);
	Queue_remove(&data.msg.elem);
	Task_setPri(Task_self(), 1);
	Task_yield();
	Event_post(vesselReqEvt, data.msg.eventBitmask);
	Log_info0("maintainCanal::Task Terminated");
}

/*---------------------------------------------------------------------------
Function name: ledSrvTaskHandler
Description: This is ledSrvTask function- responsible for providing LED service.
Input: None
Output: None
Algorithm: Prolog- 	   Calculate "orBitMask" generically for pending on it.
		   Life Cycle- 1. Pend on "vesselReqEvt" until someone asks for LED
		   	   	   	   	  service via posting "vesselReqEvt".
		   	   	   	   2. Check if the "ledSpecQ" is empty.
		   	   	   	   3. If yes- break and finish.
		   	   	   	   4. If not- iterates on "ledSpecQ" queue and search for
		   	   	   	   	  the message of the vessel who requested the service.
		   	   	   	   5. Check for abnormal behavior- if the message was not
		   	   	   	   	  found in the queue, there is a problem.
		   	   	   	   6. Activate the right LED function according to "ledToBlink"
		   	   	   	   	  message field.
---------------------------------------------------------------------------*/
void ledSrvTaskHandler(void)
{
	//Prolog
	UInt orBitMask = (1 << (VSG_NUM + 1)) - 1;
	Int consumedBitMask;
	Queue_Elem *elem;
	InitQMsg *msg = NULL;

	//Life Cycle
	while(TRUE)
	{
		consumedBitMask = Event_pend(vesselReqEvt, Event_Id_NONE, orBitMask, BIOS_WAIT_FOREVER);
		if(Queue_empty(ledSpecQ))
			break;
		for (elem = Queue_head(ledSpecQ); elem != (Queue_Elem *) ledSpecQ; elem = Queue_next(elem))
		{
			if(((InitQMsg *)elem)->eventBitmask == consumedBitMask)
			{
				msg = (InitQMsg *)elem;
				break;
			}
		}
		if(msg == NULL)
			continue;
		switch(ledToBlinkArr[msg->ledToBlink])
		{
			case both_e:
				bothLedToggle(msg->numOfBlinks);
				break;
			case red_e:
				redLedToggle(msg->numOfBlinks);
				break;
			case green_e:
				greenLedToggle(msg->numOfBlinks);
				break;
			case grad_e:
				gradationLedToggle(msg->numOfBlinks);
				break;
		}
		msg = NULL;
	}
}

/*---------------------------------------------------------------------------
Function name: ledToggle
Description: Toggle the LED
Input: uint8_t selectedPort, uint16_t selectedPins, int times
Output: None
Algorithm: Iterates 2*times and toggle the LED with delay between each toggle.
---------------------------------------------------------------------------*/
void ledToggle(uint8_t selectedPort, uint16_t selectedPins, int times)
{
	times <<= 1;
	while(times > 0)
	{
		GPIO_toggleOutputOnPin(selectedPort, selectedPins);
		delay();
		times--;
	}
}

/*---------------------------------------------------------------------------
Function name: bothLedToggle
Description: Toggle the LED
Input: Int times
Output: None
Algorithm: Iterates 2*times and toggle the red and green LEDs with delay
 	 	   between each toggle.
---------------------------------------------------------------------------*/
void bothLedToggle(Int times)
{
	times <<= 1;
	while(times > 0)
	{
		GPIO_toggleOutputOnPin(RED);
		GPIO_toggleOutputOnPin(GREEN);
		delay();
		times--;
	}
}

/*---------------------------------------------------------------------------
Function name: gradationLedToggle
Description: Toggle the LED
Input: Int times
Output: None
Algorithm: Iterates 2*times and toggle the LEDs following the pattern:
		   red, delay, green, delay.
---------------------------------------------------------------------------*/
void gradationLedToggle(Int times)
{
	times <<= 1;
	while(times > 0)
	{
		gradLedToggleHelper(RED);
		gradLedToggleHelper(GREEN);
		times--;
	}
}

/*---------------------------------------------------------------------------
Function name: delay
Description: Creates a non-blocking delay.
Input: None
Output: None
Algorithm: Use "__delay_cycles" to create a delay of 1/2sec.
---------------------------------------------------------------------------*/
void delay(void)
{
	__delay_cycles(1024000);
}

/*---------------------------------------------------------------------------
Function name: oppPriBarrier
Description: The Opposite priority barrier map point.
Input: Int *curMapPoint, Data *data
Output: None
Algorithm: 1. Set the priority of the vessel to the opposite priority according
			  to "vesselPriorities" array.
		   2. Increase "oppPriBriCounter" by 1.
		   3. If the last vessel arrived- start the "barrierOppClk" Clock.
		   4. Pend on "bOppQueueSem" Semaphore.
---------------------------------------------------------------------------*/
void oppPriBarrier(Int *curMapPoint, Data *data)
{
	Task_setPri(vesselHandles[data->vesselIndx], vesselPriorities[VSG_NUM - 1 - data->vesselIndx]);
	oppPriBriCounter++;
	if(oppPriBriCounter == VSG_NUM)
		Clock_start(barrierOppClk);
	Semaphore_pend(bOppQueueSem, BIOS_WAIT_FOREVER);
}

/*---------------------------------------------------------------------------
Function name: canalCrossing
Description: The Canal crossing map point.
Input: Int *curMapPoint, Data *data
Output: None
Algorithm: 1. Achive the mutexPri "gateMutexPri0" key.
		   2. Request LED service from "ledSrvTask" and delay.
		   3. Print Log about the vessels condition and exit "gateMutexPri0".
---------------------------------------------------------------------------*/
void canalCrossing(Int *curMapPoint, Data *data)
{
	IArg gateKey;
	gateKey = GateMutexPri_enter(gateMutexPri0);
	reqLEDService(curMapPoint, data);
	delay();
	Log_info2("Vessel %s Canal Entry; Priority = %u", (IArg)vesselNames[data->vesselIndx], Task_getPri(vesselHandles[data->vesselIndx]));
	GateMutexPri_leave(gateMutexPri0, gateKey);
	Log_info2("Vessel %s Canal Exit; Priority = %u", (IArg)vesselNames[data->vesselIndx], Task_getPri(vesselHandles[data->vesselIndx]));
}

/*---------------------------------------------------------------------------
Function name: fifoBarrier
Description: The FIFO barrier map point.
Input: Int *curMapPoint, Data *data
Output: None
Algorithm: 1. Increase "fifoBriCounter" by 1.
		   2. If the last vessel arrived- start the "barrierFifoClk" Clock.
		   3. Pend on "bFifoQueueSem" Semaphore.
---------------------------------------------------------------------------*/
void fifoBarrier(Int *curMapPoint, Data *data)
{
	fifoBriCounter++;
	if(fifoBriCounter == VSG_NUM)
	{
		//Task_setPri(Task_self(), 4);
		Clock_start(barrierFifoClk);
	}
	Semaphore_pend(bFifoQueueSem, BIOS_WAIT_FOREVER);
}

/*---------------------------------------------------------------------------
Function name: reqLEDService
Description: Unload and End point function + helper for posting to "vesselReqEvt".
Input: Int *curMapPoint, Data *data
Output: None
Algorithm: 1. Update "numOfBlinks" field.
		   2. Post to "vesselReqEvt".
---------------------------------------------------------------------------*/
void reqLEDService(Int *curMapPoint, Data *data)
{
	data->msg.numOfBlinks = mapPointsBlinks[*curMapPoint];
	Event_post(vesselReqEvt, data->msg.eventBitmask);
}

/*---------------------------------------------------------------------------
Function name: tsClockHandler
Description: "Time sharing" clock function- every 50ms.
Input: None
Output: None
Algorithm: Yields the current task.
---------------------------------------------------------------------------*/
void tsClockHandler(void)
{
	Task_yield();
}

/*---------------------------------------------------------------------------
Function name: bOppClockHandler
Description: The Opposite priority barrier clock task.
Input: None
Output: None
Algorithm: 1. Post "bOppQueueSem" semaphore.
		   2. Set the priority of the task to it's real priority.
		   3. If all vessels were released from barrier- stop the clock.
---------------------------------------------------------------------------*/
void bOppClockHandler(void)
{
	/*if(Semaphore_getCount(bOppQueueSem)!=oppPriBriCounter)
	 	 return;*/
	Semaphore_post(bOppQueueSem);
	Task_setPri(vesselHandles[oppPriBriCounter - 1], vesselPriorities[oppPriBriCounter - 1]);
	if(--oppPriBriCounter == 0)
		Clock_stop(barrierOppClk);
}

/*---------------------------------------------------------------------------
Function name: bFifoClockHandler
Description: The FIFO barrier clock task.
Input: None
Output: None
Algorithm: 1. Iterates on "vesselHandles" array and set the vessels priority to 1.
 	 	   2. Post "bFifoQueueSem" semaphore fifoBriCounter times.
		   3. Reset "fifoBriCounter".
		   3. Stop the clock.
---------------------------------------------------------------------------*/
void bFifoClockHandler(void)
{
	Int i;
	for(i = 0 ; i < fifoBriCounter ; i++)
		Task_setPri(vesselHandles[i], 1);
	for(i = 0 ; i < fifoBriCounter ; i++)
		Semaphore_post(bFifoQueueSem);
	fifoBriCounter = 0;
	Clock_stop(barrierFifoClk);
}

/*---------------------------------------------------------------------------
Function name: stopMaintSwiHandler
Description: The Swi function.
Input: None
Output: None
Algorithm: Change "stopMaintShip" variable to TRUE.
---------------------------------------------------------------------------*/
void stopMaintSwiHandler(void)
{
	stopMaintShip = TRUE;
}

/*---------------------------------------------------------------------------
Function name: printMessages
Description: Messages center
Input: char* msg, volatile const char* msgArg1, Int msgArg2, Int msgArg3
Output: None
Algorithm: Print log message using Log_Info API
---------------------------------------------------------------------------*/
void printMessages(char* msg, volatile const char* msgArg1, Int msgArg2, Int msgArg3)
{
	Log_info3(msg, (IArg)msgArg1, msgArg2, msgArg3);
}
