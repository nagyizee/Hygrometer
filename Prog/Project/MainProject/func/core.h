/** Core routines:
 *
 *
 */


#ifndef CORE_H
#define CORE_H

#ifdef __cplusplus
    extern "C" {
#endif


    #include "stm32f10x.h"
    #include "typedefs.h"
    #include "events_ui.h"

    #define OPMODE_CABLE        0x01
    #define OPMODE_LONGEXPO     0x02
    #define OPMODE_SEQUENTIAL   0x04
    #define OPMODE_TIMER        0x08
    #define OPMODE_LIGHT        0x10
    #define OPMODE_SOUND        0x20

    #define OPMODE_COMBINED     0x8000

    #define OPSTATE_STOPPED     0x00
    #define OPSTATE_MLOCKUP     0x01    // wait for mirror lockup.
    #define OPSTATE_TIMER       0x02    // wait for timer
    #define OPSTATE_EXPO        0x04    // exposure (if long expo - count down in seconds, otherwise 1/5 second pulse)
    #define OPSTATE_WAIT_NEXT   0x08    // wait for next operation (used for sequential shooting)
    #define OPSTATE_EVENT       0x10    // wait for event (light or sound trigger)
    #define OPSTATE_CABLEBULB   0x20    // executing cable release bulb mode
    #define OPSTATE_CABLE       0x40    // cable release simple mode

    /*
     * Valid modes for combinations:
     * Notations:
     *      L - long expo
     *      S - sequential shots
     *      T - timer
     *      I - lightning
     *      V - sound
     *
     *  - LS    - sequential shots with long expo. seq.iv. is time between closed shutter. Ttot = seq.nr * ( le.time + seqiv.time )
     *            sequence starts with [L], continue with [S.iv], cycle for [S.nr]
     *  - LT    - long expo which starts after timer interval. Ttot = le.time + tmr
     *            sequence starts with [T], countinues with [L]
     *  - LI    - long expo started with lighning trigger, retrigger only after expo is ready. Ttot = le.time
     *            sequence starts with [I], continues with [L]
     *  - LV    - same as abowe
     *            sequence starts with [V], continues with [L]
     *  - LST   - sequential shots with long expo after timer. Ttot = seq.nr * ( le.time + seqiv.time ) + tmr
     *            sequence starts with [T], continues in:  [L], continued with [S.iv], cycle for [S.nr]
     *  - TI    - light triggered shot after timer period. Ttot = tmr.
     *  - TV    - sound triggered shot after timer period. Ttot = tmr.
     *  - LTI   - light triggered long expo after timer period. Ttot = tmr + le.time.
     *  - LTV   - sound triggered long expo after timer period. Ttot = tmr + le.time.
     *  - LSI/V - light or sound triggered sequential long expos
     *  - LSTI/V- light or sound triggered sequential long expos with timer startup
     *
     */

    enum EBeepSequence
    {
        beep_pwron = 0x2e,          // .=  ->  110 101      ->      10 1110
        beep_pwroff = 0x1f6,        // ..- ->  110 110 111  ->  1 1111 0110
        beep_expo_start = 0x1b6,    // ... ->  110 110 110  ->  1 1011 0110
        beep_expo_tick = 0x06,      // .   ->  110
        beep_expo_end = 0x07,       // -   ->  111
    };
        

    struct SADC
    {
        uint32 samples[4];      // DMA buffer - Channel indexes: 0 - Battery,  1 - LowGain,  2 - HighGain,  3 - Temperature
        uint32 smplmin[2];      // sample minimums for active channel
        uint32 smplmax[2];      // sample maximums for active channel

        uint32 base;            // mobile base
        int32  value;           // calculated value after filtering, offsetting and adding gain
        int32  filter_dly;      // delay element in the low pass filter
        int32  vmin;            // minimum value
        int32  vmax;            // maximum value

        bool armed;             // armed for trigger shutter on event - set by application, reset by isr when triggered
        int adc_in_run;         // 0 - not in run, 1 - light, 2 - sound ( controlled by application - checked by isr )
        int channel_lock;       // 0 - no channel lock, 1 / 2 - lock low or high gain channel for debug purpose
    };

    struct SCoreSetLight
    {
        uint8   gain;           // sensitivity gain         5 - 100%
        uint8   thold;          // trigger threshold        0 - 50
        uint8   trig_h;         // trigger on ligther
        uint8   trig_l;         // trigger on darker
        bool    bright;         // use amp stage 2 for bright light condition
        uint32  iv_pretrig;     // pretrigger interval in 100us units
    };

    struct SCoreSetSound
    {
        uint16  iv_pretrig;     // pretrigger interval in 100us units
        uint8   gain;           // sensitivity gain
        uint8   thold;          // trigger threshold
    };



    struct SCoreSetup
    {
        timestruct              time_longexpo;
        uint16                  seq_shotnr;
        uint16                  seq_interval;       // interval in seconds between shots
        uint16                  tmr_interval;       // interval in seconds
        struct SCoreSetLight    light;
        struct SCoreSetSound    sound;
        uint8                   mlockup;            // if non-0 then mirror lockup is in use
        uint8                   disp_brt_on;        // display brightness on full power  ( 0x00 - 0x40 )
        uint8                   disp_brt_dim;       // display brightness on dimmed state
        uint8                   disp_brt_auto;      // use the light sensor to set up display brightness - when selected then the max brightness value is dowscaled as light diminishes
        uint16                  pwr_stdby;          // time for standby mode. - idle state, low ui activity, display dimmed 
        uint16                  pwr_disp_off;       // time for display off. must > t.stdby, display is turned off.
        uint16                  pwr_off;            // time for auto power off.
        uint16                  shtr_pulse;         // shutter pulse length in x10 ms
        uint32                  beep_on;            // beep in use
        uint16                  beep_hi;            // high pitch
        uint16                  beep_low;           // low pitch
    };





    struct SCoreOperation
    {
        uint32      opmode;             // bitmask with the selected operation modes
        uint32      op_state;           // operation state
        uint32      w_time;             // wait time in 10ms units (used for mirror lockup, wait states, etc)

        uint32      op_time_left;       // time left in seconds (displayed on the status bar in ui)

        timestruct  time_longexpo;      // long expo in time format
        uint32      expo;               // long expo in seconds
        uint16      seq_shotnr;         // nr of shots to be takes, if 0 then it takes infinite shots, for fixed shot nr. minimum value is 2
        uint16      seq_interval;       // interval between shots in seconds, for normal expo this is the iv. between each shot,
                                        //      in case of long expo, it is the calculated iv. between shots from finishing one and starting the new one
        uint16      tmr_interval;       // timer interval for taking the first shot
    };



    struct SCoreMeasure
    {
        uint8       battery;            // 0 - 100 in % adjusted allready
        uint8       batt_rdout;         // seconds tick for battery value read out
        int8        scaled_val;         // scaled value to +/-100 for the measured channel (light or sound)
    };



    struct SCore
    {
        struct SCoreSetup       setup;
        struct SCoreOperation   op;
        struct SCoreMeasure     measure;
    };

    int  core_init( struct SCore **instance );
    void core_poll( struct SEventStruct *evmask );
    int  core_get_pwrstate();
    void core_update_battery();

    // save / load / reset setup
    int core_setup_save( void );
    int core_setup_reset( bool save );
    int core_setup_load( void );

    //
    void core_reset_longexpo(void);
    void core_reset_sequential(void);
    void core_reset_timer(void);

    void core_start(void);
    void core_start_bulb(void);             // valid only for cable release bulb mode
    void core_stop(void);

    void core_beep( enum EBeepSequence beep );

    void core_analog_start( bool snd );     // if snd = 1 - acquisition is started for sound else for light
    void core_analog_stop(void);
    uint32 core_analog_getbuffer();
    void core_analog_get_raws( uint16* raws );  // raws[7]:  0-low, 1-high, 2-lowmin, 3-highmin, 4-lowmax, 5-highmax, 6-base
    void core_analog_get_minmax( uint32* maxval, uint32* minval );  // get the minimums and maximums for the selected channel

#ifdef __cplusplus
    }
#endif


#endif // CORE_H
