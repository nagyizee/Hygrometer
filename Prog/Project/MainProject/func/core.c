/*
 *
 *  Operation modes:
 *      - long expo:        Holding the shutter for x seconds set up on the main window
 *      - sequential:       Sequential photography with given nr. of pictures in given interval between them
 *      - timer:            Timer for first photo
 *      - light trigger:    Photo taken at light change event
 *      - sound trigger:    Photo taken at sound spike event
 *
 *  Mirror lockup can be activated with a timeout for vibration attenuation. settable in second.
 *  The effect of mirror lockup for various modes:
 *      - long expo:        long expo countdown begins after mirror lockup timeout.
 *      - sequential:       lockup interval is counted in the picture interval, actual picture is taken after mirror lockup timeout
 *      - timer:            timer is downcounted, then mirror lockup timeout, then picture is taken.
 *      - l/s trigger:      mirror lockup shutter release at starting the program, picture taken in moment of trigger event
 *
 *  Trigger event can be delayed in xxx.x milliseconds for precise triggering. Triggering is IRQ controlled, no application lags are considered
 *
 *  Allowed mode combinations:
 *      - all standalone
 *      - long expo, sequential, timer - in any combination
 *      - light trigger is mutually exclussive with sound trigger
 *      - trigger can be combined with long expo and timer.
 *      - if trigger is combined with sequential then it will keep taking pictures on trigger event and ignores the interval between pictures
 *        if timer is combined also then it is considered only for the first shot
 *  
 *  Analog part:
        - gain is in range of 5% - 100%
          5% is the full range (1x gain) of the low gain adc input. 
          50% is 10x gain of the low gain adc input.
          55% is the full range of the high gain adc input
          100% is the 10x gain of the high gain adc input 
          So in summary 1% setup gain is the full range and 100% setup gain is 100x overall gain

 *
 *
 **/

#include <stdio.h>
#include <string.h>
#include "core.h"
#include "hw_stuff.h"
//#include "hw_timers.h"
#include "eeprom_spi.h"

#define ADC_BASE_UNINITTED      0xffffffff
#define ADC_BASE_FILTER_SHIFT   13

#ifdef ON_QT_PLATFORM
struct STIM1 stim;
struct STIM1 *TIM1 = &stim;
struct STIM1 *TIMER_ADC;
#endif

static struct SCore         core;
volatile struct SADC        adc;


// called by ADC isr when trigger condition satisfied and no pretrigger is needed
// called by Pretrigger timer isr when time is up ( this is started by ADC isr when conditions satisfied and pretrigger is needed )
static void core_ISR_internal_shutter(void)
{
    if ( (core.op.opmode & OPMODE_TIMER) == 0 )
    {
        HW_Shutter_Release_ON();        // release shutter if not in timer mode
        core_beep( beep_expo_tick );
    }
    HW_ADC_ResetPretrigger();           // reset the pretrigger timer
    adc.armed = false;                  // reset the armed status - tell the application that event occured
    adc.base = ADC_BASE_UNINITTED;      // reinit base
    HW_ADC_StartAcquisition( (uint32)adc.samples, adc.adc_in_run - 1 ); // restart acquisition
}



