/*
 *
 *          - Core routines for the Thermo/Hygro/Barograph -
 *
 *
 *      Working sequence:
 *          - after CPU start-up and initialization:
 *              PowerUP case:  - User_Power_Up - when the power button is pressed by user.
 *                               Case_1: First start-up - timer initted from 0, operational mode set to "standby", uninitted state marked(first_start = 1)
 *                                                        UI started up with date/time entry requirement. Can go in low power but state is remembered
 *                                                        Sensors initted, barometer first read - adjust the low pass filter
 *                                                        Schedule next battery check to 5sec, next sensor read to 60sec.
 *                                                      - NOTE: ui in start state - it is started up only when long press detected on pwr button, but the
 *                                                                                  rest of operations are done
 *                                                      - NV states are saved
 *                               Case_2: Next start-ups - ui in start state - wait for long press
 *                                                      - check for schedule time:
 *                                                              - if none - wait till UI confirms long press -> valid power on, else sleep
 *                                                              - if yes - valid power on
 *                                                        For valid power on:
 *                                                          - init sensors / get data from NVram
 *
 *                               At power-save timer out:
 *                                      - if monitoring task and registering task - will power down with alarm set.
 *                                      - if no task - will power down with battery and pressure low period read schedule
 *
 *                             - Alarm power up - system wake up on RTC alarm.
 *
 *
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
#include "utilities.h"
#include "sensors.h"

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

volatile struct SEventStruct events = { 0, };
static volatile uint32 counter = 0;
static volatile uint32 RTCctr = 0;
static volatile uint32 sec_ctr = 0;


static uint32   RTCclock;           // user level RTC clock - when entering in core loop with 0.5sec event the RTC clock is copied, and this value is used till the next call


extern void DispHAL_ISR_Poll(void);


    void TimerSysIntrHandler(void)
    {
        // !!!!!!! IMPORTANT NOTE !!!!!!!!
        // IF timer 15 in use - check for the interrupt flag

        // Clear update interrupt bit
        HW_LED_On();
        TIMER_SYSTEM->SR = (uint16)~TIM_FLAG_Update;

        if ( sec_ctr < 500 )  // execute this isr only for useconds inside the 0.5second interval
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

    }//END: Timer1IntrHandler


    void TimerRTCIntrHandler(void)
    {
        // clear the interrrupt flag and update clock alarm for the next second
        HW_LED_On();
        TIMER_RTC->CRL &= (uint16)~RTC_ALARM_FLAG;

        RTC_WaitForLastTask();
        RTCctr = RTC_GetCounter();
        RTC_SetAlarm( RTCctr + 1 );

        // adjust internal oscillator for precision clock
        counter = 0;
        sec_ctr = 0;
        events.timer_tick_system = 1;
        events.timer_tick_10ms = 1;
        events.timer_tick_05sec = 1;
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
        case ut_60min:      return 3600;
        default:            return 0;
    }
}

static void internal_clear_monitoring(void)
{
    uint32 i;

    // clear monitoring averages and schedules
    memset( &core.nv.op.sens_rd.moni, 0, sizeof(core.nv.op.sens_rd.moni) );

    // clear tendency graphs
    for (i=0; i<CORE_MSR_SET; i++)
    {
        core.nv.op.sens_rd.tendency[i].c = 0;
        core.nv.op.sens_rd.tendency[i].w = 0;
    }
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

static int local_calculate_dewpoint( uint32 temp, uint32 rh )
{

    return 0;
}

static int local_calculate_abs_humidity( uint32 temp, uint32 rh )
{

    return 0;
}

static void local_minmax_compare_and_update( uint32 entry, uint32 value )
{
    struct SMinimMaxim  *mmentry;
    bool update = false;
    int i;

    mmentry = &core.nv.op.sens_rd.minmax[entry];

    for ( i=0; i<mms_day_bfr; i++ )         // set up only the current values
    {
        if ( (mmentry->min[i] == 0) || (mmentry->min[i] > value) )
        {
            mmentry->min[i] = value;
            update = true;
        }
        if ( (mmentry->max[i] == 0) || (mmentry->max[i] < value) )
        {
            mmentry->max[i] = value;
            update = true;
        }
    }

    if ( update )
    {
        switch ( entry )
        {
            case CORE_MMP_TEMP:     core.measure.dirty.b.upd_temp_minmax = 1;   break;
            case CORE_MMP_RH:       core.measure.dirty.b.upd_hum_minmax = 1;    break;
            case CORE_MMP_ABSH:     core.measure.dirty.b.upd_abshum_minmax = 1; break;
            case CORE_MMP_PRESS:    core.measure.dirty.b.upd_press_minmax = 1;  break;
        }
    }
}

static void local_tendency_add_entry( uint32 entry, uint32 value )
{
    int w_temp = core.nv.op.sens_rd.tendency[entry].w;
    int c_temp = core.nv.op.sens_rd.tendency[entry].c;
    // store the average of the measurements
    core.nv.op.sens_rd.tendency[entry].value[ w_temp++ ] = value;
    if ( w_temp == STORAGE_TENDENCY )
        w_temp = 0;
    if ( c_temp < STORAGE_TENDENCY )
        c_temp++;
    core.nv.op.sens_rd.tendency[entry].w = w_temp;
    core.nv.op.sens_rd.tendency[entry].c = c_temp;
}

static void local_tendency_restart( uint32 entry )
{
    core.nv.op.sens_rd.tendency[entry].c = 0;
    core.nv.op.sens_rd.tendency[entry].w = 0;
}


static inline void local_process_temp_sensor_result( uint32 temp )
{
    // temperature is provided in 16fp9 + 40*C
    if ( temp != core.measure.measured.temperature )
    {
        core.measure.measured.temperature = temp;
        core.measure.dirty.b.upd_temp = 1;

        if ( core.nv.op.op_flags.b.op_monitoring )  // in monitoring mode - check for min/max
        {
            if ( temp == 0x0000 )                   // -40*C the absolute minimum of the device - marks also uninitted temperature min/max - omit this value
                temp = 0x0001;

            local_minmax_compare_and_update( CORE_MMP_TEMP, temp );
        }
    }

    // if monitoring - calculate the average for the tendency graph
    if ( core.nv.op.op_flags.b.op_monitoring )
    {
        if ( temp == 0x0000 )       // -40*C the absolute minimum of the device - marks also uninitted temperature min/max - omit this value
            temp = 0x0001;

        core.nv.op.sens_rd.moni.avg_sum_temp += temp;
        core.nv.op.sens_rd.moni.avg_ctr_temp++;
        // if scheduled tendency update reached - update the tendency list
        if ( core.nv.op.sens_rd.moni.sch_moni_temp <= RTCclock )
        {
            // add the average value in the fifo
            local_tendency_add_entry( CORE_MMP_TEMP, core.nv.op.sens_rd.moni.avg_sum_temp / core.nv.op.sens_rd.moni.avg_ctr_temp );
            // clean up the average and set up for the next session
            core.nv.op.sens_rd.moni.avg_ctr_temp = 0;
            core.nv.op.sens_rd.moni.avg_sum_temp = 0;
            core.nv.op.sens_rd.moni.sch_moni_temp += 2 * internal_time_unit_2_seconds( core.nv.setup.tim_tend_temp );
            // notify the UI to update
            core.measure.dirty.b.upd_th_tendency = 1;
        }
    }
}


static inline void local_process_hygro_sensor_result( uint32 rh )
{
    uint32 dew;     // calculated dew point temperature in 16FP9+40*
    uint32 abs;     // calculated absolute humidity in g/m3*100

    dew = local_calculate_dewpoint( core.measure.measured.temperature, rh );
    abs = local_calculate_abs_humidity( core.measure.measured.temperature, rh );
    rh = ((rh * 100) >> RH_FP);     // calculate the x100 % value from 16FP8

    // temperature is provided in 16fp9 + 40*C
    if ( (rh != core.measure.measured.temperature) ||
         (dew != core.measure.measured.dewpoint) ||
         (abs != core.measure.measured.absh) )
    {
        core.measure.measured.rh = rh;
        core.measure.measured.absh = abs;
        core.measure.measured.dewpoint = dew;
        core.measure.dirty.b.upd_hygro = 1;

        if ( core.nv.op.op_flags.b.op_monitoring )     // check for min/max
        {
            if ( rh == 0 )       // rh 0 % marks also uninitted temperature min/max - omit this value
                rh = 1;
            if ( abs == 0 )
                abs = 1;

            local_minmax_compare_and_update( CORE_MMP_RH, rh );
            local_minmax_compare_and_update( CORE_MMP_ABSH, abs );
        }
    }

    // if monitoring - calculate the average for the tendency graph
    if ( core.nv.op.op_flags.b.op_monitoring )
    {
        if ( rh == 0 )       // rh 0 % marks also uninitted temperature min/max - omit this value
            rh = 1;
        if ( abs == 0 )
            abs = 1;

        core.nv.op.sens_rd.moni.avg_sum_rh += rh;
        core.nv.op.sens_rd.moni.avg_sum_abshum += abs;
        core.nv.op.sens_rd.moni.avg_ctr_hygro++;
        // if scheduled tendency update reached - update the tendency list
        if ( core.nv.op.sens_rd.moni.sch_moni_hygro <= RTCclock )
        {
            // add the average value in the fifo
            local_tendency_add_entry( CORE_MMP_RH, core.nv.op.sens_rd.moni.avg_sum_rh / core.nv.op.sens_rd.moni.avg_ctr_hygro );
            local_tendency_add_entry( CORE_MMP_ABSH, core.nv.op.sens_rd.moni.avg_sum_abshum / core.nv.op.sens_rd.moni.avg_ctr_hygro );
            // clean up the average and set up for the next session
            core.nv.op.sens_rd.moni.avg_sum_rh = 0;
            core.nv.op.sens_rd.moni.avg_sum_abshum = 0;
            core.nv.op.sens_rd.moni.avg_ctr_hygro = 0;
            core.nv.op.sens_rd.moni.sch_moni_hygro += 2 * internal_time_unit_2_seconds( core.nv.setup.tim_tend_hygro );
            // notify the UI to update
            core.measure.dirty.b.upd_th_tendency = 1;
        }
    }
}


static inline void local_process_pressure_sensor_result( uint32 press )
{
    // pressure comes in 20fp2 Pa (18bit pascal, 2 fractional)
    uint32 pr_filt;
    
    // calculate filtred value
    pr_filt = press;

    // store the measurement and create max/min
    if ( pr_filt != core.measure.measured.pressure )
    {
        core.measure.measured.pressure = pr_filt;
        core.measure.dirty.b.upd_pressure = 1;

        if ( core.nv.op.op_flags.b.op_monitoring )  // in monitoring mode - check for min/max
        {
            local_minmax_compare_and_update( CORE_MMP_PRESS, convert_press_20fp2_16bit(pr_filt) );
        }
    }

    // if monitoring - calculate the average for the tendency graph
    if ( core.nv.op.op_flags.b.op_monitoring )
    {
        core.nv.op.sens_rd.moni.avg_sum_press += convert_press_20fp2_16bit(pr_filt);
        core.nv.op.sens_rd.moni.avg_ctr_press++;
        // if scheduled tendency update reached - update the tendency list
        if ( core.nv.op.sens_rd.moni.sch_moni_press <= RTCclock )
        {
            // add the average value in the fifo
            local_tendency_add_entry( CORE_MMP_PRESS, core.nv.op.sens_rd.moni.avg_sum_press / core.nv.op.sens_rd.moni.avg_ctr_press );
            // clean up the average and set up for the next session
            core.nv.op.sens_rd.moni.avg_ctr_temp = 0;
            core.nv.op.sens_rd.moni.avg_sum_temp = 0;
            core.nv.op.sens_rd.moni.sch_moni_press += 2 * internal_time_unit_2_seconds( core.nv.setup.tim_tend_press );
            // notify the UI to update
            core.measure.dirty.b.upd_press_tendency = 1;
        }
    }
}


static inline void local_push_minmax_set_if_needed(void)
{
    uint32 check_val;
    int i;

    // check for passing day
    check_val = RTCclock / DAY_TICKS;
    if ( check_val != core.nv.op.sens_rd.moni.clk_last_day )
    {

        // shift the min/max values to day before and reset the day
        core.nv.op.sens_rd.moni.clk_last_day = check_val;

        for (i=0; i<CORE_MSR_SET; i++)
        {
            core.nv.op.sens_rd.minmax[i].max[ mms_day_bfr ] = core.nv.op.sens_rd.minmax[i].max[ mms_day_crt ];
            core.nv.op.sens_rd.minmax[i].min[ mms_day_bfr ] = core.nv.op.sens_rd.minmax[i].min[ mms_day_crt ];
        }
         
        core_op_monitoring_reset_minmax( ss_thermo, mms_day_crt );
        core_op_monitoring_reset_minmax( ss_pressure, mms_day_crt );
        core_op_monitoring_reset_minmax( ss_rh, mms_day_crt );
    }

    // check for passing week
    check_val = (RTCclock + DAY_TICKS) / WEEK_TICKS;        // add 1 day_ticks because the counter = 0x0000 starts on Thuesday
    if ( check_val != core.nv.op.sens_rd.moni.clk_last_week )
    {
        // shift the min/max values to day before and reset the day
        core.nv.op.sens_rd.moni.clk_last_week = check_val;

        for (i=0; i<CORE_MSR_SET; i++)
        {
            core.nv.op.sens_rd.minmax[i].max[ mms_week_bfr ] = core.nv.op.sens_rd.minmax[i].max[ mms_week_crt ];
            core.nv.op.sens_rd.minmax[i].min[ mms_week_bfr ] = core.nv.op.sens_rd.minmax[i].min[ mms_week_crt ];
        }

        core_op_monitoring_reset_minmax( ss_thermo, mms_week_bfr );
        core_op_monitoring_reset_minmax( ss_pressure, mms_week_bfr );
        core_op_monitoring_reset_minmax( ss_rh, mms_week_bfr );
    }
}


static inline void local_check_first_scheduled_op(void)
{
    uint32 RTCsched = CORE_SCHED_NONE;

    if ( core.nv.op.sched.sch_thermo < RTCsched )
        RTCsched = core.nv.op.sched.sch_thermo;
    if ( core.nv.op.sched.sch_hygro < RTCsched )
        RTCsched = core.nv.op.sched.sch_hygro;
    if ( core.nv.op.sched.sch_press < RTCsched )
        RTCsched = core.nv.op.sched.sch_press;

    core.nf.next_schedule = RTCsched;
}


static inline void local_check_sensor_read_schedules(void)
{
    if ( core.nv.op.sched.sch_thermo <= RTCclock )
    {
        Sensor_Acquire( SENSOR_TEMP );
        core.vstatus.int_op.f.sens_read |= SENSOR_TEMP; 

        if (core.vstatus.int_op.f.sens_real_time == ss_thermo)
            core.nv.op.sched.sch_thermo += CORE_SCHED_TEMP_REALTIME;
        else if ( core.nv.op.op_flags.b.op_monitoring )
            core.nv.op.sched.sch_thermo += CORE_SCHED_TEMP_MONITOR;
        else
            core.nv.op.sched.sch_thermo = CORE_SCHED_NONE;
    }

    if ( core.nv.op.sched.sch_hygro <= RTCclock )
    {
        Sensor_Acquire( SENSOR_RH );
        core.vstatus.int_op.f.sens_read |= SENSOR_RH; 

        if (core.vstatus.int_op.f.sens_real_time == ss_rh)
            core.nv.op.sched.sch_hygro += CORE_SCHED_RH_REALTIME;
        else if ( core.nv.op.op_flags.b.op_monitoring )
            core.nv.op.sched.sch_hygro += CORE_SCHED_RH_MONITOR;
        else
            core.nv.op.sched.sch_hygro = CORE_SCHED_NONE;
    }

    if ( core.nv.op.sched.sch_press <= RTCclock )
    {
        Sensor_Acquire( SENSOR_PRESS );
        core.vstatus.int_op.f.sens_read |= SENSOR_PRESS; 

        if (core.vstatus.int_op.f.sens_real_time == ss_pressure)
            core.nv.op.sched.sch_press += CORE_SCHED_PRESS_REALTIME;
        else if ( core.nv.op.op_flags.b.op_monitoring )
            core.nv.op.sched.sch_press += CORE_SCHED_PRESS_MONITOR;
        else
            core.nv.op.sched.sch_press += CORE_SCHED_PRESS_STBY;
    }

    local_check_first_scheduled_op();

}


static void local_nvfast_load_struct(void)
{
    core.nf.status.val = BKP_ReadBackupRegister( BKP_DR2 );
    core.nf.next_schedule = BKP_ReadBackupRegister( BKP_DR3 ) | ((uint32)(BKP_ReadBackupRegister( BKP_DR4 )) << 8);

    core.measure.measured.temperature = BKP_ReadBackupRegister( BKP_DR5 );
    core.measure.measured.rh = BKP_ReadBackupRegister( BKP_DR6 );

}

static void local_initialize_core_operation(void)
{
    memset( &core.nv.op, 0, sizeof(core.nv.op) );

    core.nf.next_schedule = RTCclock;

    core.nv.op.sched.sch_thermo = CORE_SCHED_NONE;
    core.nv.op.sched.sch_hygro = CORE_SCHED_NONE;
    core.nv.op.sched.sch_press = RTCclock;                 // schedule only the pressure sensor for read

}


static inline void local_poll_aux_operations(void)
{
    // check battery
    if ( RTCclock >= core.vstatus.battcheck )
    {
        local_update_battery();
        core.measure.dirty.b.upd_battery = 1;
        if ( HW_Charge_Detect() )
        {
            core.vstatus.ui_cmd &= ~CORE_UISTATE_LOW_BATTERY;
            core.vstatus.ui_cmd |= CORE_UISTATE_CHARGING;
        }
        else if ( core.measure.battery < 5 )        // 3.25V treshold or below and no charge
        {
            core.vstatus.ui_cmd |= CORE_UISTATE_LOW_BATTERY;
            core.vstatus.ui_cmd &= ~CORE_UISTATE_CHARGING;
        }

        // schedule to 5sec intervals
        core.vstatus.battcheck = RTCclock + 10;
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
            return (int)( ((int)temp16fp9 * 100) >> TEMP_FP ) + 23315;      // substract the 40*C from the 273.15*K
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



uint32 core_get_clock_counter(void)
{
    return RTCclock;
}

uint32 core_restart_rtc_tick(void)
{
    __disable_interrupt();
    RTC_WaitForLastTask();
    RTCctr = RTC_GetCounter();
    RTC_SetAlarm( RTCctr + 1 );
    __enable_interrupt();
    return 0;
}

void core_set_clock_counter( uint32 counter )
{
    __disable_interrupt();
    RTCctr = counter;
    HW_SetRTC( RTCctr );
    RTC_SetAlarm( RTCctr + 1 );
    //TBD if something needs to be done for alarm setup/etc.
    __enable_interrupt();
}


void core_beep( enum EBeepSequence beep )
{
    if ( core.nv.setup.beep_on )
    {
        BeepSequence( (uint32)beep );
    }
}

//-------------------------------------------------
//     Setup save / load
//-------------------------------------------------

#define EEADDR_SETUP    0x00
#define EEADDR_OPS      (EEADDR_SETUP + sizeof(struct SCoreSetup))
#define EEADDR_CK_SETUP (EEADDR_OPS + sizeof(struct SCoreOperation))
#define EEADDR_CK_OPS   (EEADDR_CK_SETUP + 2)


int core_setup_save( void )
{
    uint32 i;
    uint16  cksum_setup  = 0xABCD;
    uint16  cksum_op     = 0xABCD;
    uint8   *buffer;

    if ( eeprom_enable(true) )
        return -1;

    // calculate checksums
    buffer = (uint8*)&core.nv.setup;
    for ( i=0; i<sizeof(core.nv.setup); i++ )
        cksum_setup = cksum_setup + ( (uint16)(buffer[i] << 8) - (uint16)(~buffer[i]) ) + 1;

    buffer = (uint8*)&core.nv.op;
    for ( i=0; i<sizeof(core.nv.op); i++ )
        cksum_op = cksum_op + ( (uint16)(buffer[i] << 8) - (uint16)(~buffer[i]) ) + 1;

    // wait for enable to be finished
    while ( eeprom_is_operation_finished() == false );

    // write nonvolatile data
    buffer = (uint8*)&core.nv.setup;
    if ( eeprom_write( EEADDR_SETUP, buffer, sizeof(core.nv.setup), true ) != sizeof(core.nv.setup) )
        return -1;
    while ( eeprom_is_operation_finished() == false );

    buffer = (uint8*)&core.nv.op;
    if ( eeprom_write( EEADDR_OPS, buffer, sizeof(core.nv.op), true ) != sizeof(core.nv.op) )
        return -1;
    while ( eeprom_is_operation_finished() == false );

    // write checksums
    if ( eeprom_write( EEADDR_CK_SETUP, (uint8*)&cksum_setup, 2, false ) != 2 )
        return -1;
    if ( eeprom_write( EEADDR_CK_OPS, (uint8*)&cksum_op, 2, false ) != 2 )
        return -1;

    // disable eeprom
    eeprom_disable();
    return 0;
}


int core_setup_reset( bool save )
{
    struct SCoreSetup *setup = &core.nv.setup;

    setup->disp_brt_on  = 0x30;
    setup->disp_brt_dim = 12;
    setup->pwr_stdby = 30*2;          // 30sec standby
    setup->pwr_disp_off = 2*60*2;     // 2min standby
    setup->pwr_off = 5*60*2;          // 5min pwr off

    setup->beep_on = 1;
    setup->beep_hi = 1554;
    setup->beep_low = 2152;

    setup->show_unit_temp = tu_C;
    setup->show_unit_hygro = hu_rh;
    setup->show_unit_press = pu_hpa;
    setup->show_mm_temp = ( mms_set1 | (mms_set2 << 4) | (mms_day_crt << 8) );
    setup->show_mm_hygro = ( mms_set1 | (mms_set2 << 4) | (mms_day_crt << 8) );
    setup->show_mm_press = mms_set1;

    setup->tim_tend_temp = ut_30sec;
    setup->tim_tend_hygro = ut_30sec;
    setup->tim_tend_press = ut_10min;

    local_initialize_core_operation();

    if ( save )
    {
        return core_setup_save();
    }
    return 0;
}


int core_setup_load( bool no_op_load )
{
    uint32  i;
    uint16  cksum;
    uint16  cksum_setup;
    uint16  cksum_op;
    uint8   *buffer;

    if ( eeprom_enable(false) )
        return -1;

    // read the setup and checksum
    buffer = (uint8*)&core.nv.setup;
    if ( eeprom_read( EEADDR_SETUP, sizeof(core.nv.setup), buffer, true ) != sizeof(core.nv.setup) )
        return -1;
    while ( eeprom_is_operation_finished() == false );
    if ( eeprom_read( EEADDR_CK_SETUP, 2, (uint8*)&cksum_setup, false ) != 2 )
        return -1;
    while ( eeprom_is_operation_finished() == false );

    // read the ops if needed
    if ( no_op_load == false )
    {
        buffer = (uint8*)&core.nv.op;
        if ( eeprom_read( EEADDR_OPS, sizeof(core.nv.op), buffer, true ) != sizeof(core.nv.op) )
            return -1;
    }

    // check the setup checksum till op is read
    buffer = (uint8*)&core.nv.setup;
    cksum = 0xABCD;
    for ( i=0; i<sizeof(core.nv.setup); i++ )
        cksum = cksum + ( (uint16)(buffer[i] << 8) - (uint16)(~buffer[i]) ) + 1;
    if ( cksum != cksum_setup )
        goto _error_exit;
    while ( eeprom_is_operation_finished() == false );

    // op read is finished - check the cksum for this also
    if ( no_op_load == false )
    {
        if ( eeprom_read( EEADDR_CK_OPS, 2, (uint8*)&cksum_op, false ) != 2 )
            return -1;
        while ( eeprom_is_operation_finished() == false );

        buffer = (uint8*)&core.nv.op;
        cksum = 0xABCD;
        for ( i=0; i<sizeof(struct SCoreOperation); i++ )
            cksum = cksum + ( (uint16)(buffer[i] << 8) - (uint16)(~buffer[i]) ) + 1;
        if ( cksum != cksum_op )
            goto _error_exit;
  }

    eeprom_disable();
    return 0;

_error_exit:
    // eeprom data corrupted or not initialized
    while ( eeprom_is_operation_finished() == false );

    core_setup_reset( true );
    eeprom_disable();
    return -1;
}


void core_nvfast_save_struct(void)
{
    BKP_WriteBackupRegister( BKP_DR2, core.nf.status.val );
    BKP_WriteBackupRegister( BKP_DR3, core.nf.next_schedule & 0xffff );
    BKP_WriteBackupRegister( BKP_DR4, (core.nf.next_schedule >> 8) & 0xffff );

    // save the measured temperature and dewpoint because we need them independently from each-other after start-up for calculations
    BKP_WriteBackupRegister( BKP_DR5, core.measure.measured.temperature );
    BKP_WriteBackupRegister( BKP_DR6, core.measure.measured.rh );


    if ( core.nv.dirty )
    {
        core.nv.dirty = false;
        core_setup_save();
    }
}



void core_op_realtime_sensor_select( enum ESensorSelect sensor )
{
    if ( sensor == ss_none )
        core.vstatus.int_op.f.sens_real_time = 0;
    switch ( sensor )
    {
        case ss_thermo:
            if ( (core.nv.op.sched.sch_thermo > (RTCclock + CORE_SCHED_TEMP_REALTIME)) ||
                 (core.nv.op.sched.sch_thermo < RTCclock) )
                core.nv.op.sched.sch_thermo = RTCclock;        // will be rescheduled by the core_loop() at first RTC tick
            break;
        case ss_rh:
            if ( (core.nv.op.sched.sch_hygro > (RTCclock + CORE_SCHED_RH_REALTIME)) ||
                 (core.nv.op.sched.sch_hygro < RTCclock) )
                core.nv.op.sched.sch_hygro = RTCclock;        // will be rescheduled by the core_loop() at first RTC tick
            break;
        case ss_pressure:
            if ( (core.nv.op.sched.sch_press > (RTCclock + CORE_SCHED_PRESS_REALTIME)) ||
                 (core.nv.op.sched.sch_press < RTCclock) )
                core.nv.op.sched.sch_press = RTCclock;        // will be rescheduled by the core_loop() at first RTC tick
            break;
    }
    core.vstatus.int_op.f.sens_real_time = sensor;
    local_check_first_scheduled_op();
}

void core_op_monitoring_switch( bool enable )
{
    if ( enable == false )
    {
        // disabling monitoring - clean up the 
        core.nv.op.op_flags.b.op_monitoring = 0;
        internal_clear_monitoring();
    }
    else if ( core.nv.op.op_flags.b.op_monitoring == 0 )
    {
        internal_clear_monitoring();
        core.nv.op.sens_rd.moni.sch_moni_temp  = RTCclock + 2 * internal_time_unit_2_seconds( core.nv.setup.tim_tend_temp );
        core.nv.op.sens_rd.moni.sch_moni_hygro = RTCclock + 2 * internal_time_unit_2_seconds( core.nv.setup.tim_tend_hygro );
        core.nv.op.sens_rd.moni.sch_moni_press = RTCclock + 2 * internal_time_unit_2_seconds( core.nv.setup.tim_tend_press );
        core.nv.op.sens_rd.moni.clk_last_day = RTCclock / DAY_TICKS;
        core.nv.op.sens_rd.moni.clk_last_week = (RTCclock + DAY_TICKS) / WEEK_TICKS;
        core.nv.op.op_flags.b.op_monitoring = 1;
    }
}


void core_op_monitoring_rate( enum ESensorSelect sensor, enum EUpdateTimings timing )
{
    if ( sensor == ss_none )
        return;
    switch ( sensor )
    {
        case ss_thermo: 
            if ( core.nv.setup.tim_tend_temp != timing )
            {
                core.nv.setup.tim_tend_temp = timing;
                if ( core.nv.op.op_flags.b.op_monitoring )
                {
                    core.nv.op.sens_rd.moni.sch_moni_temp = RTCclock + 2 * internal_time_unit_2_seconds( timing );
                    core.nv.op.sens_rd.moni.avg_ctr_temp = 0;
                    core.nv.op.sens_rd.moni.avg_sum_temp = 0;
                    local_tendency_restart( CORE_MMP_TEMP );
                }
            }
            break;
        case ss_rh: 
            if ( core.nv.setup.tim_tend_hygro != timing )
            {
                core.nv.setup.tim_tend_hygro = timing;
                if ( core.nv.op.op_flags.b.op_monitoring )
                {
                    core.nv.op.sens_rd.moni.sch_moni_hygro = RTCclock + 2 * internal_time_unit_2_seconds( timing );
                    core.nv.op.sens_rd.moni.avg_ctr_hygro = 0;
                    core.nv.op.sens_rd.moni.avg_sum_rh = 0;
                    core.nv.op.sens_rd.moni.avg_sum_abshum = 0;
                    local_tendency_restart( CORE_MMP_RH );
                    local_tendency_restart( CORE_MMP_ABSH );
                }
            }
            break;
        case ss_pressure: 
            if ( core.nv.setup.tim_tend_press != timing )
            {
                core.nv.setup.tim_tend_press = timing;
                if ( core.nv.op.op_flags.b.op_monitoring )
                {
                    core.nv.op.sens_rd.moni.sch_moni_press = RTCclock + 2 * internal_time_unit_2_seconds( timing );
                    core.nv.op.sens_rd.moni.avg_ctr_press = 0;
                    core.nv.op.sens_rd.moni.avg_sum_press = 0;
                    local_tendency_restart( CORE_MMP_PRESS );
                }
            }
            break;
    }
}

void core_op_monitoring_reset_minmax( enum ESensorSelect sensor, int mmset )
{
    if ( mmset > mms_week_crt )
    {
        // for last day / last week we set up <uninitialized>
        switch (sensor)
        {
            case ss_thermo:
                core.nv.op.sens_rd.minmax[CORE_MMP_TEMP].max[mmset] = 0;
                core.nv.op.sens_rd.minmax[CORE_MMP_TEMP].min[mmset] = 0;
                break;
            case ss_rh:
                core.nv.op.sens_rd.minmax[CORE_MMP_RH].max[mmset] = 0;
                core.nv.op.sens_rd.minmax[CORE_MMP_RH].min[mmset] = 0;
                core.nv.op.sens_rd.minmax[CORE_MMP_ABSH].max[mmset] = 0;
                core.nv.op.sens_rd.minmax[CORE_MMP_ABSH].min[mmset] = 0;
                break;
            case ss_pressure:
                core.nv.op.sens_rd.minmax[CORE_MMP_PRESS].max[mmset] = 0;
                core.nv.op.sens_rd.minmax[CORE_MMP_PRESS].min[mmset] = 0;
                break;
        }
    }
    else
    {
        // for current min/max sets we are using the current measured values as start point
        switch (sensor)
        {
            case ss_thermo:
                core.nv.op.sens_rd.minmax[CORE_MMP_TEMP].max[mmset] = core.measure.measured.temperature;
                core.nv.op.sens_rd.minmax[CORE_MMP_TEMP].min[mmset] = core.measure.measured.temperature;
                break;
            case ss_rh:
                core.nv.op.sens_rd.minmax[CORE_MMP_ABSH].max[mmset] = core.measure.measured.absh;
                core.nv.op.sens_rd.minmax[CORE_MMP_ABSH].min[mmset] = core.measure.measured.absh;
                core.nv.op.sens_rd.minmax[CORE_MMP_RH].max[mmset] = core.measure.measured.rh;
                core.nv.op.sens_rd.minmax[CORE_MMP_RH].min[mmset] = core.measure.measured.rh;
                break;
            case ss_pressure:
                core.nv.op.sens_rd.minmax[CORE_MMP_PRESS].max[mmset] = core.measure.measured.pressure;
                core.nv.op.sens_rd.minmax[CORE_MMP_PRESS].min[mmset] = core.measure.measured.pressure;
                break;
        }
    }
}


//-------------------------------------------------
//     main core interface
//-------------------------------------------------

int core_init( struct SCore **instance )
{
    uint32 wur;
    bool charge = false;

    memset(&core, 0, sizeof(core));
    if ( instance )
        *instance = &core;

    // get the RTC clock counter in a local copy
    RTCclock = RTC_GetCounter();
    RTCctr = RTCclock;

    // check the wake-up reason
    wur = HW_GetWakeUpReason();
    if ( wur & WUR_FIRST )
    {
        // first start-up - means that clock and work status is uninitialized
        local_initialize_core_operation();

        // init some default stuff for first start-up
        core.vstatus.int_op.f.first_pwrup = 1;
        core.nf.status.b.first_start = 1;

        core.nv.op.params.press_msl = 101325 - 50000;   // 1013.25 hpa reference pressure
    }
    else
    {
        // consequent start-up - check the reason
        local_nvfast_load_struct();
    }

    if ( wur & (WUR_USR | WUR_FIRST) )
        local_update_battery();

    charge = HW_Charge_Detect();

    // mark the first loop after init
    core.vstatus.int_op.f.first_run = 1;

    // activate power button long press detection in the UI module
    if ( wur & WUR_USR )
        core.vstatus.ui_cmd = CORE_UISTATE_START_LONGPRESS;       // order pwr/mode button long press detection from UI
    if ( core.nf.status.b.first_start )
        core.vstatus.ui_cmd |= CORE_UISTATE_SET_DATETIME;         // order date/time setup
    if ( charge )
        core.vstatus.ui_cmd |= CORE_UISTATE_CHARGING;             // indicate charging

    // get the current RTC counter and check if operation is scheduled
    if ( (core.nf.next_schedule <= RTCclock) || charge || (wur & (WUR_USR | WUR_FIRST)) )
    {
        // schedule was made on an operation - we need to check out the core.nv with setup and saved status
        // we will fetch nonvolatile data also when charger is connected because it will not go back in power save mode
        if ( eeprom_init() )
            goto _err_exit;
        eeprom_enable(false);
        core.vstatus.int_op.f.nv_state = CORE_NVSTATE_PWR_RUNUP;    // FRAM chip needs time to start-up - wait for it 1ms
        core.vstatus.int_op.f.core_bsy = 1;                         // mark core busy
        if ( RTCclock >= core.nf.next_schedule )
            core.vstatus.int_op.f.sched = 1;                        // mark the scheduled event for chek-up
    }

/*
    BeepSetFreq( core.nv.setup.beep_low, core.nv.setup.beep_hi );
*/
    return 0;
