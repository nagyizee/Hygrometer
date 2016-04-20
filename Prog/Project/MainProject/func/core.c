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
 *                                      - if monitoring task and recording task - will power down with alarm set.
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
 *              - recording mode: - UI will refresh the readings of the current sensor in real time,
 *                                the other sensors are read at monitoring period (2sec. TBD) in case of high rate or low rate w averaging, values are
 *                                averaged to the recording rate, and recording.
 *                                for low rate w/o averaging - sensor read is done at low rate.
 *
 *
 *
 *
 *
 **/

#include <stdio.h>
#include <string.h>
#include <math.h>
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


// Workbuffer usage:
// 1. For Graph displaying: 
//      buffer is partitionated:
//          - RAWDISP:  - raw data for display. Display contains 100points on horizontal
//                      - there are 3 sections:         [TEMP] [RH] [PRESS]
//                      - each section has 3 sets:      [MIN][MAX][AVG]             ->  RAWDISP: [  [Tm][TM][Ta] [Hm][HM][Ha] [Pm][PM][Pa] ]
//                      - memory size = 3 sections * 3 sets * 100 points * 2bytes = 1800bytes -> 0x708 
//                      we will use 4bit aligned ending (16 byte packs): 0x710
//          - OFFS_FLIP1:
//          - OFFS_FLIP2:   - 256 bytes of flip buffers. One is read from NVram, the other is processed to fill the RAWDISP
// 
// 2. For Serial data transfer:
//          - OFFS_FLIP1:
//          - OFFS_FLIP2:   - 256 bytes of flip buffers. One is read from NVram, the other is transmitted on UART


#define WB_DISPPOINT        100
#define WB_FLIPB_SIZE       0x200

#define WB_OFFS_FLIP1       0x000                               // 512 bytes transfer buffer F1
#define WB_OFFS_FLIP2       WB_FLIPB_SIZE                       // 512 bytes transfer buffer F2
#define WB_OFFS_RAWDISP     (WB_OFFS_FLIP2 + WB_FLIPB_SIZE)     // from 0x200 -> 0x910  - 710 bytes display graph memory: 100 samples * 2 bps * 3 (low/high/avg) * 3 params (T/RH/P)

#define WB_OFFS_TEMP        (WB_OFFS_RAWDISP)
#define WB_OFFS_TEMP_MIN    (WB_OFFS_TEMP)
#define WB_OFFS_TEMP_MAX    (WB_OFFS_TEMP_MIN + 200)
#define WB_OFFS_TEMP_AVG    (WB_OFFS_TEMP_MAX + 200)

#define WB_OFFS_RH          (WB_OFFS_TEMP_AVG + 200)
#define WB_OFFS_RH_MIN      (WB_OFFS_RH)
#define WB_OFFS_RH_MAX      (WB_OFFS_RH_MIN + 200)
#define WB_OFFS_RH_AVG      (WB_OFFS_RH_MAX + 200)

#define WB_OFFS_P           (WB_OFFS_RH_AVG + 200)
#define WB_OFFS_P_MIN       (WB_OFFS_P)
#define WB_OFFS_P_MAX       (WB_OFFS_P_MIN + 200)
#define WB_OFFS_P_AVG       (WB_OFFS_P_MAX + 200)

#define WB_SIZE             (WB_OFFS_P_AVG + 200)               // 


static uint8    workbuff[ WB_SIZE ];


#define EEADDR_SETUP    0x00
#define EEADDR_OPS      (EEADDR_SETUP + sizeof(struct SCoreSetup))
#define EEADDR_RECORD   (EEADDR_OPS + sizeof(struct SCoreOperation))
#define EEADDR_CK_SETUP (EEADDR_RECORD + sizeof(struct SCoreNonVolatileRec))
#define EEADDR_CK_OPS   (EEADDR_CK_SETUP + 2)
#define EEADDR_CK_REC   (EEADDR_CK_OPS + 2)
#define EEADDR_STORAGE  CORE_RECMEM_PAGESIZE       // leave the first page for setup - the rest is for recording

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

void internal_DBG_dump_values()
{
    #ifdef ON_QT_PLATFORM
    if ( core.vstatus.int_op.f.nv_initted )
        HW_DBG_DUMP( &core );
    #endif
}