void CoreADC_ISR_Complete(void)
{
    uint32 mval;
    int32 gain;
    // clear the interrupt request
    ADC1->SR &= ~( ADC_IF_EOC );

    // register the minimums and maximums
    gain = 2;
    while ( gain )
    {
        if ( adc.samples[gain] > adc.smplmax[gain-1] )
            adc.smplmax[gain-1] = adc.samples[gain];
        if ( adc.samples[gain] < adc.smplmin[gain-1] )
            adc.smplmin[gain-1] = adc.samples[gain];
        gain--;
    }

    // get the operation parameters and the selected measured channel
    if ( adc.adc_in_run == 1 )
        gain = core.setup.light.gain;
    else if ( adc.adc_in_run == 2 )
        gain = core.setup.sound.gain;
    else
        return;

    if ( (adc.channel_lock == 0) )
    {
        if ( adc.adc_in_run == 1)
        {
            if ( core.setup.light.bright )
                mval = adc.samples[2];              // high gain
            else
                mval = adc.samples[1];              // low gain
        }
        else
            mval = adc.samples[1];      // for audio we currently support low gain only
    }
    else
    {
        mval = adc.samples[adc.channel_lock];
    }

    // calculate the comparation base level
    if ( adc.base == ADC_BASE_UNINITTED )
    {
        adc.base = mval;
        adc.filter_dly = mval << ADC_BASE_FILTER_SHIFT;
    }
    else
    {
        // simple first order filter 
        // Yt = (1-a)*Yt-1 + a*Xt       -> converted to fixed point
        adc.filter_dly = adc.filter_dly - ( adc.filter_dly >> ADC_BASE_FILTER_SHIFT ) + mval;
        adc.base = adc.filter_dly >> ADC_BASE_FILTER_SHIFT;
    }

    // calculate the measured value
    {
        int32 val;
        val = (((int32)mval - (int32)adc.base) * gain) / 10; // calculate with base and gain for the 12bit value
        val = (val * 100) / 4096;                            // bring it down to +/- 100 range
        if ( val > 100 )                                     // normalize to +/- 100 max
            val = 100;
        if ( val < -100 )
            val = -100;
        if ( (adc.adc_in_run == 2) && (val < 0) )           // for sound only the positive difference is considered
            val = 0;
        adc.value = val;
        if ( adc.vmin > val )
            adc.vmin = val;
        if ( adc.vmax < val )
            adc.vmax = val;
    }

    // trigger is armed - check for conditions
    if ( adc.armed )
    {
        uint32 pretrig;
        bool trg_ok = false;

        // check if threshold levels reached
        if ( adc.adc_in_run == 1 )      // light
        {
            if ( (core.setup.light.trig_h && (adc.value > core.setup.light.thold)) || 
                 (core.setup.light.trig_l && ((0-adc.value) > core.setup.light.thold) ) )
                 trg_ok = true;
            pretrig = core.setup.light.iv_pretrig;
        }
        else                            // sound
        {   
            if ( adc.value > core.setup.sound.thold )
                 trg_ok = true;
            pretrig = core.setup.sound.iv_pretrig;
        }

        // if threshold levels then 
        if ( trg_ok )
        {
            // stop the aquisition and this IRQ - this is mainly done for freeing up ADC timer to be used as pretrigger timer
            HW_ADC_StopAcquisition();
            if ( pretrig )
            {
                // if pretriggered - set up pretrigger timer
                HW_ADC_SetupPretrigger( pretrig );
            }
            else
            {
                // release the shutter
                core_ISR_internal_shutter();
            }
        }
    }
}


void Core_ISR_PretriggerCompleted(void)
{
    TIMER_ADC->SR = (uint16)~TIM_FLAG_Update; 
    if ( adc.adc_in_run == 0 )  // prevent accidental triggering after trigger function is stopped by the application
        return; 

    core_ISR_internal_shutter();    // this will clear the irq flag also
}



