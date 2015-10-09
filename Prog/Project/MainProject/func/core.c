/*
 *
 *
 **/

#include <stdio.h>
#include <string.h>
#include "core.h"
#include "events_ui.h"
#include "hw_stuff.h"
#include "eeprom_spi.h"

#ifdef ON_QT_PLATFORM
struct STIM1 stim;
struct STIM1 *TIM1 = &stim;
struct STIM1 *TIMER_ADC;
#endif

struct SCore  core;


//////////////////////////////////////////////////////////////////
//
//      ISR RELATED DEFINES AND ROUTINES - handle with care
//
//////////////////////////////////////////////////////////////////

// Debug feature for loop efficiency
//#define STATISTICKS
#define STAT_TMR_CAL

volatile struct SEventStruct events = { 0, };
static volatile uint32 counter = 0;
static volatile uint32 RTCctr = 0;
static volatile uint32 sec_ctr = 0;
static volatile uint32 rcc_comp = 0x10;
static volatile uint32 tmr_over = 0;

#ifdef STAT_TMR_CAL
static volatile uint32 tmr_over_max = 0;
static volatile uint32 tmr_under_max = 0;
#endif


extern void DispHAL_ISR_Poll(void);


    void TimerSysIntrHandler(void)
    {
        // !!!!!!! IMPORTANT NOTE !!!!!!!!
        // IF timer 16 in use - check for the interrupt flag

        // Clear update interrupt bit
        TIMER_SYSTEM->SR = (uint16)~TIM_FLAG_Update;

        if ( sec_ctr < 2000 )  // execute this isr only for useconds inside the 1second interval
        {
            counter++;
            sec_ctr++;
            events.timer_tick_system = 1;

            if ( counter == SYSTEM_T_10MS_COUNT )
            {
                events.timer_tick_10ms = 1;
                counter = 0;
            }
            DispHAL_ISR_Poll();
        }
        else
        {
            // TODO: can do stats - detect faster than normal internal osclillator speed
            tmr_over++;
        }

    }//END: Timer1IntrHandler


    void TimerRTCIntrHandler(void)
    {
        // clear the interrrupt flag and update clock alarm for the next second
        TIMER_RTC->CRL &= (uint16)~RTC_ALARM_FLAG;
        RTC_WaitForLastTask();
        RTCctr = RTC_GetCounter();
        RTC_SetAlarm( RTCctr + 1 );

        // adjust internal oscillator for precision clock
        if ( tmr_over )                         // timer overshoot
        {
            if ( rcc_comp > 0 )
            {
                rcc_comp--;
                RCC_AdjustHSICalibrationValue( rcc_comp );
            }
        }
        else if ( sec_ctr < 1999 )              // timer undershoot
        {
            if ( rcc_comp < 0x1F )
            {
                rcc_comp++;
                RCC_AdjustHSICalibrationValue( rcc_comp );
            }
        }


    #ifdef STAT_TMR_CAL
        if ( (tmr_over) && (tmr_over > tmr_over_max) )
        {
            tmr_over_max = tmr_over;
        }
        else if ( (sec_ctr < 1999) && ( (1999-sec_ctr) > tmr_under_max ) && (sec_ctr > 1250) )
        {
            tmr_under_max = (1999-sec_ctr);
        }
    #endif

        tmr_over = 0;
        counter = 0;
        sec_ctr = 0;
        events.timer_tick_system = 1;
        events.timer_tick_10ms = 1;
        events.timer_tick_05sec = 1;

        // reset everything and signal the events
        if ( (RTCctr & 0x01) == 0)
        {
            events.timer_tick_1sec = 1;
        }
    }//END: Timer1IntrHandler



/////////////////////////////////////////////////////
//
//   internal routines
//
/////////////////////////////////////////////////////


static void local_update_battery()
{
    uint32 battery;
    battery = HW_ADC_GetBattery();

    // limit battery value and remove offset
    if (battery > VBAT_MIN )
        battery -= VBAT_MIN;
    else
        battery = 0;
    if ( battery > VBAT_DIFF )
        battery = VBAT_DIFF;
    // scale to 0 - 100%
    core.measure.battery = (uint8)((battery * 100) / VBAT_DIFF);     
}

static int local_calculate_dewpoint( int temp, int rh )
{

}

static int local_calculate_abs_humidity( int temp, int rh )
{

}


static inline void local_poll_measurements( void )
{
    int msr;

    // acquire the measured parameters
    msr = HWDBG_Get_Temp();
    if ( msr != core.measure.measured.temperature )
    {
        core.measure.measured.temperature = msr;
        core.measure.dirty.b.upd_temp = 1;
    }
    msr = HWDBG_Get_Humidity();
    if ( msr != core.measure.measured.rh )
    {
        core.measure.measured.rh = msr;
        core.measure.dirty.b.upd_hum = 1;
    }
    msr = HWDBG_Get_Pressure();
    if ( msr != core.measure.measured.pressure )
    {
        core.measure.measured.pressure = msr;
        core.measure.dirty.b.upd_pressure = 1;
    }

    // calculate derivated values
    msr = local_calculate_dewpoint( core.measure.measured.temperature, core.measure.measured.rh );
    if ( msr != core.measure.measured.dewpoint )
    {
        core.measure.measured.dewpoint = msr;
        core.measure.dirty.b.upd_hum = 1;
    }
    msr = local_calculate_abs_humidity( core.measure.measured.temperature, core.measure.measured.rh );
    if ( msr != core.measure.measured.absh )
    {
        core.measure.measured.absh = msr;
        core.measure.dirty.b.upd_hum = 1;
    }
}

