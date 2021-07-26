/*
 *  Do not modify this file; it is automatically 
 *  generated and any modifications will be overwritten.
 *
 * @(#) xdc-A71
 */

#include <xdc/std.h>

#include <ti/sysbios/knl/Clock.h>
extern const ti_sysbios_knl_Clock_Handle timeSharingClk;

#include <ti/sysbios/knl/Task.h>
extern const ti_sysbios_knl_Task_Handle ledSrvTask;

#include <ti/sysbios/knl/Task.h>
extern const ti_sysbios_knl_Task_Handle vesselTask1;

#include <ti/sysbios/knl/Task.h>
extern const ti_sysbios_knl_Task_Handle vesselTask2;

#include <ti/sysbios/knl/Task.h>
extern const ti_sysbios_knl_Task_Handle vesselTask3;

#include <ti/sysbios/knl/Task.h>
extern const ti_sysbios_knl_Task_Handle vesselTask4;

#include <ti/sysbios/knl/Queue.h>
extern const ti_sysbios_knl_Queue_Handle ledSpecQ;

#include <ti/sysbios/knl/Event.h>
extern const ti_sysbios_knl_Event_Handle vesselReqEvt;

#include <ti/sysbios/knl/Clock.h>
extern const ti_sysbios_knl_Clock_Handle barrierOppClk;

#include <ti/sysbios/knl/Clock.h>
extern const ti_sysbios_knl_Clock_Handle barrierFifoClk;

#include <ti/sysbios/knl/Semaphore.h>
extern const ti_sysbios_knl_Semaphore_Handle bOppQueueSem;

#include <ti/sysbios/knl/Semaphore.h>
extern const ti_sysbios_knl_Semaphore_Handle bFifoQueueSem;

#include <ti/sysbios/gates/GateMutexPri.h>
extern const ti_sysbios_gates_GateMutexPri_Handle gateMutexPri0;

#include <ti/sysbios/knl/Swi.h>
extern const ti_sysbios_knl_Swi_Handle stopMaintSwi;

#define TI_DRIVERS_WIFI_INCLUDED 0

extern int xdc_runtime_Startup__EXECFXN__C;

extern int xdc_runtime_Startup__RESETFXN__C;

#ifndef ti_sysbios_knl_Task__include
#ifndef __nested__
#define __nested__
#include <ti/sysbios/knl/Task.h>
#undef __nested__
#else
#include <ti/sysbios/knl/Task.h>
#endif
#endif

extern ti_sysbios_knl_Task_Struct TSK_idle;