void core_update_battery()
{
    uint32 battery;
    if ( adc.adc_in_run == 0 )
        battery = HW_ADC_GetBattery();
    else
        battery = adc.samples[0];

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


static int core_internal_get_interval(void)
{
    int time = 0;

    time = core.setup.seq_interval - core.setup.mlockup;

    if ( core.op.opmode & OPMODE_LONGEXPO )
    {
        time -= core.setup.time_longexpo.hour * 3600 + core.setup.time_longexpo.minute *60 + core.setup.time_longexpo.second;
    }
    else
    {
        time -= 1;
    }

    if ( time < 0 )
        return 0;
    return time;
}

static void core_internal_calculate_total_time(void)
{
    uint32 tm = 0;

    if ( (core.op.opmode & OPMODE_SEQUENTIAL) && ((core.op.opmode & (OPMODE_LIGHT | OPMODE_SOUND)) == 0) )
    {
        tm = core.setup.seq_interval * core.setup.seq_shotnr;       // calculate the sequencial time only for non event triggered operations
    }
    else 
    {
        tm += core.setup.mlockup;
        if ( core.op.opmode & OPMODE_LONGEXPO )
        {
            tm += core.setup.time_longexpo.hour * 3600 + core.setup.time_longexpo.minute *60 + core.setup.time_longexpo.second;
        }
    }
    if ( core.op.opmode & OPMODE_TIMER )
    {
        tm += core.op.tmr_interval; 
    }

    core.op.op_time_left = tm;
}


static void core_internal_eventtrigger_start(void)
{
    // if mirror lockup - then do this first
    if (  core.setup.mlockup )
    {
        HW_Shutter_Prefocus_ON();
        HW_Shutter_Release_ON();
        core.op.w_time  = core.setup.shtr_pulse;                   // 150ms shutter pulse
        core.op.expo    = 2;                    // mirror lockup in seconds - use 2 seconds
        core.op.op_state = OPSTATE_MLOCKUP;
    }
    else
    {
        HW_Shutter_Prefocus_ON();               // prefocus to speed up camera reaction
        core.op.op_state = OPSTATE_EVENT;
    }
}



// called after mirror lockup or timer
static void core_internal_process_next_op(void)     
{
    HW_Shutter_Prefocus_ON();

    // if was in mirror lockup and event triggering in use
    if ( ( core.op.op_state == OPSTATE_MLOCKUP ) &&
         ( core.op.opmode & ( OPMODE_LIGHT | OPMODE_SOUND )) )
    {
        core.op.op_state = OPSTATE_EVENT;   // wait for event state
        adc.armed = true;                   // arm the trigger
        // w_time should be 0 till now
        // when trigger is armed - the adc input sampling routine will look after the event condition
        //    - on event condition (including pretrigger timer) it will release the shutter and 
        //      removes the armed flag.
        //    - core poll will detect this and start a w_time for shutter signal off and continue the 
        //      exposing
        return;
    }

    // for mirror lockup
    if (  core.setup.mlockup &&
         (core.op.op_state != OPSTATE_MLOCKUP) &&
         (core.op.op_state != OPSTATE_EVENT)  )
    {
        HW_Shutter_Release_ON();
        core.op.w_time  = core.setup.shtr_pulse;                    //150ms shutter pulse
        core.op.expo = core.setup.mlockup;                          //mirror lockup in seconds
        core.op.op_state = OPSTATE_MLOCKUP;
        return;
    }

    // release the shutter only for normal modes
    if ( ((core.op.opmode & ( OPMODE_LIGHT | OPMODE_SOUND)) == 0) ||    // triggered modes will do it in ISR
         ( core.op.op_state == OPSTATE_TIMER )  )                       // exception for timer mode which will trigger from application - so this exception is needed here
    {
        HW_Shutter_Release_ON();
        if ( BeepIsRunning() == false )
            core_beep( beep_expo_tick );
    }

    // for normal expo
    if ( (core.op.opmode & OPMODE_LONGEXPO) == 0 )  // if no longexpo in use
    {                                               //
        core.op.w_time = core.setup.shtr_pulse;     // 150ms shutter impulse
    }

    core_reset_longexpo();
    core.op.op_state = OPSTATE_EXPO;    // enter in exposing state
}

// called after an expo procedure is finished
static bool core_internal_process_after_expo(void)
{
    if ( core.op.opmode & OPMODE_LONGEXPO )
    {
        HW_Shutter_Release_OFF();       // turn off release
        HW_Shutter_Prefocus_OFF();      // and prefocus
    }

    if ( core.op.opmode & OPMODE_SEQUENTIAL )
    {
        core.op.seq_shotnr --;
        if ( core.op.seq_shotnr )
        {
            if ( core.op.opmode & (OPMODE_LIGHT | OPMODE_SOUND) )       // for light or sound trigger ignore the interval
            {
                core.op.seq_interval = 0;
                core.op.op_time_left = 0;
                core.op.w_time = 50;                    // wait additional 500ms for everything to stabilize. Note - do not use 0 - will break the code sequence
                core_internal_eventtrigger_start();     // set modes for event trigger functionality
                return false;
            }
            else
            {
                core.op.op_state = OPSTATE_WAIT_NEXT;
                core.op.seq_interval = core_internal_get_interval();    // use interval - mirror lockup interval for normal cases
            }

            if ( core.op.seq_interval == 0 )
            {
                core_internal_process_next_op();
            }
            return false;
        }
    }

    // operation finished
    core_stop();
    return true;
}


static void core_reset_second( struct SEventStruct *evmask )
{
    struct SEventStruct evtemp = {0, };
    evtemp.timer_tick_1sec = 1;
    evtemp.timer_tick_10ms = 1;
    evtemp.timer_tick_system = 1;
    evtemp.reset_sec = 1;

    // mark second start in RTC
    HW_Seconds_Start();

    // reset events
    if ( evmask != NULL )
    {
        evmask->timer_tick_10ms = 0;
        evmask->timer_tick_1sec = 0;
        evmask->timer_tick_system = 0;
    }

    Event_Clear(evtemp);

}


/////////////////////////////////////////////////////
//
//   main routines
//
/////////////////////////////////////////////////////


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

    setup->time_longexpo.hour   = 0;
    setup->time_longexpo.minute = 0;
    setup->time_longexpo.second = 20;
    setup->time_longexpo.t100   = 0;

    setup->seq_shotnr = 3;
    setup->seq_interval = 1;

    setup->tmr_interval = 5;

    setup->light.gain = 30;
    setup->light.thold = 12;
    setup->light.trig_h = 1;
    setup->light.trig_l = 0;
    setup->light.iv_pretrig = 0;
    setup->light.bright = 0;

    setup->sound.gain = 50;
    setup->sound.thold = 20;
    setup->sound.iv_pretrig = 0;

    setup->mlockup  = 0;
    setup->disp_brt_on  = 0x30;
    setup->disp_brt_dim = 12;
    setup->disp_brt_auto = 0;
    setup->pwr_stdby = 30;       // 30sec standby
    setup->pwr_disp_off = 60;    // 1min standby
    setup->pwr_off = 300;        // 5min pwr off

    setup->shtr_pulse = 20;

    setup->beep_on = 1;
    setup->beep_hi = 1554;
    setup->beep_low = 2152;

    if ( save )
    {
        return core_setup_save();
    }
    return 0;
//////////////////////////////////
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


void core_reset_longexpo(void)
{
    core.op.time_longexpo  = core.setup.time_longexpo;
    core.op.expo = core.setup.time_longexpo.hour * 3600 + core.setup.time_longexpo.minute *60 + core.setup.time_longexpo.second;
}

void core_reset_sequential(void)
{
    core.op.seq_interval   = core.setup.seq_interval;
    core.op.seq_shotnr     = core.setup.seq_shotnr;
}

void core_reset_timer(void)
{
    core.op.tmr_interval   = core.setup.tmr_interval;
}


void core_start(void)
{
    // reset operation parameters for all operation before startup
    core_reset_longexpo();
    core_reset_sequential();
    core_reset_timer();

    if ( core.op.opmode & OPMODE_CABLE )
    {
        HW_Shutter_Prefocus_ON();
        HW_Shutter_Release_ON();
        core_beep( beep_expo_tick );
        core_reset_second(NULL);
        core.op.op_state = OPSTATE_CABLE;
        return;                 // keep it simple - when (S) is pressed set the shutter signal, when released, release the shutter signal
    }

    // If event trigger is in use
    if ( core.op.opmode & ( OPMODE_LIGHT | OPMODE_SOUND ) )
    {
        // if no adc sampling started - start it
        if ( adc.adc_in_run == 0 )
        {
            core.op.w_time = 200;       // 2 second for analog startup and circuit stabilization
            core_analog_start( (core.op.opmode & OPMODE_SOUND) ? true : false );
        }
        else
            core.op.w_time = 100;       // 1 second for preventing glitches because of user action

        core_internal_eventtrigger_start();
        core.op.op_time_left = 0;       // no time-left estimation for event triggered shooting
    }
    // for normal cases
    else
    {
        // for OPMODE_LONGEXPO, OPMODE_SEQUENTIAL, OPMODE_TIMER
        if ( core.op.opmode & OPMODE_TIMER )
        {
            core.op.op_state = OPSTATE_TIMER;
        }
        else
            core_internal_process_next_op();

        core_internal_calculate_total_time();
    }
    core_reset_second(NULL);
}

void core_start_bulb(void)
{
    core.op.op_state = OPSTATE_CABLEBULB;
    core.op.time_longexpo.hour = 0;
    core.op.time_longexpo.minute = 0;
    core.op.time_longexpo.second = 1;
}

void core_stop(void)
{
    if ( core.op.op_state == 0 )
        return;
    core.op.op_state = 0;
    HW_Shutter_Prefocus_OFF();
    HW_Shutter_Release_OFF();
    HW_Seconds_Restore();
}


void core_beep( enum EBeepSequence beep )
{
    if ( core.setup.beep_on )
    {
        BeepSequence( (uint32)beep );
    }
}


void core_analog_start( bool snd )
{
    if ( adc.adc_in_run )
        return;

    adc.base = ADC_BASE_UNINITTED;
    adc.adc_in_run = snd + 1;
    adc.armed = false;
    HW_PWR_An_On();
    HW_ADC_StartAcquisition( (uint32)adc.samples, snd );
}

void core_analog_stop(void)
{
    if ( adc.adc_in_run == false )
        return;
    adc.adc_in_run = 0;         // reset this first so if accidentally it enters in adc isr - this will terminate the execution
    HW_ADC_StopAcquisition();
    HW_ADC_ResetPretrigger();
    HW_PWR_An_Off();
}

// dbg purpose only
uint32 core_analog_getbuffer()
{
    return (uint32)(&adc);
}

static void core_analog_reset_minmax( void )
{
    adc.smplmin[0] = adc.samples[1];
    adc.smplmax[0] = adc.samples[1];
    adc.smplmin[1] = adc.samples[2];
    adc.smplmax[1] = adc.samples[2];
}


void core_analog_get_raws( uint16* raws )
{
    int i = 0;
    uint32 *ptr;
    uint16 *rptr;
    __disable_interrupt();
    raws[0] = (uint16)adc.samples[1];
    raws[1] = (uint16)adc.samples[2];
    i = 5;
    ptr = (uint32*)&adc.smplmin[0];
    rptr = raws + 2;
    while ( i-- )
    {
        *rptr = (uint16)(*ptr);
        rptr++;
        ptr++;
    }
    core_analog_reset_minmax();
    __enable_interrupt();
}

void core_analog_get_minmax( uint32* maxval, uint32* minval )
{
    if ( adc.adc_in_run == 0 )
        return;
    __disable_interrupt();
    *maxval = adc.vmax;
    *minval = adc.vmin;
    adc.vmax = -100;
    adc.vmin = 100;
    __enable_interrupt();
}

int core_init( struct SCore **instance )
{
    memset(&core, 0, sizeof(core));
    memset((void*)&adc, 0, sizeof(struct SADC) );
    *instance = &core;

    if ( eeprom_init() )
        goto _err_exit;
    if ( core_setup_load() )
        goto _err_exit;

    core.op.opmode = OPMODE_CABLE;
    core_update_battery();
    BeepSetFreq( core.setup.beep_low, core.setup.beep_hi );

    return 0;

_err_exit:
    return -1;


}


void core_poll( struct SEventStruct *evmask )
{
    // poll the started state
    if ( core.op.op_state )
    {
        // --- operations to do in bulk mode 
        


        // --- operations to do on 10ms intervals
        if ( evmask->timer_tick_10ms == 0 )
            return;

        // --- internal wait timer in progress
        if ( core.op.w_time )
        {
            core.op.w_time--;
            if ( core.op.w_time == 0 )
            {
                // internal wait terminated
                if ( core.op.op_state & ( OPSTATE_EXPO | OPSTATE_MLOCKUP ) )
                {
                    HW_Shutter_Release_OFF();       // turn off release
                    HW_Shutter_Prefocus_OFF();      // and prefocus
                }
                else if ( core.op.op_state & OPSTATE_EVENT )
                {
                    adc.armed = true;               // arm the trigger
                }

            }
        }
        // --- check if event is triggered ( not until the w_time for analog signal settling terminated )
        else if ( (core.op.op_state & OPSTATE_EVENT) && (adc.armed == false) )
        {
            // means that event produced
            if ( core.op.opmode & OPMODE_TIMER )            // if we need timer also then start in timer mode
            {
                core.op.op_state = OPSTATE_TIMER;
            }
            else
                core_internal_process_next_op();            // else proceed with exposure
            core_internal_calculate_total_time();
            core_reset_second( evmask );                    // consider this moment as beginning interval for seconds
        }

        // --- Operations on 1 second interval
        if ( evmask->timer_tick_1sec )      // 1 second tick
        {
            switch ( core.op.op_state )
            {
                case OPSTATE_TIMER:                     // waiting for timer
                    core.op.tmr_interval--;
                    if ( core.op.tmr_interval == 0 )
                    {
                        core_internal_process_next_op();
                    }
                    break;
                case OPSTATE_MLOCKUP:
                    core.op.expo--;
                    if ( core.op.expo == 0 )
                    {
                        core_internal_process_next_op();
                    }
                    break;
                case OPSTATE_EXPO:                      // exposing
                    if ( core.op.opmode & OPMODE_LONGEXPO )
                    {

                        if ( core.op.time_longexpo.second )
                            core.op.time_longexpo.second--;
                        else
                        {
                            core.op.time_longexpo.second = 59;
                            if ( core.op.time_longexpo.minute )
                                core.op.time_longexpo.minute--;
                            else
                            {
                                core.op.time_longexpo.minute = 59;
                                if ( core.op.time_longexpo.hour )
                                    core.op.time_longexpo.hour--;
                            }
                        }

                        core.op.expo--;
                        if ( core.op.expo == 0 )
                        {
                            if ( core_internal_process_after_expo() )
                                evmask->key_event = 1;  // wake up the ui
                        }
                    }
                    else
                    {
                        if ( core_internal_process_after_expo() )
                            evmask->key_event = 1;  // wake up the ui
                    }
                    break;
                case OPSTATE_WAIT_NEXT:
                    core.op.seq_interval--;
                    if ( core.op.seq_interval == 0 )
                    {
                        core_internal_process_next_op();
                    }
                    break;
                case OPSTATE_CABLEBULB:
                    core.op.time_longexpo.second++;
                    if ( core.op.time_longexpo.second == 60 )
                    {
                        core.op.time_longexpo.second = 0;
                        core.op.time_longexpo.minute++;
                        if ( core.op.time_longexpo.minute == 60 )
                        {
                            core.op.time_longexpo.minute = 0;
                            core.op.time_longexpo.hour++;
                        }
                    }
                    break;
            }

            if ( (core.op.op_state & ( OPSTATE_EXPO | OPSTATE_MLOCKUP | OPSTATE_TIMER | OPSTATE_WAIT_NEXT )) &&
                 core.op.op_time_left )
            {
                core.op.op_time_left--;
            }
        }
    }

    if ( evmask->timer_tick_1sec )
    {
        core.measure.batt_rdout++;
        if ( core.measure.batt_rdout >= 5)
        {
            core.measure.batt_rdout = 0;
            core_update_battery();
        }
    }

    // copy this in every loop
    core.measure.scaled_val = adc.value;

    return;
}


int  core_get_pwrstate()
{
    if ( core.op.op_state == 0 )                // if core is stopped we don't care about adc - ui should take care
        return SYSSTAT_CORE_STOPPED;
    if ( core.op.w_time || adc.adc_in_run )     // if core in run and doing 10ms timings or adc in run (waiting trigger or something) then always run with systimer on
        return SYSSTAT_CORE_RUN_FULL;
    else                                        // if core in run and no time critical operation (waiting for seconds) - it can run with 1sec. wake up timing
    return SYSSTAT_CORE_RUN_LOW;
}