void internal_DBG_simu_1_cycle()
{
    #ifdef ON_QT_PLATFORM
        HW_DBG_SIMUSKIP();
    #endif
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


static void local_recording_pushdata( uint32 task_idx, enum ESensorSelect sensor, uint32 value )
{
    // value is in 16bit format ( 16fp9+40* for temp, 16fp8 0-100% for RH, 16bit Pascals for pressure )
    struct SRecTaskInternals *pfunc;
    struct SRecTaskInstance  task;
    uint32 smask = 1 << (sensor - 1);

    task = core.nvrec.task[task_idx];
    pfunc = &core.nvrec.func[task_idx];

    if ( (task.task_elems & smask)== 0 )        // sensor not part of this task - exit, do not average for this task
        return;

    // sum for average
    pfunc->avg_sum[sensor-1] += value;
    pfunc->avg_cnt[sensor-1] ++;

    // if time passed through the scheduled one - record the item
    if ( RTCclock >= pfunc->shedule )
    {
        if ( (smask & pfunc->elem_mask) == 0 )      // parameter not recorded to elem buffer yet - can be recorded
        {
            // calculate the average and convert it to 12bit
            uint32 rval = (pfunc->avg_sum[sensor-1] / pfunc->avg_cnt[sensor-1]) >> 4;
            pfunc->avg_sum[sensor-1] = 0;
            pfunc->avg_cnt[sensor-1] = 0;
            pfunc->elem_mask |= smask;              // mark that item is registered in the recording element

            // check if the set is collected
            if ( pfunc->elem_mask == task.task_elems )
            {
                pfunc->shedule += 2 * core_utils_timeunit2seconds( task.sample_rate );      // reschedule
                pfunc->elem_mask = CORE_ELEM_TO_RECORD;                                     // mark for non-volatile writing4
                pfunc->last_timestamp = RTCclock;
                core.vstatus.int_op.f.core_bsy = 1;
                core.vstatus.int_op.f.op_recsave = 1;
            }

            // record the item
            if( (task.task_elems == rtt_t) ||
                (task.task_elems == rtt_h) || 
                (task.task_elems == rtt_p) )    
            {
                // in this case sensor is for sure the one to be recorded first
                if ( pfunc->elem_shift )
                {
                    pfunc->element[0] |= rval >> 8;         // [oooo xxxx]              -- leaves the old MSB at start
                    pfunc->element[1] = rval & 0xff;        //            [xxxx xxxx]
                }
                else
                {
                    pfunc->element[0] = rval >> 4;          // [xxxx xxxx]
                    pfunc->element[1] = (rval << 4) & 0xff; //            [xxxx 0000]   -- clears the terminating LSB
                }
            } 
            else if ( ( (sensor == ss_thermo) &&                                                // thermo sensor can be at start only in 2 cases: TH, TP
                        ((task.task_elems == rtt_th) || (task.task_elems == rtt_tp)) ) ||       
                      ( (sensor == ss_rh) &&                                                    // hygro sensor can be at start only in 1 case: HP
                        (task.task_elems == rtt_hp) ) )
            {
                pfunc->element[0] = rval >> 4;              // [tttt tttt]
                pfunc->element[1] |= (rval << 4) & 0xff;    //            [tttt oooo]  
            }
            else if ( ( (sensor == ss_rh) &&                                                    // hygro sensor can be at end only in 1 case: TH
                        (task.task_elems == rtt_th)  ) || 
                      ( (sensor == ss_pressure) &&                                              // pressure sensor can be at end only in 2 cases: TP, HP
                        ((task.task_elems == rtt_tp) || (task.task_elems == rtt_hp)) ) )
            {
                pfunc->element[1] |= rval >> 8;             //            [oooo hhhh]
                pfunc->element[2] = rval & 0xff;            //                       [hhhh hhhh]
            }
            else 
            {
                // only THP can get here
                if ( pfunc->elem_shift )
                {
                    if ( sensor == ss_thermo )
                    {
                        pfunc->element[0] |= rval >> 8;         // [oooo tttt]
                        pfunc->element[1] = rval & 0xff;        //            [tttt tttt]
                    }
                    else if ( sensor == ss_rh )
                    {
                        pfunc->element[2] = rval >> 4;          //                      [hhhh hhhh]
                        pfunc->element[3] |= (rval << 4) & 0xff;//                                 [hhhh oooo]  
                    }
                    else
                    {
                        pfunc->element[3] |= rval >> 8;         //                                 [oooo pppp]
                        pfunc->element[4] = rval & 0xff;        //                                            [pppp pppp]
                    }
                }
                else
                {
                    if ( sensor == ss_thermo )
                    {
                        pfunc->element[0] = rval >> 4;          // [tttt tttt]
                        pfunc->element[1] |= (rval << 4) & 0xff;//            [tttt oooo]  
                    }
                    else if ( sensor == ss_rh )
                    {
                        pfunc->element[1] |= rval >> 8;         //            [oooo hhhh]
                        pfunc->element[2] = rval & 0xff;        //                       [hhhh hhhh]
                    }
                    else
                    {
                        pfunc->element[3] = rval >> 4;          //                                  [pppp pppp]
                        pfunc->element[4] = (rval << 4) & 0xff; //                                             [pppp 0000]  
                    }
                }
            }
        }
    }
}


static uint32 internal_recording_get_address( uint32 elem_ptr, enum ERecordingTaskType elem_type, bool *shift )
{
    uint32 ival;

    switch ( elem_type )
    {
        case rtt_t:
        case rtt_h:
        case rtt_p:
            ival = 3;
            break;
        case rtt_th:
        case rtt_tp:
        case rtt_hp:
            ival = 6;
            break;
        default:
            ival = 9;
            break;
    }

    ival *= elem_ptr;
    if ( shift )
    {
        if ( ival & 0x01 )
            *shift = true;
        else
            *shift = false;
    }
    return (ival >> 1);
}

// converts element pointer ( task read or write pointers ) and elem type to physical address
//                                                            0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15   16   17   18
// for rtt_t, rtt_h, rtt_p:         [e0][e1][e2][e3][e4]:   [0 0][0 1][1 1][2 2][2 3][3 3][4 4][4 X]
//              0 -> 0,     1 -> 1 + shift,     2 -> 3,     3 -> 4 + shift,     4 -> 6
// 
// for rtt_th, rtt_tp, rtt_hp:      [e0][e1][e2][e3][e4]:   [0 0][0 0][0 0][1 1][1 1][1 1][2 2][2 2][2 2][3 3][3 3][3 3][4 4][4 4][4 4]
//              0 -> 0,     1 -> 3,     2 -> 6,     3 -> 9,     4 -> 12
// 
// for rtt_thp:                     [e0][e1][e2][e3][e4]:   [0 0][0 0][0 0][0 0][0 1][1 1][1 1][1 1][1 1][2 2][2 2][2 2][2 2][2 3][3 3][3 3][3 3][3 3][4 4][4 4][4 4][4 4][4 X]
//              0 -> 0,     1 -> 4 + shift,     2 -> 9,     3 -> 13 + shift,        4 -> 18

static void local_recording_savedata(void)
{
    int i;

    // check if NVRAM is free
    if ( eeprom_is_operation_finished() == false )
        return;

    // enable and 
    eeprom_enable( true );
    if ( eeprom_is_operation_finished() == false )
        return;

    for (i=0; i<STORAGE_RECTASK; i++)
    {
        struct SRecTaskInternals *pfunc;

        pfunc = &core.nvrec.func[i];

        if ( pfunc->elem_mask == CORE_ELEM_TO_RECORD )
        {
            // task has an element to be saved
            uint32 ee_addr;
            uint32 ee_len;

            // calculate the nvram address and data length
            switch ( core.nvrec.task[i].task_elems )
            {
                case rtt_t:
                case rtt_h:
                case rtt_p:
                    ee_addr = (3 * (uint32)pfunc->w) >> 1;
                    ee_len = 2;
                    break;
                case rtt_th:
                case rtt_tp:
                case rtt_hp:
                    ee_addr = (6 * (uint32)pfunc->w) >> 1;
                    ee_len = 3;
                    break;
                default:
                    ee_addr = (9 * (uint32)pfunc->w) >> 1;
                    ee_len = 5;
                    break;
            }
            
            // write to the storage
            eeprom_write( ee_addr + EEADDR_STORAGE + (uint32)core.nvrec.task[i].mempage * CORE_RECMEM_PAGESIZE, pfunc->element, ee_len, false );
            while ( eeprom_is_operation_finished() == false );

            // prepare task for the next aquisition
            if ( (ee_len != 3) &&                       // If element is a non byte complete type (1 or 3 items -> 1.5 or 4.5 bytes)
                 (pfunc->elem_shift == 0) )             // and no shift was made at the prew. operation                     
            {
                // save the last half byte from the prew. write
                if ( ee_len == 2 )
                    pfunc->element[0] = pfunc->element[1];          // for 1.5 bytes  
                else
                    pfunc->element[0] = pfunc->element[4];          // for 4.5 bytes
                // mark the shift
                pfunc->elem_shift = 1;
                pfunc->element[1] = 0;
                pfunc->element[2] = 0;
                pfunc->element[3] = 0;
                pfunc->element[4] = 0;
            }
            else
            {
                memset( pfunc->element, 0, 5 );
                pfunc->elem_shift = 0;
            }
            pfunc->elem_mask = 0;           // clear the elem mask

            // advance the write pointer
            pfunc->w++;
            if ( pfunc->w == pfunc->wrap )
                pfunc->w = 0;
                
            // delete the oldest record if needed
            if ( pfunc->w == pfunc->r )
            {
                pfunc->r++;
                if ( pfunc->r == pfunc->wrap )
                    pfunc->r = 0;
            }
            else
                pfunc->c++;

            core.nvrec.dirty = true;
        }
    }

    core.vstatus.int_op.f.op_recsave = 0;      // everythign is saved
}

static void local_recording_all_pushdata( enum ESensorSelect sensor, uint32 value )
{
    int i;
    for ( i=0; i<STORAGE_RECTASK; i++ )
    {
        if ( core.nvrec.running & (1<<i) )
            local_recording_pushdata( i, sensor, value );
    }
}


static void local_recording_task_reset( uint32 task_idx )
{
    uint32 wrap;
    memset( &core.nvrec.func[task_idx], 0, sizeof(struct SRecTaskInternals) );

    // wrap arround pointer is the max. elem. count of the current task
    wrap = core_op_recording_get_total_samplenr( core.nvrec.task[task_idx].size * CORE_RECMEM_PAGESIZE, core.nvrec.task[task_idx].task_elems );

    core.nvrec.func[task_idx].shedule = CORE_SCHED_NONE;
    core.nvrec.func[task_idx].wrap = wrap;
}

static void local_recording_task_stop( uint32 task_idx )
{
    // just disable the clock
    core.nvrec.func[task_idx].shedule = CORE_SCHED_NONE;
}

static void local_recording_task_start( uint32 task_idx )
{
    core.nvrec.func[task_idx].shedule = RTCclock + 2 * core_utils_timeunit2seconds( core.nvrec.task[task_idx].sample_rate );
}


static inline void internal_recording_read_process_simple( uint8 *wbuff, uint32 smp_proc, bool shifted )
{
    // Do not try to optimize this - it should work as quick as possible
    int i;
    uint32 disp_ptr;

    disp_ptr = core.readout.raw_ptr << 1;

    switch ( core.readout.taks_elem )
    {
        case rtt_t:
        case rtt_p:
        case rtt_h:
            {
                uint16 *rawbuff;
                uint32 val;

                if ( core.readout.taks_elem == rtt_t )
                    rawbuff = (uint16*)(workbuff + WB_OFFS_TEMP_AVG + disp_ptr);
                else if ( core.readout.taks_elem == rtt_h )
                    rawbuff = (uint16*)(workbuff + WB_OFFS_RH_AVG + disp_ptr);
                else
                    rawbuff = (uint16*)(workbuff + WB_OFFS_P_AVG + disp_ptr);

                for ( i=0; i<smp_proc; i++ )
                {
                    // get the 12bit value
                    if ( shifted )
                    {   
                        val = ( ((uint32)(wbuff[0]) << 8) | wbuff[1] ) & 0x0fff;    // [xxxx tttt][tttt tttt]
                        shifted = false;
                        wbuff += 2;
                    }
                    else
                    {
                        val = ( ((uint32)(wbuff[0]) << 4) | (wbuff[1] >> 4) );      // [tttt tttt][tttt xxxx]
                        shifted = true;
                        wbuff += 1;
                    }
                    // convert to 16bit and save
                    *rawbuff = (val << 4);
                    rawbuff++;
                }
            }
            break;
        case rtt_th:
        case rtt_tp:
        case rtt_hp:
            {
                uint16 *rawbuff1;
                uint16 *rawbuff2;
                uint32 val1;
                uint32 val2;

                if ( core.readout.taks_elem == rtt_th )
                {
                    rawbuff1 = (uint16*)(workbuff + WB_OFFS_TEMP_AVG + disp_ptr);
                    rawbuff2 = (uint16*)(workbuff + WB_OFFS_RH_AVG + disp_ptr);
                }
                else if ( core.readout.taks_elem == rtt_tp )
                {
                    rawbuff1 = (uint16*)(workbuff + WB_OFFS_TEMP_AVG + disp_ptr);
                    rawbuff2 = (uint16*)(workbuff + WB_OFFS_P_AVG + disp_ptr);
                }
                else
                {
                    rawbuff1 = (uint16*)(workbuff + WB_OFFS_RH_AVG + disp_ptr);
                    rawbuff2 = (uint16*)(workbuff + WB_OFFS_P_AVG + disp_ptr);
                }

                for ( i=0; i<smp_proc; i++ )
                {
                    // get the 12bit value - no shift in this case
                    val1 = ( ((uint32)(wbuff[0]) << 4) | (wbuff[1] >> 4) );     // [tttt tttt][tttt hhhh][hhhh hhhh]
                    val2 = ( ((uint32)(wbuff[1]) << 8) | wbuff[2] ) & 0x0fff;
                    wbuff += 3;

                    // convert to 16bit and save
                    *rawbuff1 = (val1 << 4);
                    *rawbuff2 = (val2 << 4);
                    rawbuff1++;
                    rawbuff2++;
                }
            }
            break;
        default:
            {
                uint16 *rawbuff1 = (uint16*)(workbuff + WB_OFFS_TEMP_AVG + disp_ptr);
                uint16 *rawbuff2 = (uint16*)(workbuff + WB_OFFS_RH_AVG + disp_ptr);
                uint16 *rawbuff3 = (uint16*)(workbuff + WB_OFFS_P_AVG + disp_ptr);
                uint32 val1;
                uint32 val2;
                uint32 val3;
                for ( i=0; i<smp_proc; i++ )
                {
                    // get the 12bit value
                    if ( shifted )
                    {                                                               //      0          1          2          3          4
                        val1 = ( ((uint32)(wbuff[0]) << 8) | wbuff[1] ) & 0x0fff;   // [xxxx tttt][tttt tttt][hhhh hhhh][hhhh pppp][pppp pppp]
                        val2 = ( ((uint32)(wbuff[2]) << 4) | (wbuff[3] >> 4) );
                        val3 = ( ((uint32)(wbuff[3]) << 8) | wbuff[4] ) & 0x0fff;
                        shifted = false;
                        wbuff += 5;
                    }
                    else
                    {
                        val1 = ( ((uint32)(wbuff[0]) << 4) | (wbuff[1] >> 4) );     // [tttt tttt][tttt hhhh][hhhh hhhh][pppp pppp][pppp xxxx]
                        val2 = ( ((uint32)(wbuff[1]) << 8) | wbuff[2] ) & 0x0fff;
                        val3 = ( ((uint32)(wbuff[3]) << 4) | (wbuff[4] >> 4) ); 
                        shifted = true;
                        wbuff += 4;
                    }
                    // convert to 16bit and save
                    *rawbuff1 = (val1 << 4);
                    *rawbuff2 = (val2 << 4);
                    *rawbuff3 = (val3 << 4);
                    rawbuff1++;
                    rawbuff2++;
                    rawbuff3++;
                }
            }
            break;
    }
    core.readout.raw_ptr += smp_proc;
}

static inline void internal_recording_read_process_minmaxavg( uint8 *wbuff, uint32 smp_proc, bool shifted, bool last )
{
    int i;
    uint32 disp_ptr;

    register uint32 dispctr  = core.readout.dispctr;
    register uint32 dispprev = core.readout.dispprev;
    register uint32 dispstep = core.readout.dispstep;

    disp_ptr = (dispctr >> 15) & (~0x01);       // use the disp counter's integer part, but x2

    switch ( core.readout.taks_elem )
    {
        case rtt_t:
        case rtt_p:
        case rtt_h:
            {
                uint16 *rawbuff;
                register uint32 val;

                if ( core.readout.taks_elem == rtt_t )
                    rawbuff = (uint16*)(workbuff + WB_OFFS_TEMP + disp_ptr);
                else if ( core.readout.taks_elem == rtt_h )
                    rawbuff = (uint16*)(workbuff + WB_OFFS_RH + disp_ptr);
                else
                    rawbuff = (uint16*)(workbuff + WB_OFFS_P + disp_ptr);

                for ( i=0; i<smp_proc; i++ )
                {
                    // get the 12bit value
                    if ( shifted )
                    {   
                        val = (( ((uint32)(wbuff[0]) << 8) | wbuff[1] ) & 0x0fff) << 4;    // [xxxx tttt][tttt tttt]
                        shifted = false;
                        wbuff += 2;
                    }
                    else
                    {
                        val = ( ((uint32)(wbuff[0]) << 4) | (wbuff[1] >> 4) ) << 4;      // [tttt tttt][tttt xxxx]
                        shifted = true;
                        wbuff += 1;
                    }

                    // check min/max and sum for average
                    if ( core.readout.v1min > val )
                        core.readout.v1min = val;
                    if ( core.readout.v1max < val )
                        core.readout.v1max = val;
                    core.readout.v1ctr++;
                    core.readout.v1sum += val;

                    // see if should proceed with the next point
                    dispctr += dispstep;
                    if ( (dispctr >> 16) != dispprev)
                    {
                        dispprev = (dispctr >> 16);

                        val = core.readout.v1sum / core.readout.v1ctr;
                        *rawbuff        = core.readout.v1min;
                        *(rawbuff+100)  = core.readout.v1max;
                        *(rawbuff+200)  = val;

                        core.readout.v1min = 0xffff;
                        core.readout.v1max = 0;
                        core.readout.v1sum = 0;
                        core.readout.v1ctr = 0;

                        rawbuff++;
                    }
                }
            }
            break;
        case rtt_th:
        case rtt_tp:
        case rtt_hp:
            {
                uint16 *rawbuff1;
                uint16 *rawbuff2;
                register uint32 val1;
                register uint32 val2;

                if ( core.readout.taks_elem == rtt_th )
                {
                    rawbuff1 = (uint16*)(workbuff + WB_OFFS_TEMP + disp_ptr);
                    rawbuff2 = (uint16*)(workbuff + WB_OFFS_RH + disp_ptr);
                }
                else if ( core.readout.taks_elem == rtt_tp )
                {
                    rawbuff1 = (uint16*)(workbuff + WB_OFFS_TEMP + disp_ptr);
                    rawbuff2 = (uint16*)(workbuff + WB_OFFS_P + disp_ptr);
                }
                else
                {
                    rawbuff1 = (uint16*)(workbuff + WB_OFFS_RH + disp_ptr);
                    rawbuff2 = (uint16*)(workbuff + WB_OFFS_P + disp_ptr);
                }

                for ( i=0; i<smp_proc; i++ )
                {
                    // get the 12bit value - no shift in this case
                    val1 = ( ((uint32)(wbuff[0]) << 4) | (wbuff[1] >> 4) ) << 4;        // [tttt tttt][tttt hhhh][hhhh hhhh]
                    val2 = ( ( ((uint32)(wbuff[1]) << 8) | wbuff[2] ) & 0x0fff) << 4;
                    wbuff += 3;

                    // check min/max and sum for average
                    if ( core.readout.v1min > val1 )
                        core.readout.v1min = val1;
                    if ( core.readout.v1max < val1 )
                        core.readout.v1max = val1;
                    core.readout.v1ctr++;
                    core.readout.v1sum += val1;

                    if ( core.readout.v2min > val2 )
                        core.readout.v2min = val2;
                    if ( core.readout.v2max < val2 )
                        core.readout.v2max = val2;
                    core.readout.v2ctr++;
                    core.readout.v2sum += val2;

                    // see if should proceed with the next point
                    dispctr += dispstep;
                    if ( (dispctr >> 16) != dispprev)
                    {
                        dispprev = (dispctr >> 16);

                        val1 = core.readout.v1sum / core.readout.v1ctr;
                        val2 = core.readout.v2sum / core.readout.v2ctr;
                        *rawbuff1        = core.readout.v1min;
                        *(rawbuff1+100)  = core.readout.v1max;
                        *(rawbuff1+200)  = val1;

                        *rawbuff2        = core.readout.v2min;
                        *(rawbuff2+100)  = core.readout.v2max;
                        *(rawbuff2+200)  = val2;

                        core.readout.v1min = 0xffff;
                        core.readout.v1max = 0;
                        core.readout.v1sum = 0;
                        core.readout.v1ctr = 0;
                        core.readout.v2min = 0xffff;
                        core.readout.v2max = 0;
                        core.readout.v2sum = 0;
                        core.readout.v2ctr = 0;

                        rawbuff1++;
                        rawbuff2++;
                    }
                }
            }
            break;
        case rtt_thp:
            {
                uint16 *rawbuff1;
                uint16 *rawbuff2;
                uint16 *rawbuff3;
                register uint32 val1;
                register uint32 val2;
                register uint32 val3;

                rawbuff1 = (uint16*)(workbuff + WB_OFFS_TEMP + disp_ptr);
                rawbuff2 = (uint16*)(workbuff + WB_OFFS_RH + disp_ptr);
                rawbuff3 = (uint16*)(workbuff + WB_OFFS_P + disp_ptr);

                for ( i=0; i<smp_proc; i++ )
                {
                    // get the 12bit value - no shift in this case
                    if ( shifted )
                    {                                                               //      0          1          2          3          4
                        val1 = (( ((uint32)(wbuff[0]) << 8) | wbuff[1] ) & 0x0fff) << 4;   // [xxxx tttt][tttt tttt][hhhh hhhh][hhhh pppp][pppp pppp]
                        val2 = ( ((uint32)(wbuff[2]) << 4) | (wbuff[3] >> 4) ) << 4;
                        val3 = (( ((uint32)(wbuff[3]) << 8) | wbuff[4] ) & 0x0fff) << 4;
                        shifted = false;
                        wbuff += 5;
                    }
                    else
                    {
                        val1 = ( ((uint32)(wbuff[0]) << 4) | (wbuff[1] >> 4) ) << 4;       // [tttt tttt][tttt hhhh][hhhh hhhh][pppp pppp][pppp xxxx]
                        val2 = (( ((uint32)(wbuff[1]) << 8) | wbuff[2] ) & 0x0fff) << 4;
                        val3 = ( ((uint32)(wbuff[3]) << 4) | (wbuff[4] >> 4) ) << 4; 
                        shifted = true;
                        wbuff += 4;
                    }

                    // check min/max and sum for average
                    if ( core.readout.v1min > val1 )
                        core.readout.v1min = val1;
                    if ( core.readout.v1max < val1 )
                        core.readout.v1max = val1;
                    core.readout.v1ctr++;
                    core.readout.v1sum += val1;

                    if ( core.readout.v2min > val2 )
                        core.readout.v2min = val2;
                    if ( core.readout.v2max < val2 )
                        core.readout.v2max = val2;
                    core.readout.v2ctr++;
                    core.readout.v2sum += val2;

                    if ( core.readout.v3min > val3 )
                        core.readout.v3min = val3;
                    if ( core.readout.v3max < val3 )
                        core.readout.v3max = val3;
                    core.readout.v3ctr++;
                    core.readout.v3sum += val3;

                    // see if should proceed with the next point
                    dispctr += dispstep;
                    if ( (dispctr >> 16) != dispprev)
                    {
                        dispprev = (dispctr >> 16);

                        val1 = core.readout.v1sum / core.readout.v1ctr;
                        val2 = core.readout.v2sum / core.readout.v2ctr;
                        val3 = core.readout.v3sum / core.readout.v3ctr;
                        *rawbuff1        = core.readout.v1min;
                        *(rawbuff1+100)  = core.readout.v1max;
                        *(rawbuff1+200)  = val1;

                        *rawbuff2        = core.readout.v2min;
                        *(rawbuff2+100)  = core.readout.v2max;
                        *(rawbuff2+200)  = val2;

                        *rawbuff3        = core.readout.v3min;
                        *(rawbuff3+100)  = core.readout.v3max;
                        *(rawbuff3+200)  = val3;

                        core.readout.v1min = 0xffff;
                        core.readout.v1max = 0;
                        core.readout.v1sum = 0;
                        core.readout.v1ctr = 0;
                        core.readout.v2min = 0xffff;
                        core.readout.v2max = 0;
                        core.readout.v2sum = 0;
                        core.readout.v2ctr = 0;
                        core.readout.v3min = 0xffff;
                        core.readout.v3max = 0;
                        core.readout.v3sum = 0;
                        core.readout.v3ctr = 0;

                        rawbuff1++;
                        rawbuff2++;
                        rawbuff3++;
                    }
                }
            }
            break;
    }

    core.readout.dispctr  = dispctr;
    core.readout.dispprev = dispprev;
    core.readout.dispstep = dispstep;
}


uint32 internal_recording_read_calculate_next_step( uint32 length, uint32 *ee_size )
{
    uint32 ee_addr;

    ee_addr = core.readout.task_elsizeX2 * core.readout.to_ptr;
    if ( ee_addr & 0x01 )
        core.readout.shifted = true;
    else
        core.readout.shifted = false;
    ee_addr = ( ee_addr >> 1 ) + core.readout.task_offs;

    // calculate the length to be read - it should be the minimum bw. start_smpl -> wrap point  and  F1/F2 buffer size (256 bytes)
    if ( (core.readout.to_ptr + length) > core.readout.wrap )
        length = core.readout.wrap - core.readout.to_ptr;                   // limit the read length to the wrap point
    if ( (length * core.readout.task_elsizeX2) > ((WB_FLIPB_SIZE - 1)*2) ) 
        length = ((WB_FLIPB_SIZE - 1)*2) / core.readout.task_elsizeX2;      // limit the read length to the max flip buffer size with 1 byte reserve

    *ee_size = ((length * core.readout.task_elsizeX2) >> 1) + 1;            // always compensate for the half byte

    // save the next read position and decrease the read length
    core.readout.to_process = length;
    core.readout.to_read -= length;
    core.readout.to_ptr += length; 
    if ( core.readout.to_ptr >= core.readout.wrap )
        core.readout.to_ptr = 0;

    return ee_addr;
}

static inline void local_recording_readout( void )
{
    if ( (core.vstatus.int_op.f.op_recread == 0) ||     // bulky call - exit
         core.vstatus.int_op.f.graph_ok )
        return;

    if ( eeprom_is_operation_finished() == false )      // if still busy reading from NVRAM - exit
        return;

    // NVRAM read finished - set up the next read and process the data from the current one
    {
        uint8 *wbuff;
        uint32 smp_proc;
        bool finished = false;
        bool shifted;

        // prerequisites for data processing
        smp_proc = core.readout.to_process;         // sample points read from memory
        shifted = core.readout.shifted;
        if ( core.readout.flipbuff == 0 )
            wbuff = workbuff + WB_OFFS_FLIP1;
        else
            wbuff = workbuff + WB_OFFS_FLIP2;

        // put the DMA to work if there is more data to be read from NVRAM
        if ( core.readout.to_read )
        {
            uint32 ee_addr;
            uint32 ee_len;
            ee_addr = internal_recording_read_calculate_next_step( core.readout.to_read, &ee_len );

            if ( core.readout.flipbuff == 0 )
            {
                eeprom_read( ee_addr, ee_len, workbuff + WB_OFFS_FLIP2, true );        // read to F2, F1 will be processed below
                core.readout.flipbuff = 1;
            }
            else
            {
                eeprom_read( ee_addr, ee_len, workbuff + WB_OFFS_FLIP1, true );        // read to F1, F2 will be processed below
                core.readout.flipbuff = 0;
            }
        }
        else
            finished = true;

        // process the display points
        if ( core.readout.total_read <= WB_DISPPOINT )
            internal_recording_read_process_simple( wbuff, smp_proc, shifted );         // points fit to display - no min/max registering, just push them in the raw buffer
        else
            internal_recording_read_process_minmaxavg( wbuff, smp_proc, shifted, finished );      // points doesn't fit - min/max should be calculated, 

        internal_DBG_simu_1_cycle();

        if ( finished )
        {
            core.vstatus.int_op.f.graph_ok = 1;
            core.vstatus.int_op.f.op_recread = 0;
        }
    }
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
            core.nv.op.sens_rd.moni.sch_moni_temp += 2 * core_utils_timeunit2seconds( core.nv.setup.tim_tend_temp );
            // notify the UI to update
            core.measure.dirty.b.upd_th_tendency = 1;
        }
    }

    if ( core.nv.op.op_flags.b.op_recording )
    {
        local_recording_all_pushdata( ss_thermo, temp );
    }
}


