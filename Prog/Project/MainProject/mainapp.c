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

struct SCore *core;
static bool failure = false;
static bool wake_up = false;

static uint32 *stack_limit;

static uint32 sys_st = 0;
static uint32 ui_st = SYSSTAT_UI_ON;

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
    uint32 pwr_mode = HWSLEEP_STOP;

    CheckStack();
    sys_st |= DispHAL_App_Poll();
    sys_st |= core_get_pwrstate();
    if ( BeepIsRunning() )
        sys_st |= SYSSTAT_UI_ON;

    // decide power management model
    if ( pwr_check(SYSSTAT_CORE_BULK) )
        // full run mode - no sleeping
        pwr_mode = HWSLEEP_FULL;
    else 
    {   
        if ( pwr_check( SYSSTAT_DISP_BUSY | SYSSTAT_CORE_RUN_FULL | SYSSTAT_UI_ON ) )
            // when display is processed, ui is working or core runs with short timing needs ( 10ms and 250us )
            pwr_mode = HWSLEEP_WAIT;
        else if ( sys_st == ( SYSSTAT_CORE_STOPPED | SYSSTAT_UI_PWROFF ) )
            // power off condition
            pwr_mode = HWSLEEP_OFF;
        // otherwise remains with stopped state - wake up at every 1 second or at user action
        else
        {
            if ( pwr_check( SYSSTAT_UI_STOP_W_ALLKEY ) )
                pwr_mode = HWSLEEP_STOP_W_ALLKEYS;
            else if ( pwr_check( SYSSTAT_UI_STOP_W_SKEY ) )
                pwr_mode = HWSLEEP_STOP_W_SKEY;
        }
    }

    wake_up = HW_Sleep( pwr_mode );
    sys_st = 0;
}


// Main application routine
static inline void ProcessApplication( struct SEventStruct *evmask )
{
    core_poll( evmask );

    if ( evmask->timer_tick_10ms || evmask->key_event )
    {
        ui_st = ui_poll( evmask );
    }
    sys_st |= ui_st;
}

// Main application entry
void main_entry( uint32 *stack_top )
{
    stack_limit = stack_top;
    InitHW();               // init hardware
    if ( core_init( &core ) )
    {
        failure = true;
        return;
    }

    ui_init( core );
    DispHAL_Display_On( );
    DispHAL_SetContrast( 0x30 );
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
