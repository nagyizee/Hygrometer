/*
 *
 *          - Core routines for the Thermo/Hygro/Barograph -
 *
 *
 *      Working sequence:
 *          - after CPU start-up and initialization:
 *
 *              PowerUP case:  - User_Power_Up - when the power button is pressed by user.
 *                               This initiates the UI, sensor in view begin real time acquisition. If UI stopped - sensor is disabled
 *                               If monitoring task - all sensors are aquisitioning at low rate except the on-view sensor
 *                               If registering task - sensor acquisition is done at the set rate
 *                               At power-save timer out:
 *                                      - if monitoring task - will stop only - no power down
 *                                      - if registering task - will power down with alarm set if rate is very low ( 10 min or > )
 *                                      - if no task - will power down
 *
 *                             - Alarm power up - system wake up on RTC alarm.
 *                               Ui remains dormant - no reaction to buttons
 *                               Do the scheduled measurements:
 *                                      - if registering task - register the sensor reading - schedule for the next period
 *                                      - if no task - read the sensors and battery - schedule for next long period wake-up
 *
 *         - Operation modes:
 *              - standby mode: - UI will refresh the readings of the current sensor in real time,
 *                                other sensors are checked for long period for alert.
 *                                - Display off will power down
 *                                - Power button will power down
 *                                - power down will schedule for long period sensor reading
 *
 *              - monitoring mode: - UI will refresh the readings of the current sensor in real time,
 *                                the other sensors are read at monitoring period (2sec. TBD), min/max values are processed, measurements
 *                                are averaged for monitoring rate (10sec, 30sec, 1min, 5min, 30min) for tendency update
 *
 *              - registering mode: - UI will refresh the readings of the current sensor in real time,
 *                                the other sensors are read at monitoring period (2sec. TBD) in case of high rate or low rate w averaging, values are
 *                                averaged to the registration rate, and registered.
 *                                for low rate w/o averaging - sensor read is done at low rate.
 *
 *
 *
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

static uint32   RTCclock;           // user level RTC clock - when entering in core loop with 0.5sec event the RTC clock is copied, and this value is used till the next call


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

static uint32 internal_time_unit_2_seconds( uint32 time_unit )
{
    switch ( time_unit )
    {
        case ut_5sec:       return 5;
        case ut_10sec:      return 10;
        case ut_30sec:      return 30;
        case ut_1min:       return 60;
        case ut_2min:       return 120;
        case ut_5min:       return 300;
        case ut_10min:      return 600;
        case ut_30min:      return 1800;
        default:            return 0;
    }
}

static void internal_clear_monitoring(void)
{
    // clear monitoring averages and schedules
    memset( &core.op.sread.moni, 0, sizeof(core.op.sread.moni) );

    // clear tendency graphs
    core.measure.tendency.temp.c = 0;
    core.measure.tendency.temp.w = 0;
    core.measure.tendency.RH.c = 0;
    core.measure.tendency.RH.w = 0;
    core.measure.tendency.abshum.c = 0;
    core.measure.tendency.abshum.w = 0;
    core.measure.tendency.press.c = 0;
    core.measure.tendency.press.w = 0;
}

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


static inline void local_process_temp_sensor_result( uint32 temp )
{
    // temperature is provided in 16fp9 + 40*C
    if ( temp != core.measure.measured.temperature )
    {
        core.measure.measured.temperature = temp;
        core.measure.dirty.b.upd_temp = 1;

        if ( core.op.op_flags.b.op_monitoring )     // check for min/max
        {
            int i;

            if ( temp == 0x0000 )       // -40*C the absolute minimum of the device - marks also uninitted temperature min/max - omit this value
                temp = 0x0001;

            for ( i=0; i<STORAGE_MINMAX; i++ )
            {
                if ( (core.measure.minmax.temp_min[i] == 0) ||
                     (core.measure.minmax.temp_min[i] > temp) )
                {
                    core.measure.minmax.temp_min[i] = temp;
                    core.measure.dirty.b.upd_temp_minmax = 1;
                }
                if ( (core.measure.minmax.temp_max[i] == 0) ||
                     (core.measure.minmax.temp_max[i] < temp) )
                {
                    core.measure.minmax.temp_max[i] = temp;
                    core.measure.dirty.b.upd_temp_minmax = 1;
                }
            }
        }
    }

    // if monitoring - calculate the average for the tendency graph
    if ( core.op.op_flags.b.op_monitoring )
    {
        if ( temp == 0x0000 )       // -40*C the absolute minimum of the device - marks also uninitted temperature min/max - omit this value
            temp = 0x0001;

        core.op.sread.moni.avg_sum_temp += temp;
        core.op.sread.moni.avg_ctr_temp++;
        // if scheduled tendency update reached - update the tendency list
        if ( core.op.sread.moni.sch_moni_temp <= RTCclock )
        {
            int w_temp = core.measure.tendency.temp.w;
            int c_temp = core.measure.tendency.temp.c;
            // store the average of the measurements
            core.measure.tendency.temp.value[ w_temp++ ] = core.op.sread.moni.avg_sum_temp / core.op.sread.moni.avg_ctr_temp;
            if ( w_temp == STORAGE_TENDENCY )
                w_temp = 0;
            if ( c_temp < STORAGE_TENDENCY )
                c_temp++;
            core.measure.tendency.temp.w = w_temp;
            core.measure.tendency.temp.c = c_temp;
            // clean up the average and set up for the next session
            core.op.sread.moni.avg_ctr_temp = 0;
            core.op.sread.moni.avg_sum_temp = 0;
            core.op.sread.moni.sch_moni_temp += 2 * internal_time_unit_2_seconds( core.setup.tim_tend_temp );
            // notify the UI to update
            core.measure.dirty.b.upd_th_tendency = 1;
        }
    }
}


// OBSOLETED - remove it
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
        case tu_C:
            return (int)( ((int)temp16fp9 * 100) >> TEMP_FP ) - 4000;
        case tu_F:
            return (int)( ((int)temp16fp9 * 180) >> TEMP_FP ) - 4000;       // see the mathcad sheet why this formula
        case tu_K:
            return (int)( ((int)temp16fp9 * 100) >> TEMP_FP ) + 23315;              // substract the 40*C from the 273.15*K
    }
    return 0;
}

uint32 core_utils_unit2temperature( int temp100, enum ETemperatureUnits unit )
{
    switch ( unit )
    {
        case tu_C:
            if ( temp100 >= 8800 )
                return 0xffff;
            if ( temp100 <= -4000 )
                return 0x0000;
            return (( temp100 + 4000 ) << TEMP_FP) / 100;
        case tu_F:
            if ( temp100 >= 19040 )     // 88*C
                return 0xffff;
            if ( temp100 <= -4000 )     // -40*C
                return 0x0000;
            return (( temp100 + 4000 ) << TEMP_FP) / 180;
        case tu_K:
            if ( temp100 >= 36115 )
                return 0xffff;
            if ( temp100 <= 23315 )
                return 0x0000;
            return (( temp100 - 23315 ) << TEMP_FP) / 100;
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


void core_beep( enum EBeepSequence beep )
{
    if ( core.setup.beep_on )
    {
        BeepSequence( (uint32)beep );
    }
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

    setup->show_unit_temp = tu_C;
    setup->show_mm_temp = ( mms_set1 | (mms_set2 << 4) | (mms_day_crt << 8) );
    setup->show_mm_hygro = ( mms_set1 | (mms_set2 << 4) | (mms_day_crt << 8) );
    setup->show_mm_press = mms_set1;

    core.setup.tim_tend_temp = ut_5sec;

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


void core_op_realtime_sensor_select( enum ESensorSelect sensor )
{
    if ( sensor == ss_none )
        core.op.op_flags.b.sens_real_time = 0;
    switch ( sensor )
    {
        case ss_thermo:
            if ( core.op.sread.sch_thermo < RTCclock )
                core.op.sread.sch_thermo = RTCclock;        // will be rescheduled by the core_loop() at first RTC tick
            break;
    }

}

void core_op_monitoring_switch( bool enable )
{
    if ( enable == false )
    {
        // disabling monitoring - clean up the 
        core.op.op_flags.b.op_monitoring = 0;
        internal_clear_monitoring();
    }
    else if ( core.op.op_flags.b.op_monitoring == 0 )
    {
        internal_clear_monitoring();
        core.op.sread.moni.sch_moni_temp = RTCclock + 2 * internal_time_unit_2_seconds( core.setup.tim_tend_temp );
        core.op.op_flags.b.op_monitoring = 1;
    }
}

void core_op_monitoring_rate( enum ESensorSelect sensor, enum EUpdateTimings timing )
{
    if ( sensor == ss_none )
        return;
    switch ( sensor )
    {
        case ss_thermo: 
            if ( core.setup.tim_tend_temp != timing )
            {
                core.setup.tim_tend_temp = timing;
                if ( core.op.op_flags.b.op_monitoring )
                {
                    core.op.sread.moni.avg_ctr_temp = 0;
                    core.op.sread.moni.avg_sum_temp = 0;
                    core.op.sread.moni.sch_moni_temp = RTCclock + 2 * internal_time_unit_2_seconds( timing );
                    core.measure.tendency.temp.c = 0;
                    core.measure.tendency.temp.w = 0;
                }
            }
            break;
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

    Sensor_Init();
    local_update_battery();
    BeepSetFreq( core.setup.beep_low, core.setup.beep_hi );

    // -- set up initial schedule clocks
    RTCclock = RTCctr;
    core.op.sread.sch_thermo = RTCclock;
    core.op.sread.sch_hygro = RTCclock;
    core.op.sread.sch_press = RTCclock;

    // -- move this to the Backup domain simulation
    core_op_monitoring_rate( ss_thermo, core.setup.tim_tend_temp );
    core_op_monitoring_switch( true );

    return 0;

_err_exit:
    return -1;
}


void core_poll( struct SEventStruct *evmask )
{
    // if no operation to be done - return
    if ( core.op.op_flags.val == 0 )
        return;

    // operations to be executed on RTC tick
    if ( evmask->timer_tick_05sec )
    {
        RTCclock = RTCctr;      // do a local copy

        if ( (core.op.sread.sch_thermo <= RTCclock) )
        {
            if ( core.op.op_flags.b.op_monitoring || (core.op.op_flags.b.sens_real_time == ss_thermo) )
            {
                Sensor_Acquire( SENSOR_TEMP );
                core.op.op_flags.b.check_sensor |= SENSOR_TEMP; 

                if (core.op.op_flags.b.sens_real_time == ss_thermo)
                    core.op.sread.sch_thermo += CORE_SCHED_TEMP_REALTIME;
                else
                    core.op.sread.sch_thermo += CORE_SCHED_TEMP_MONITOR;
            }
        }
    }

    // poll the sensor module if waiting data from sensors
    if ( core.op.op_flags.b.check_sensor )
    {
        if ( Sensor_Is_Ready() & SENSOR_TEMP )
        {
            local_process_temp_sensor_result( Sensor_Get_Value(SENSOR_TEMP) );
            core.op.op_flags.b.check_sensor &= ~SENSOR_TEMP;
        }
    }


    return;
}


int  core_get_pwrstate()
{
    if ( core.op.op_flags.val == 0 )
        return SYSSTAT_CORE_STOPPED;
}