static inline void local_process_hygro_sensor_result( uint32 rh )
{
    uint32 dew;     // calculated dew point temperature in 16FP9+40*
    uint32 abs;     // calculated absolute humidity in g/m3*100

    if ( core.nv.op.op_flags.b.op_recording )
    {
        local_recording_all_pushdata( ss_rh, rh );
    }

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
            core.nv.op.sens_rd.moni.sch_moni_hygro += 2 * core_utils_timeunit2seconds( core.nv.setup.tim_tend_hygro );
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

    if ( core.nv.op.op_flags.b.op_recording )
    {
        local_recording_all_pushdata( ss_pressure, (pr_filt >> 4) );
    }

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
            core.nv.op.sens_rd.moni.sch_moni_press += 2 * core_utils_timeunit2seconds( core.nv.setup.tim_tend_press );
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


static uint32 internal_sensor_shedule_increment( enum ESensorSelect sensor )
{
    uint32 val = CORE_SCHED_NONE;
    uint32 val_mon = CORE_SCHED_NONE;
    uint32 val_rec = CORE_SCHED_NONE; 

    if ( core.vstatus.int_op.f.sens_real_time == sensor )
        return CORE_SCHED_RT;

    // check for monitoring rates on the current sensor
    if ( core.nv.op.op_flags.b.op_monitoring )
    {
        switch( sensor )
        {
            case ss_thermo:     val_mon = core.nv.setup.tim_tend_temp; break;
            case ss_rh:         val_mon = core.nv.setup.tim_tend_hygro; break;
            case ss_pressure:   val_mon = core.nv.setup.tim_tend_press; break;
        }
    }

    // check for recording rates on the current sensor and peek the fastest
    if ( core.nv.op.op_flags.b.op_recording && core.vstatus.int_op.f.nv_rec_initted )
    {
        int i;
        for (i=0; i<STORAGE_RECTASK; i++)
        {
            if ( (core.nvrec.running & (1<<i)) &&                           // if task is running
                 (core.nvrec.task[i].task_elems & (1 << (sensor-1)) ) &&    // and sensor in use for this task
                 (val_rec > core.nvrec.task[i].sample_rate) )               // and current rate is bigger than the rate from task
            {
                val_rec = core.nvrec.task[i].sample_rate;
            }
        }
    }

    // get the fastest time
    if ( val_mon < val )
        val = val_mon;
    if ( val_rec < val )
        val = val_rec;

    // for pressure sensor we must have a minimum speed of 1min/sample
    if ( (sensor == ss_pressure) && (val > ut_60min) )
        val = ut_60min;
        
    // return the right sensor update period
    if ( val == CORE_SCHED_NONE  )
        return 0;                   // no read
    else if ( val == ut_60min )
        return CORE_SCHED_1MIN;     // 128 ticks -> 64sec / read  - used for 1hr updates
    else if ( val >= ut_1min )
        return CORE_SCHED_8SEC;     // 8sec / read                - used for 1min -> 30min updates
    else if ( val >= ut_10sec )
        return CORE_SCHED_4SEC;     // 4sec / read                - used for 10sec -> 30sec
    else
        return CORE_SCHED_2SEC;     // 2sec / read                - used for 5sec updates
}

static void internal_sensor_shedule_setval( uint32 incval, uint32 *pshed )
{
    uint32 shed = *pshed;

    if ( incval == 0 )      // no increment should be done, disable sheduling
    {
        *pshed = CORE_SCHED_NONE;
        return;
    }

    shed = (RTCclock + incval);
    switch ( incval )
    {
        case CORE_SCHED_RT:     *pshed = (shed & CORE_SCHED_RT_MASK); return;
        case CORE_SCHED_2SEC:   *pshed = (shed & CORE_SCHED_2SEC_MASK); return;
        case CORE_SCHED_4SEC:   *pshed = (shed & CORE_SCHED_4SEC_MASK); return;
        case CORE_SCHED_8SEC:   *pshed = (shed & CORE_SCHED_8SEC_MASK); return;
        case CORE_SCHED_1MIN:   *pshed = (shed & CORE_SCHED_1MIN_MASK); return;
    }
}

static inline void local_check_sensor_read_schedules(void)
{
    if ( core.nv.op.sched.sch_thermo <= RTCclock )
    {
        Sensor_Acquire( SENSOR_TEMP );
        core.vstatus.int_op.f.op_sread |= SENSOR_TEMP; 
        internal_sensor_shedule_setval( internal_sensor_shedule_increment( ss_thermo ), &core.nv.op.sched.sch_thermo );
    }
    if ( core.nv.op.sched.sch_hygro <= RTCclock )
    {
        Sensor_Acquire( SENSOR_RH );
        core.vstatus.int_op.f.op_sread |= SENSOR_RH; 
        internal_sensor_shedule_setval( internal_sensor_shedule_increment( ss_rh ), &core.nv.op.sched.sch_hygro );
    }
    if ( core.nv.op.sched.sch_press <= RTCclock )
    {
        Sensor_Acquire( SENSOR_PRESS );
        core.vstatus.int_op.f.op_sread |= SENSOR_PRESS; 
        internal_sensor_shedule_setval( internal_sensor_shedule_increment( ss_pressure ), &core.nv.op.sched.sch_press );
    }

    local_check_first_scheduled_op();
}


static void local_sensor_reschedule_all(void)
{
    internal_sensor_shedule_setval( internal_sensor_shedule_increment( ss_thermo ), &core.nv.op.sched.sch_thermo );
    internal_sensor_shedule_setval( internal_sensor_shedule_increment( ss_rh ), &core.nv.op.sched.sch_hygro );
    internal_sensor_shedule_setval( internal_sensor_shedule_increment( ss_pressure ), &core.nv.op.sched.sch_press );

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

static void local_initialize_recording(void)
{
    int i;
    memset( &core.nvrec, 0, sizeof(core.nvrec) );

    core.nvrec.dirty = true;
    core.nvrec.running = 0;                     // no task in run

    core.nvrec.task[0].mempage = 0;             // 0x400 offset
    core.nvrec.task[0].size = 60;               // 60kbytes of data -> 7.1 days (~week) of TH aquisition with 1/2 min resolution
    core.nvrec.task[0].task_elems = rtt_th;  

    core.nvrec.task[1].mempage = 60;            // 0x400 offset
    core.nvrec.task[1].size = 30;               // 30kbytes of data -> 7.1 days (~week) of P aquisition with 1/2 min resolution
    core.nvrec.task[1].task_elems = rtt_p;  

    for (i=0; i<STORAGE_RECTASK; i++)
    {
        core.nvrec.task[i].sample_rate = ut_30sec;
        if ( i > 1 )
        {
            core.nvrec.task[i].task_elems = rtt_p;  
            core.nvrec.task[i].size = 1;  
            core.nvrec.task[i].mempage = 90 + (i-2);
        }
    }

    core.vstatus.int_op.f.nv_rec_initted = 1;
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

uint32 core_utils_timeunit2seconds( uint32 time_unit )
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

    if ( core.nvrec.dirty )
    {
        core.nvrec.dirty = false;

        cksum_setup = 0xABCD;
        buffer = (uint8*)&core.nvrec;
        for ( i=0; i<sizeof(core.nvrec); i++ )
            cksum_setup = cksum_setup + ( (uint16)(buffer[i] << 8) - (uint16)(~buffer[i]) ) + 1;

        if ( eeprom_write( EEADDR_RECORD, buffer, sizeof(core.nvrec), true ) != sizeof(core.nvrec) )
            return -1;
        while ( eeprom_is_operation_finished() == false );

        if ( eeprom_write( EEADDR_CK_REC, (uint8*)&cksum_setup, 2, false ) != 2 )
            return -1;
    }

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
    local_initialize_recording();

    if ( save )
    {
        return core_setup_save();
    }
    return 0;
}


int core_nvrecording_load( void )
{
    uint32  i;
    uint16  cksum;
    uint16  cksum_op;
    uint8   *buffer;

    if ( eeprom_enable(false) )
        return -1;
    while ( eeprom_is_operation_finished() == false );

    buffer = (uint8*)&core.nvrec;
    if ( eeprom_read( EEADDR_RECORD, sizeof(core.nvrec), buffer, true ) != sizeof(core.nvrec) )
        return -1;
    while ( eeprom_is_operation_finished() == false );
    if ( eeprom_read( EEADDR_CK_REC, 2, (uint8*)&cksum_op, false ) != 2 )
        return -1;
    while ( eeprom_is_operation_finished() == false );

    cksum = 0xABCD;
    for ( i=0; i<sizeof(core.nvrec); i++ )
        cksum = cksum + ( (uint16)(buffer[i] << 8) - (uint16)(~buffer[i]) ) + 1;
    if ( cksum != cksum_op )
        return -2;

    core.vstatus.int_op.f.nv_rec_initted = 1;
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


    // check if recording data should be loaded
    if ( core.nv.op.op_flags.b.op_recording )
    {
        if ( core_nvrecording_load() )
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


    if ( core.nv.dirty || core.nvrec.dirty )
    {
        core.nv.dirty = false;
        core_setup_save();
    }
}


void core_op_realtime_sensor_select( enum ESensorSelect sensor )
{
    if ( sensor == ss_none )
        core.vstatus.int_op.f.sens_real_time = 0;

    core.vstatus.int_op.f.sens_real_time = sensor;

    // reshedule the sensor reads
    local_sensor_reschedule_all();
}


void core_op_monitoring_switch( bool enable )
{
    if ( enable == false )
    {
        // disabling monitoring - clean up the
        if ( core.nv.op.op_flags.b.op_monitoring )
        {
            core.nv.op.op_flags.b.op_monitoring = 0;
            local_sensor_reschedule_all();
        }
    }
    else if ( core.nv.op.op_flags.b.op_monitoring == 0 )
    {
        internal_clear_monitoring();
        core.nv.op.sens_rd.moni.sch_moni_temp  = RTCclock + 2 * core_utils_timeunit2seconds( core.nv.setup.tim_tend_temp );
        core.nv.op.sens_rd.moni.sch_moni_hygro = RTCclock + 2 * core_utils_timeunit2seconds( core.nv.setup.tim_tend_hygro );
        core.nv.op.sens_rd.moni.sch_moni_press = RTCclock + 2 * core_utils_timeunit2seconds( core.nv.setup.tim_tend_press );
        core.nv.op.sens_rd.moni.clk_last_day = RTCclock / DAY_TICKS;
        core.nv.op.sens_rd.moni.clk_last_week = (RTCclock + DAY_TICKS) / WEEK_TICKS;
        core.nv.op.op_flags.b.op_monitoring = 1;
        local_sensor_reschedule_all();
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
                    core.nv.op.sens_rd.moni.sch_moni_temp = RTCclock + 2 * core_utils_timeunit2seconds( timing );
                    core.nv.op.sens_rd.moni.avg_ctr_temp = 0;
                    core.nv.op.sens_rd.moni.avg_sum_temp = 0;
                    local_tendency_restart( CORE_MMP_TEMP );
                    internal_sensor_shedule_setval( internal_sensor_shedule_increment( ss_thermo ), &core.nv.op.sched.sch_thermo );
                    local_check_first_scheduled_op();
                }
            }
            break;
        case ss_rh: 
            if ( core.nv.setup.tim_tend_hygro != timing )
            {
                core.nv.setup.tim_tend_hygro = timing;
                if ( core.nv.op.op_flags.b.op_monitoring )
                {
                    core.nv.op.sens_rd.moni.sch_moni_hygro = RTCclock + 2 * core_utils_timeunit2seconds( timing );
                    core.nv.op.sens_rd.moni.avg_ctr_hygro = 0;
                    core.nv.op.sens_rd.moni.avg_sum_rh = 0;
                    core.nv.op.sens_rd.moni.avg_sum_abshum = 0;
                    local_tendency_restart( CORE_MMP_RH );
                    local_tendency_restart( CORE_MMP_ABSH );
                    internal_sensor_shedule_setval( internal_sensor_shedule_increment( ss_rh ), &core.nv.op.sched.sch_hygro );
                    local_check_first_scheduled_op();
                }
            }
            break;
        case ss_pressure: 
            if ( core.nv.setup.tim_tend_press != timing )
            {
                core.nv.setup.tim_tend_press = timing;
                if ( core.nv.op.op_flags.b.op_monitoring )
                {
                    core.nv.op.sens_rd.moni.sch_moni_press = RTCclock + 2 * core_utils_timeunit2seconds( timing );
                    core.nv.op.sens_rd.moni.avg_ctr_press = 0;
                    core.nv.op.sens_rd.moni.avg_sum_press = 0;
                    local_tendency_restart( CORE_MMP_PRESS );
                    internal_sensor_shedule_setval( internal_sensor_shedule_increment( ss_pressure ), &core.nv.op.sched.sch_press );
                    local_check_first_scheduled_op();
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


void core_op_recording_init(void)
{
    if ( core.vstatus.int_op.f.nv_rec_initted )
        return;

    if ( core_nvrecording_load() )
    {
        core.vstatus.ui_cmd |= CORE_UISTATE_EECORRUPTED;
        // TODO the rest
    }
    eeprom_deepsleep();
}

void core_op_recording_switch( bool enabled )
{
    int i;

    if ( core.nv.op.op_flags.b.op_recording )
    {
        if ( enabled == false )
        {
            // recording master switch - stop the currently running tasks (do not clear the run flag)
            for ( i=0; i<STORAGE_RECTASK; i++ )
            {
                if ( core.nvrec.running & (1<<i) )
                    local_recording_task_stop( i );
            }
            core.nv.op.op_flags.b.op_recording = 0;
            core.nvrec.dirty = true;
            local_sensor_reschedule_all();
        }
    }
    else if ( enabled )
    {
        // recording master switch - reset and start the currently running tasks
        for ( i=0; i<STORAGE_RECTASK; i++ )
        {
            if ( core.nvrec.running & (1<<i) )
            {
                local_recording_task_reset( i );
                local_recording_task_start( i );
            }
        }
        core.nv.op.op_flags.b.op_recording = 1;
        core.nvrec.dirty = true;
        local_sensor_reschedule_all();
    }
}

void core_op_recording_setup_task( uint32 task_idx, struct SRecTaskInstance *task )
{
    bool clean_up = false;

    if ( memcmp( task, &core.nvrec.task[task_idx], sizeof(struct SRecTaskInstance) ) )
    {
        core.nvrec.task[task_idx] = *task;
        local_recording_task_reset( task_idx );

        if ( core.nv.op.op_flags.b.op_recording && (core.nvrec.running & (1<<task_idx)) )
            local_recording_task_start( task_idx );

        local_sensor_reschedule_all();
        core.nvrec.dirty = true;
    }
}

void core_op_recording_task_run( uint32 task_idx, bool run )
{
    if ( core.nvrec.running & (1<<task_idx) )
    {
        if ( run == false )
        {
            // stop the task
            local_recording_task_stop( task_idx );

            core.nvrec.running &= ~(1<<task_idx);
            core.nvrec.dirty = true;
            local_sensor_reschedule_all();
        }
    }
    else if ( run )
    {
        // task is started
        if ( core.nv.op.op_flags.b.op_recording )
        {
            local_recording_task_reset( task_idx );
            local_recording_task_start( task_idx );
        }

        core.nvrec.running |= (1<<task_idx);
        core.nvrec.dirty = true;
        local_sensor_reschedule_all();
    }
}

uint32 core_op_recording_get_total_samplenr( uint32 mem_len, enum ERecordingTaskType rectype )
{
    uint32 factor = 0;
    switch ( rectype )
    {
        case rtt_t:
        case rtt_h:
        case rtt_p:
            factor = 3;       // 1.5 byte  - 1x 12bit
            break;
        case rtt_th:
        case rtt_hp:
        case rtt_tp:
            factor = 6;       // 3 bytes   - 2x 12bit
            break;
        case rtt_thp:
            factor = 9;       // 4.5 bytes - 3x 12bit
            break;
    }

    return ( (mem_len * 2) / factor ); 
}

int core_op_recording_read_request( uint32 task_idx, uint32 smpl_depth, uint32 length )
{
    struct SRecTaskInternals *pfunc;

    pfunc = &core.nvrec.func[task_idx];

    if ( (smpl_depth > pfunc->c) || ( length > smpl_depth ) )      // prevent faulty values
        return -1;
    if ( core.vstatus.int_op.f.op_recread )                         // if read in progress - do not allow a new one
        return -2;

    core.vstatus.int_op.f.graph_ok = 0;                             // data is being processed - clear the graph ok flag

    {
        uint32 ee_addr;
        uint32 ee_size;
        uint32 start_smpl;
        uint32 to_read;

        // enable eeprom
        eeprom_enable(false);

        memset( &core.readout, 0, sizeof(core.readout) );

        // find out the start pointer
        if ( pfunc->w >= smpl_depth )
            start_smpl = pfunc->w - smpl_depth;
        else
            start_smpl = (pfunc->w + pfunc->wrap) - smpl_depth;

        // set up readout state variables
        core.readout.task_offs = EEADDR_STORAGE + (uint32)core.nvrec.task[task_idx].mempage * CORE_RECMEM_PAGESIZE; // save the Task memory offset
        core.readout.taks_elem = core.nvrec.task[task_idx].task_elems;                                              // save the task elem type
        core.readout.total_read = length;                                                                           // how many samples should be read in total
        core.readout.to_read = length; 
        core.readout.wrap = pfunc->wrap;
        core.readout.to_ptr = start_smpl;
        switch ( core.readout.taks_elem )
        {
            case rtt_t:
            case rtt_h:
            case rtt_p: core.readout.task_elsizeX2 = 3; break;
            case rtt_th:
            case rtt_tp:
            case rtt_hp: core.readout.task_elsizeX2 = 6; break;
            default:     core.readout.task_elsizeX2 = 9; break;
        }

        core.readout.v1min = 0xffff;
        core.readout.v2min = 0xffff;
        core.readout.v3min = 0xffff;

        if ( length > WB_DISPPOINT )
        {
            // case when there are more elements than display points
            core.readout.dispstep = (100 << 16) / length;
        }

        // calculate the memory address and size and advance the read pointers
        ee_addr = internal_recording_read_calculate_next_step( length, &ee_size );

        // start the read operation
        while ( eeprom_is_operation_finished() == false );      // check if wake-up is done (if was the case)
        eeprom_read( ee_addr, ee_size, workbuff + WB_OFFS_FLIP1, true );

        internal_DBG_simu_1_cycle();
    }

    core.vstatus.int_op.f.op_recread = 1;
    core.vstatus.int_op.f.core_bsy = 1;
}

void core_op_recording_read_cancel( uint32 task_idx )
{
    if ( core.vstatus.int_op.f.op_recread == 0 )
        return;

    while ( eeprom_is_operation_finished() == false );

    core.vstatus.int_op.f.op_recread = 0;
    core.vstatus.int_op.f.graph_ok = 0;

    if ( core.vstatus.int_op.f.op_recsave == 0 )
    {
        if ( core.vstatus.int_op.f.op_sread == 0 )
            core.vstatus.int_op.f.core_bsy = 0;
        eeprom_deepsleep();  
    }
}

int core_op_recording_read_busy(void)
{

}


// for calculations and formulas see reg_generator.mcdx
// 2*PI = 6.28 - integer part on 3 bytes, max X = 2^16 - 16bytes -> 13  ==> use FP12 for safety and sign
#define PI2FP12 25736           // 2*pi in FP12                         

int internal_dbgfill_calculate_points( int i, uint32 smpl_day, uint32 diff, uint32 minim )
{
    int val;

    val = (int) (  ( 1 - cos((i*PI2FP12)/(float)(smpl_day << 12)) ) *                                   // ( 1-cos(x*2pi/sday)) * 
                   (  diff  *  ( cos( (i*PI2FP12)/(float)( (smpl_day << 12) * 10 ) ) + 1 )   /  4 )  ); // (vmax - vmin) *  cos(x*2pi/sday*10)+1  / 4

    val = val + minim + (int)( sin((i*PI2FP12*(uint64)24)/(float)(smpl_day << 12)) * diff / 10 );       // high frequency component
    if ( val < 0 )
        val = 0;
    if ( val > 0xffff )
        val = 0xffff;
    return val;
}


void core_op_recording_dbgfill( uint32 t )
{
    int i;
    uint32 smpl_day;
    uint32 smpl_total;
    uint32 elems;
    int val;
    
    smpl_total = 3 + core_op_recording_get_total_samplenr( core.nvrec.task[t].size * CORE_RECMEM_PAGESIZE, core.nvrec.task[t].task_elems );
    smpl_day = (24 * 3600) / core_utils_timeunit2seconds( core.nvrec.task[t].sample_rate );
    elems = core.nvrec.task[t].task_elems;

    local_recording_task_reset(t);

    for ( i=0; i<smpl_total; i++ )
    {
        core.nvrec.func[t].shedule = 0;

        if ( elems & CORE_BM_TEMP )
        {
            val = internal_dbgfill_calculate_points(i, smpl_day, (( 128 - 0 ) << TEMP_FP),  ( 0 << TEMP_FP ) );
            local_recording_pushdata( t, ss_thermo, val );
        }
        if ( elems & CORE_BM_RH )
        {
            val = internal_dbgfill_calculate_points(i, smpl_day, (( 100 - 0 ) << RH_FP),  ( 0 << RH_FP ) );
            local_recording_pushdata( t, ss_rh, val );
        }
        if ( elems & CORE_BM_PRESS )
        {
            val = internal_dbgfill_calculate_points(i, smpl_day, ( 60000 - 0 ),  0 );
            local_recording_pushdata( t, ss_pressure, val );
        }

        local_recording_savedata();
    }

    if ( (core.vstatus.int_op.f.op_recsave == 0 ) &&
         (core.vstatus.int_op.f.op_recread == 0 )   )
    {
        if ( core.vstatus.int_op.f.op_sread == 0 )
            core.vstatus.int_op.f.core_bsy = 0;
        eeprom_deepsleep();  
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

            if ( core.vstatus.int_op.f.op_sread )
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
                        core.vstatus.int_op.f.op_sread &= ~SENSOR_TEMP;
                    }
                    if ( result & SENSOR_RH )
                    {
                        local_process_hygro_sensor_result( Sensor_Get_Value(SENSOR_RH) );
                        core.vstatus.int_op.f.op_sread &= ~SENSOR_RH;
                    }
                    if ( result & SENSOR_PRESS )
                    {
                        local_process_pressure_sensor_result( Sensor_Get_Value(SENSOR_PRESS) );
                        core.vstatus.int_op.f.op_sread &= ~SENSOR_PRESS;
                    }

                    if ( (core.vstatus.int_op.f.op_sread == 0) &&
                         (core.vstatus.int_op.f.op_recsave == 0 ) &&
                         (core.vstatus.int_op.f.op_recread == 0 )   )
                    {
                        core.vstatus.int_op.f.core_bsy = 0;
                    }
                }
                break;                                  // break the busy loop
            }

            if ( core.vstatus.int_op.f.op_recsave )
            {
                local_recording_savedata();

                if ( (core.vstatus.int_op.f.op_recsave == 0 ) &&
                     (core.vstatus.int_op.f.op_recread == 0 )   )
                {
                    if ( core.vstatus.int_op.f.op_sread == 0 )
                        core.vstatus.int_op.f.core_bsy = 0;
                    eeprom_deepsleep();  
                }
                break;                                  // break the busy loop
            }

            if ( core.vstatus.int_op.f.op_recread )
            {
                local_recording_readout();

                if ( (core.vstatus.int_op.f.op_recsave == 0 ) &&
                     (core.vstatus.int_op.f.op_recread == 0 )   )
                {
                    if ( core.vstatus.int_op.f.op_sread == 0 )
                        core.vstatus.int_op.f.core_bsy = 0;
                    eeprom_deepsleep();  
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

    internal_DBG_dump_values();
    
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
        if ( core.vstatus.int_op.f.nv_state == CORE_NVSTATE_PWR_RUNUP )     // in uninitted state it waits for 1ms NVram start-up
            return (pwr | SYSSTAT_CORE_RUN_FULL);
        if ( core.vstatus.int_op.f.op_recsave || core.vstatus.int_op.f.op_recread )
            return SYSSTAT_CORE_BULK;
    }

    return pwr;
}








