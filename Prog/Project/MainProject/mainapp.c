/*********
 *      Main application loop
 *
 *
 *  Power management cases:
 *
 *
 *
 **/

#include <string.h>
#include "hw_stuff.h"
#include "events_ui.h"
#include "ui.h"
#include "core.h"
//dev #include "dispHAL.h"

#include "graphic_lib.h"

#include "sensors.h"

//dev extern struct SCore core;
static bool failure = false;
static bool wake_up = false;

static uint32 *stack_limit;

static uint32 sys_st = 0;
static uint32 ui_st = SYSSTAT_UI_WAKEUP;

static inline void CheckStack(void)
{
#ifndef ON_QT_PLATFORM
    if ( *stack_limit != STACK_CHECK_WORD )
    {
         while (1)
         {
             HW_LED_On();
             asm("NOP");
             asm("NOP");
         }
    }
#endif
}


#define pwr_check( a )  ( sys_st & (a) )

static inline void System_Poll( void )
{
    enum EPowerMode pwr_mode = pm_hold;
// dev VVVVV    
    volatile uint32 pwrmode;
    pwrmode = Sensor_GetPwrStatus();
      
// dev ^^^^^      
      
    CheckStack();
/*dev
    sys_st |= DispHAL_App_Poll();
    sys_st |= core_pwr_getstate();
    if ( BeepIsRunning() )
        sys_st |= SYSSTAT_UI_ON;

    // decide power management model
    if ( pwr_check(SYSSTAT_CORE_BULK) )
        // full run mode - no sleeping
        pwr_mode = pm_full;
    else 
    {   
        if ( pwr_check( SYSSTAT_DISP_BUSY | SYSSTAT_CORE_RUN_FULL | SYSSTAT_UI_ON | SYSSTAT_UI_WAKEUP ) )
            // when display is processed, ui is working or core runs with short timing needs ( 10ms )
            pwr_mode = pm_sleep;
        else if ( pwr_check ( SYSYTAT_CORE_MONITOR ) )
        {
            // core in monitoring/fast registering mode - need to maintain memory, or to listen to sensor IRQ, so no full power down possible
            if ( sys_st == ( SYSYTAT_CORE_MONITOR | SYSSTAT_UI_PWROFF ) )
                pwr_mode = pm_hold;         // ui is off - only long press on power/mode button will wake it up - must behave like in full power down
            else
                pwr_mode = pm_hold_btn;     // ui is stopped only - buttons can wake it up at any time
            core_pwr_setup_alarm(pwr_mode);
            EventBtnClear();
        }
        else if ( sys_st == ( SYSSTAT_CORE_STOPPED | SYSSTAT_UI_PWROFF ) )
        {
            // core is stopped or long term registering, ui is off - we can cut the power
            pwr_mode = pm_down;
            core_pwr_setup_alarm(pwr_mode);
            EventBtnClear();
        }
        else
        {
            // for the remained cases we will use hold with all buttons, this includes the UI stopped case when core doesn't do anything
            pwr_mode = pm_hold_btn;     // ui is stopped only - buttons can wake it up at any time
            core_pwr_setup_alarm(pwr_mode);
            EventBtnClear();
        }
    }

    wake_up = HW_Sleep( pwr_mode );
    sys_st = 0;
*/
}



// Main application routine
static inline void ProcessApplication( struct SEventStruct *evmask )
{
/*dev
    core_poll( evmask );

    if ( evmask->timer_tick_10ms || evmask->key_event )
    {
        ui_st = ui_poll( evmask );
    }
    sys_st |= ui_st;
*/

/////// dev VVVVVVV    
    static int temp_cnt = 1000;
    static int rh_cnt = 1000;
    static int press_cnt = 1000;

    Sensor_Poll( evmask->timer_tick_system );

    // PRESS
    if ( Sensor_Is_Ready() & SENSOR_PRESS )
    {
        static volatile uint32 press;
        HW_LED_Off();
        press = Sensor_Get_Value( SENSOR_PRESS );
        press = (((press * 100) / 4) * 3) / 4;      // Pascals
    }
    else if ( press_cnt && evmask->timer_tick_system )
    {
        press_cnt--;
        if ( press_cnt == 0 )
        {
            HW_LED_On();
            Sensor_Acquire( SENSOR_PRESS );
            press_cnt = 1000;
        }
    }
        
    // RH
    if ( Sensor_Is_Ready() & SENSOR_RH )
    {
        static volatile int rh;
        HW_LED_Off();
        rh = Sensor_Get_Value( SENSOR_RH );
        rh = (int)((uint32)(125*100*rh) >> 14) - 600;
    }
    else if ( rh_cnt && evmask->timer_tick_system )
    {
        rh_cnt--;
        if ( rh_cnt == 0 )
        {
            HW_LED_On();
            Sensor_Acquire( SENSOR_RH );
            rh_cnt = 1000;
        }
    }

    // TEMP
    if ( Sensor_Is_Ready() & SENSOR_TEMP )
    {
        static volatile int temp;
        HW_LED_Off();
        temp = Sensor_Get_Value( SENSOR_TEMP );
        temp = ((uint32)(17572*temp) >> 14) - 4685;
    }
    else if ( temp_cnt && evmask->timer_tick_system )
    {
        temp_cnt--;
        if ( temp_cnt == 0 )
        {
            HW_LED_On();
            Sensor_Acquire( SENSOR_TEMP );
            temp_cnt = 1000;
        }
    }
}

// Main application entry
void main_entry( uint32 *stack_top )
{
    stack_limit = stack_top;
    InitHW();               // init hardware
/*dev    if ( core_init( NULL ) )
    {
        failure = true;
        return;
    }
    ui_init( NULL );
*/
    
    
/////// dev VVVVVVV    
    
    Sensor_Init();
    
    Sensor_Acquire( SENSOR_PRESS );
    Sensor_Acquire( SENSOR_TEMP );
    Sensor_Acquire( SENSOR_RH );
    
}


// Main application loop
void main_loop(void)
{
    struct SEventStruct event;

#ifndef ON_QT_PLATFORM
    while ( failure )
#else
    if ( failure )
#endif
    {
        event = Event_Poll( );
        Event_Clear( event );
    }


#ifndef ON_QT_PLATFORM
    while (1)
#else
    else
#endif
    {
        event = Event_Poll( );
        if ( wake_up )
        {
            event.key_event = 1;    // forces display update
        }

        ProcessApplication( &event );

        Event_Clear( event );
        
        System_Poll( );
    }
}


#ifdef ON_QT_PLATFORM
void Set_WakeUp(void)
{
    wake_up = true;
}
#endif


