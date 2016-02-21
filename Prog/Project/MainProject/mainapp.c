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
#include "dispHAL.h"

#include "graphic_lib.h"
#include "ui_graphics.h"
#include "sensors.h"

extern struct SCore core;
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

    CheckStack();
    sys_st |= DispHAL_App_Poll();
/*dev
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

//vvvvvvv dev vvvvvvvvvv
    static int to_ctr = 1;

    Sensor_Poll( evmask->timer_tick_system );

    if ( evmask->timer_tick_10ms )
    {
        if ( to_ctr )
            to_ctr--;
        else
        {
            to_ctr = 100;
            Sensor_Acquire( SENSOR_PRESS | SENSOR_TEMP | SENSOR_RH ); 
        }
    }

    if ( Sensor_Is_Ready() == (SENSOR_PRESS | SENSOR_TEMP | SENSOR_RH) )
    {
        if ( Sensor_Is_Ready() & SENSOR_PRESS )
        {
            uint32 press;
            press = Sensor_Get_Value(SENSOR_PRESS);             // in 100x Pa
            press = ((uint64)press * 7500615) / 10000000;       // gives value in 100x Hgmm
            Graphic_SetColor(0);
            Graphic_FillRectangle( 50, 46, 110, 56, 0 ); 
            uigrf_putfixpoint( 50, 46, uitxt_smallbold, press, 5, 2, ' ', false );
        }
        if ( Sensor_Is_Ready() & SENSOR_TEMP )
        {
            int temp;
            temp = Sensor_Get_Value(SENSOR_TEMP);       // 16fp9+40*C
            temp = ((temp*100) >> TEMP_FP) - 4000;        // gives value in 100x Hgmm
            Graphic_SetColor(0);
            Graphic_FillRectangle( 50, 16, 110, 26, 0 ); 
            uigrf_putfixpoint( 50, 16, uitxt_smallbold, temp, 5, 2, ' ', true );
        }
        if ( Sensor_Is_Ready() & SENSOR_RH )
        {
            uint32 rh;
            rh = Sensor_Get_Value(SENSOR_RH);       // in 100x %
            rh = ( rh * 100 ) >> RH_FP;
            Graphic_SetColor(0);
            Graphic_FillRectangle( 50, 31, 110, 41, 0 ); 
            uigrf_putfixpoint( 50, 31, uitxt_smallbold, rh, 5, 2, ' ', false );
        }
        
        DispHAL_UpdateScreen();
    }

//^^^^^^^^^^^^^^^^^^^^^^
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


//vvvvvvv  dev vvvvvv
    Sensor_Init();

    Graphics_Init( NULL, NULL );


    Graphic_SetColor(1);
    Graphic_Rectangle(0,0,127,63);

    uigrf_text( 10, 16, uitxt_smallbold, "Temp:" );
    uigrf_text( 26, 31, uitxt_smallbold, "RH:" );
    uigrf_text( 11, 46, uitxt_smallbold, "Press:" );


    DispHAL_Display_On( );
    DispHAL_SetContrast( 0x30 );
//^^^^^^^^^^^^^^^^^^^
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


