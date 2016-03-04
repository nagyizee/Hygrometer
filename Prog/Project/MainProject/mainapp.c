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
static bool wake_up = false;

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


//dev 
static uint32 crt_mode;

static inline void System_Poll( void )
{
    enum EPowerMode pwr_mode = pm_down;

    CheckStack();
/*dev
    sys_pwr |= core_pwr_getstate();
    sys_pwr |= DispHAL_App_Poll();
    if ( BeepIsRunning() )
        sys_pwr |= SYSSTAT_UI_ON;
*/
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
}


// Main application routine
static inline void ProcessApplication( struct SEventStruct *evmask )
{

    core_poll( evmask );
/*dev
    if ( evmask->timer_tick_10ms || evmask->key_event )
    {
        ui_st = ui_poll( evmask );
    }
    sys_pwr |= ui_st;
    */

// dev
    
    
    if ( (evmask->key_pressed & KEY_ESC) ||                                        // if ESC released in full/sleep power mode
         ( (crt_mode & (PM_HOLD | PM_HOLD_BTN | PM_DOWN)) &&  BtnGet_Esc())  )      // or pressed in stopped/pwdown mode
    {
        crt_mode = crt_mode << 1;       // next power mode
        if ( (crt_mode & 0x1F) == 0 )
        {
            crt_mode = PM_FULL;
        }
        BKP_WriteBackupRegister( BKP_DR2, crt_mode );
    }
   
    sys_pwr = crt_mode;
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

//dev 
    crt_mode = BKP_ReadBackupRegister(BKP_DR2);
    sys_pwr = crt_mode;
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


