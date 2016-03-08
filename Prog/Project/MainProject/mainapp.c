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

extern struct SCore core;
static bool failure = false;
static uint32 wake_up = WUR_NONE;

static uint32 *stack_limit;

static uint32 sys_pwr = 0;
static uint32 ui_st = SYSSTAT_UI_ON_WAKEUP;

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
    enum EPowerMode pwr_mode = pm_down;

    CheckStack();

//dev
    sys_pwr |= DispHAL_App_Poll();


/*dev

    sys_pwr |= core_pwr_getstate();
    sys_pwr |= DispHAL_App_Poll();
    if ( BeepIsRunning() )
        sys_pwr |= PM_SLEEP;

    // decide power management model
    if ( sys_pwr & PM_FULL )
        pwr_mode = pm_full;             // full run mode - no sleeping
    else if ( sys_pwr & PM_SLEEP )
        pwr_mode = pm_sleep;            // sleep and wake up on timer/dma irq
    else
    {
        if ( sys_pwr & PM_HOLD_BTN )
            pwr_mode = pm_hold_btn;     // device with active UI, but with low cpu usage - any button should wake it up
        else if ( sys_pwr & PM_HOLD )
            pwr_mode = pm_hold;         // device is working on background only (monitoring/registering tasks), no active UI, should be woken up by the user by power button only
        else
            pwr_mode = pm_down;         // no power requirement is needed - turn it off

        core_pwr_setup_alarm(pwr_mode); // set up the next alarm point in the RTC
        EventBtnClear();                // clear button status
    }

    wake_up = HW_Sleep( pwr_mode );     // enter in the selected power mode
    sys_pwr = 0;                        // when exit - recreate the power scenario
*/
}

uint32 tctr = 0;

static void local_put_btn_list( uint32 btn_mask, int32 y_poz )
{
    uint32 i = 0;
    for (i=0; i<7; i++)
    {
        if ( btn_mask & (1<<i) )
        {
            Graphic_PutPixel( 10 + i*10 , 30 + y_poz, 1 );
            tctr = 8;  // display clear at 80ms
        }
        else if ( y_poz < 0 )
        {
            Graphic_PutPixel( 10 + i*10 , 30 + y_poz, 0 );
        }
    }
}

extern uint32 internal_get_keys();
uint32  new_btn = 0;
uint32  old_btn = 0;

// Main application routine
static inline void ProcessApplication( struct SEventStruct *evmask )
{
/*dev
    core_poll( evmask );

    if ( evmask->timer_tick_10ms || evmask->key_event )
    {
        ui_st = ui_poll( evmask );
    }
    sys_pwr |= ui_st;
*/

//dev
    if ( evmask->key_event )
    {
        local_put_btn_list( evmask->key_pressed, 0 );
        local_put_btn_list( evmask->key_longpressed, 6);
        local_put_btn_list( evmask->key_released, 12 );

        DispHAL_UpdateScreen();
    }

    new_btn = internal_get_keys();
    if ( new_btn != old_btn )
    {
        local_put_btn_list(new_btn, -14);
        old_btn = new_btn;
        DispHAL_UpdateScreen();
    }
        
    if ( evmask->timer_tick_10ms )
    {
        if ( tctr )
        {
            tctr--;
            if ( tctr == 0 )
            {
                uint32 i;
                for (i=0; i<7; i++)
                {
                    Graphic_PutPixel( 10 + i*10 , 30 + 0 * 6, 0 );
                    Graphic_PutPixel( 10 + i*10 , 30 + 1 * 6, 0 );
                    Graphic_PutPixel( 10 + i*10 , 30 + 2 * 6, 0 );
                }
            }
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


//vvvvvvv  dev vvvvvv

    Graphics_Init( NULL, NULL );

    {
        uint32 i;
        for (i=0; i<7; i++ )
            Graphic_PutPixel( 10 + i*10, 15, 1 );       // button positions
        for (i=0; i<31; i++ )
        {
            Graphic_PutPixel( 10 + i*2, 27, 1 );       // separator lines for button events
            Graphic_PutPixel( 10 + i*2, 27+6, 1 );       // separator lines for button events
            Graphic_PutPixel( 10 + i*2, 27+12, 1 );       // separator lines for button events
        }
    }

    DispHAL_UpdateScreen();

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

#ifdef ON_QT_PLATFORM
        wake_up = HW_GetWakeUpReason();     // since HW_Sleep isn't blocker
#endif

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