_err_exit:
    return -1;
}


void core_poll( struct SEventStruct *evmask )
{
    // check for RTC tick
    if ( evmask->timer_tick_05sec )
    {
        RTCclock = RTCctr;
        if (core.nf.next_schedule <= RTCclock)
        {
            core.vstatus.int_op.f.sched = 1;            // sheduled event
            core.vstatus.int_op.f.core_bsy = 1;         // core busy
        }

        local_poll_aux_operations();
    }

    // poll the sensor module
    if ( core.vstatus.int_op.f.nv_initted )
        Sensor_Poll( evmask->timer_tick_system );
 
    // if core module is busy - do the operations
    while ( core.vstatus.int_op.f.core_bsy )            // seve all the busy events
    {
        if ( core.vstatus.int_op.f.nv_initted )
        {
            // nonvolatile memory initted - continue operations
            if ( core.vstatus.int_op.f.sched )
            {
                // was an operation schedule - check it
                local_check_sensor_read_schedules();
                if ( core.nv.op.op_flags.b.op_monitoring )
                {
                    local_push_minmax_set_if_needed();
                }
                if (core.nf.next_schedule > RTCclock)
                {
                    core.vstatus.int_op.f.sched = 0;
                    break;                              // break the busy loop 
                }
            }

            if ( core.vstatus.int_op.f.sens_read )
            {
                uint32 result;

                result = Sensor_Is_Failed();
                if ( result )
                {
                    //TODO: in case of sensor failure
                }

                result = Sensor_Is_Ready();
                if ( result )
                {
                    if ( result & SENSOR_TEMP )
                    {
                        local_process_temp_sensor_result( Sensor_Get_Value(SENSOR_TEMP) );
                        core.vstatus.int_op.f.sens_read &= ~SENSOR_TEMP;
                    }
                    if ( result & SENSOR_RH )
                    {
                        local_process_hygro_sensor_result( Sensor_Get_Value(SENSOR_RH) );
                        core.vstatus.int_op.f.sens_read &= ~SENSOR_RH;
                    }
                    if ( result & SENSOR_PRESS )
                    {
                        local_process_pressure_sensor_result( Sensor_Get_Value(SENSOR_PRESS) );
                        core.vstatus.int_op.f.sens_read &= ~SENSOR_PRESS;
                    }

                    if ( core.vstatus.int_op.f.sens_read == 0 )
                    {
                        core.vstatus.int_op.f.core_bsy = 0;
                    }
                }
                break;                                  // break the busy loop
            }
        }
        else
        {
            // if nonvolatile memory in not initted - do nothing else - must init it first
            if ( evmask->timer_tick_system && (core.vstatus.int_op.f.nv_state == CORE_NVSTATE_PWR_RUNUP) )
            {
                if ( core_setup_load( core.vstatus.int_op.f.first_pwrup ) )
                    core.vstatus.ui_cmd |= CORE_UISTATE_EECORRUPTED;

                core.vstatus.int_op.f.first_pwrup = 0;
                core.nv.dirty = true;

                eeprom_deepsleep();                         // put EEprom in deep sleep
                core.vstatus.int_op.f.nv_state = CORE_NVSTATE_OK_IDLE;
                core.vstatus.int_op.f.nv_initted = 1;

                // init the sensor module
                Sensor_Init();

                if ( core.vstatus.int_op.f.sched == 0 )
                    core.vstatus.int_op.f.core_bsy = 0;

                // loop back to execute the first busy operation (if any)
            }
            else
                break;
        }
    }

    return;
}


void core_pwr_setup_alarm( enum EPowerMode pwr_mode )
{
    if ( core.nf.next_schedule > (RTCclock + CORE_SCHED_BATTERY_CHRG) )
        HW_SetRTC_NextAlarm( RTCclock + CORE_SCHED_BATTERY_CHRG - 1 );      // set alarm for battery charge status read
    else
        HW_SetRTC_NextAlarm( core.nf.next_schedule - 1 );                   // set alarm for the next scheduled operation
}



uint32 core_pwr_getstate(void)
{
    uint32 pwr = SYSSTAT_CORE_STOPPED;

    if ( core.vstatus.int_op.f.nv_initted )
        pwr |= Sensor_GetPwrStatus();

    if ( core.vstatus.int_op.f.core_bsy )
    {
        if ( core.vstatus.int_op.f.nv_state == CORE_NVSTATE_PWR_RUNUP )       // in uninitted state it waits for 1ms NVram start-up
            return (pwr | SYSSTAT_CORE_RUN_FULL);
    }

    return pwr;
}








