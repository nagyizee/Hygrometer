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


#include "eeprom_spi.h"     //dev

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


//dev
uint8 buffer[550];



static inline void System_Poll( void )
{
    enum EPowerMode pwr_mode = pm_hold;

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
}


//dev
static void local_check_result(uint32 length)
{
    int i;
    for (i=0; i<length; i++)
    {
        if ( (buffer[i*2] != (uint8)((i>>8)&0xff)) ||
             (buffer[i*2+1] != (uint8)(i&0xff))       )
        {
            HW_LED_On();
            HW_LED_Off();
            HW_LED_On();
            HW_LED_Off();
            HW_LED_On();
            HW_LED_Off();
            while(1);               // error found - halt here
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


//////////// dev vvvvvvvvvvvv
    {
        int i;
        uint32 base_addr;

        __disable_interrupt();
        HW_LED_Off();

        for (i=0; i<275; i++)
        {
            buffer[i*2] = (uint8)((i>>8)&0xff);
            buffer[i*2+1] = (uint8)(i&0xff);
        }

        eeprom_init();
        eeprom_enable(true);

        // write operations
        i=0;
        while ( i < 2 )
        {
            base_addr = i*0x1000;
            // write synchronously small amount inside page
            HW_LED_On();
            eeprom_write( base_addr + 0x10, buffer, 6, i );
            HW_LED_Off();
            HW_LED_On();
            while ( eeprom_is_operation_finished() == false );
            HW_LED_Off();
            // write synchronously small amount bw. pages
            HW_LED_On();
            eeprom_write( base_addr + 0xfe, buffer, 6, i );
            HW_LED_Off();
            HW_LED_On();
            while ( eeprom_is_operation_finished() == false );
            HW_LED_Off();
    
            // write synchronously larger amount inside page
            HW_LED_On();
            eeprom_write( base_addr + 0x110, buffer, 200, i );
            HW_LED_Off();
            HW_LED_On();
            while ( eeprom_is_operation_finished() == false );
            HW_LED_Off();
            // write synchronously larger amount bw. pages
            HW_LED_On();
            eeprom_write( base_addr + 0x1f0, buffer, 200, i );
            HW_LED_Off();
            HW_LED_On();
            while ( eeprom_is_operation_finished() == false );
            HW_LED_Off();
    
            // write synchronously large multipage data
            HW_LED_On();
            eeprom_write( base_addr + 0x2f0, buffer, 550, i );
            HW_LED_Off();
            HW_LED_On();
            while ( eeprom_is_operation_finished() == false );
            HW_LED_Off();

            i++;
        }

        // try low power mode and re-enabling
        while( !BtnGet_Mode() )
        
        eeprom_deepsleep();
        eeprom_is_operation_finished();     // just try it out
        HW_LED_On();
        HW_LED_Off();
        HW_LED_On();
        HW_LED_Off();
        HW_LED_On();
        eeprom_enable(false);
        HW_LED_Off();

        HW_Delay( 500000 );
        while( !BtnGet_Mode() )


        for (i=0; i<550; i++)
        {
            buffer[i] = 0;
        }

        // read operations 
        i = 0;
        while ( i < 2 )
        {
            int j;
            const uint32 addresses[] = { 0x10, 0xfe, 0x110, 0x1f0, 0x2f0 };
            const uint32 lengths[]   = {   6,    6,   200,   200,   550  };
            uint32 addr;
            uint32 len;

            base_addr = i*0x1000;

            for ( j=0; j<5; j++ )
            {
                addr = base_addr + addresses[ j + i*5 ];
                len = lengths[ j + i*5 ];

                HW_LED_On();
                eeprom_read( addr, len, buffer, i );
                HW_LED_Off();
                HW_LED_On();
                while ( eeprom_is_operation_finished() == false );      // will exit imediately for sync operations
                HW_LED_Off();
                local_check_result(len);
            }

            i++;
        }


        eeprom_enable(true);
        eeprom_erase();
        HW_LED_On();
        while ( eeprom_is_operation_finished() == false );      // will exit imediately for sync operations
        HW_LED_Off();
        HW_Delay( 500000 );
        while( !BtnGet_Mode() )


        for ( i=0; i<128; i++ )
        {
            int j;
            HW_LED_On();
            eeprom_read( 256*i, 256, buffer, false );
            HW_LED_Off();
        }
            


        while(1);
    }
//////////// dev ^^^^^^^^^^^^
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