/////////////////////////////////////////////////////
//
//   main routines
//
/////////////////////////////////////////////////////

int core_utils_temperature2unit( uint16 temp16fp9, enum ETemperatureUnits unit )
{
    if ( temp16fp9 == 0 )
        return NUM100_MIN;
    if ( temp16fp9 == 0xffff )
        return NUM100_MAX;

    switch ( unit )
    {
        case ut_C:
            return (int)( ((int)temp16fp9 * 100) >> TEMP_FP ) - 4000;
        case ut_F:
            return (int)( ( ((int)temp16fp9 * 900) / 5 ) >> TEMP_FP ) - 4000;       // see the mathcad sheet why this formula
        case ut_K:
            return (int)( ((int)temp16fp9 * 100) >> TEMP_FP ) + 23315;              // substract the 40*C from the 273.15*K
    }
    return 0;
}



uint32 core_get_clock_counter()
{
    uint32 rtc_val;
    __disable_interrupt();
    rtc_val = RTCctr;
    __enable_interrupt();
    return rtc_val;
}

void core_set_clock_counter( uint32 counter )
{
    uint32 rtc_val;
    __disable_interrupt();
    RTCctr = counter;
    //TBD if something needs to be done for alarm setup/etc.
    __enable_interrupt();
}

//-------------------------------------------------
//     Setup save / load
//-------------------------------------------------

int core_setup_save( void )
{
    int i;
    uint16  cksum       = 0xABCD;
    uint8   *SetParams  = (uint8*)&core.setup;

    eeprom_init();

    if ( eeprom_enable(true) )
        return -1;

    for ( i=0; i<sizeof(struct SCoreSetup); i++ )
        cksum = cksum + ( (uint16)(SetParams[i] << 8) - (uint16)(~SetParams[i]) ) + 1;

    if ( eeprom_write( 0x10, SetParams, sizeof(struct SCoreSetup) ) != sizeof(struct SCoreSetup) )
        return -1;
    if ( eeprom_write( 0x10 + sizeof(struct SCoreSetup), (uint8*)&cksum, 2 ) != 2 )
        return -1;
    eeprom_disable();
    return 0;
}


int core_setup_reset( bool save )
{
    struct SCoreSetup *setup = &core.setup;

    setup->disp_brt_on  = 0x30;
    setup->disp_brt_dim = 12;
    setup->pwr_stdby = 30;       // 30sec standby
    setup->pwr_disp_off = 60;    // 1min standby
    setup->pwr_off = 300;        // 5min pwr off

    setup->beep_on = 1;
    setup->beep_hi = 1554;
    setup->beep_low = 2152;

    setup->show_unit_temp = ut_C;
    setup->show_mm_temp = ( mms_set1 | (mms_set2 << 4) | (mms_day_crt << 8) );
    setup->show_mm_hygro = ( mms_set1 | (mms_set2 << 4) | (mms_day_crt << 8) );
    setup->show_mm_press = mms_set1;

    if ( save )
    {
        return core_setup_save();
    }
    return 0;
}


int core_setup_load( void )
{
    uint8   i;
    uint16  cksum       = 0xABCD;
    uint16  cksumsaved  = 0;
    uint8   *SetParams  = (uint8*)&core.setup;

    if ( eeprom_enable(false) )
        return -1;

    if ( eeprom_read( 0x10, sizeof(struct SCoreSetup), SetParams ) != sizeof(struct SCoreSetup) )
        return -1;
    if ( eeprom_read( 0x10 + sizeof(struct SCoreSetup), 2, (uint8*)&cksumsaved ) != 2 )
        return -1;

    for ( i=0; i<sizeof(struct SCoreSetup); i++ )
    {
        cksum = cksum + ( (uint16)(SetParams[i] << 8) - (uint16)(~SetParams[i]) ) + 1;
    }

    if ( cksum != cksumsaved )
    {
        // eeprom data corrupted or not initialized
        if ( core_setup_reset( true ) )
            return -1;
    }

    eeprom_disable();
    return 0;
}


void core_beep( enum EBeepSequence beep )
{
    if ( core.setup.beep_on )
    {
        BeepSequence( (uint32)beep );
    }
}



//-------------------------------------------------
//     main core interface
//-------------------------------------------------

int core_init( struct SCore **instance )
{
    memset(&core, 0, sizeof(core));
    if ( instance )
        *instance = &core;

    if ( eeprom_init() )
        goto _err_exit;
    if ( core_setup_load() )
        goto _err_exit;

    local_update_battery();
    BeepSetFreq( core.setup.beep_low, core.setup.beep_hi );
    return 0;

_err_exit:
    return -1;
}


void core_poll( struct SEventStruct *evmask )
{
    if ( evmask->timer_tick_05sec )
    {
        // do the measurements at 1/2 seconds
        local_poll_measurements();

    }


    return;
}


int  core_get_pwrstate()
{
    if ( 0 ) // core.op.op_state == 0 )                // if core is stopped we don't care about adc - ui should take care
        return SYSSTAT_CORE_STOPPED;
}








