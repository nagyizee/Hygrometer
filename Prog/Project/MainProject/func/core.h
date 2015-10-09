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

    /*
     *
     */


    #define TEMP_FP         9           // use 16bit fixpoint at 9 bits for temperature


    #define NUM100_MIN      0x20000000  // minimum below value error
    #define NUM100_MAX      0x40000000  // maximum abowe value error

    #define STORAGE_MINMAX  6

    #define GET_MM_SET_SELECTOR( value, index )     ( (value >> (4*index)) & 0x0f )

    enum EBeepSequence
    {
        beep_pwron = 0x2e,          // .=  ->  110 101      ->      10 1110
        beep_pwroff = 0x1f6,        // ..- ->  110 110 111  ->  1 1111 0110
        beep_expo_start = 0x1b6,    // ... ->  110 110 110  ->  1 1011 0110
        beep_expo_tick = 0x06,      // .   ->  110
        beep_expo_end = 0x07,       // -   ->  111
    };

    enum ETemperatureUnits
    {
        ut_C = 0,                   // *C
        ut_F,                       // *F
        ut_K                        // *K
    };

    enum EMinimumMaximumStorage     // NOTE: keep the nr of elements in sync with STORAGE_MINMAX
    {
        mms_set1 = 0,               // generic location 1
        mms_set2,                   // generic location 2
        mms_day_crt,                // min/max this day
        mms_week_crt,               // min/max this week
        mms_day_bfr,                // min/max last day
        mms_week_bfr                // min/max last week
    };  


    struct SCoreSetup
    {
        uint8                   disp_brt_on;        // display brightness on full power  ( 0x00 - 0x40 )
        uint8                   disp_brt_dim;       // display brightness on dimmed state
        uint16                  pwr_stdby;          // time for standby mode. - idle state, low ui activity, display dimmed 
        uint16                  pwr_disp_off;       // time for display off. must > t.stdby, display is turned off.
        uint16                  pwr_off;            // time for auto power off.
        uint32                  beep_on;            // beep in use
        uint16                  beep_hi;            // high pitch
        uint16                  beep_low;           // low pitch
                                                    // 
        uint8                   show_unit_temp;     // see ETemperatureUnits for values
        uint8                   show_mm_press;      // show min/max set for pressure
        uint16                  show_mm_temp;       // show min/max set for temperature: selectors for displaying location 1,2,3:  ( ssm1 << 0 | mms2 << 4 | mms3 << 8 )
        uint16                  show_mm_hygro;      // show min/max set for hygrometer: selectors for displaying location 1,2,3:  ( ssm1 << 0 | mms2 << 4 | mms3 << 8 )
    };


    struct SCoreOperation
    {
        uint32      RTC_clock;          // clock counter in 0.5sec. Start date is 2012-Ian-01 00:00:00.0, Max range is 49710 days - 136 years

    };


    union UUIdirtybits
    {
        uint32  val;
        struct
        {
            uint32 upd_battery:1;          // battery measurement updated
            uint32 upd_temp:1;             // temperature measurement updated
            uint32 upd_temp_minmax:1;      // temperature minim/maxim updated
            uint32 upd_hum:1;              // humidity value updated
            uint32 upd_hum_minmax:1;       // humidity min/max values updated
            uint32 upd_abshum_minmax:1;    // absolute humidity min/max values updated
            uint32 upd_th_tendency:1;      // updated tendency value set ( 39 values for temperature/humidity - averaged between samples, shifted at update )
            uint32 upd_pressure:1;         // barometric pressure updated
            uint32 upd_press_minmax:1;     // updated min/max values
            uint32 upd_press_tendency:1;   // updated tendency value set
        } b;
    };

    struct SMeasurements
    {
        uint16  temperature;        // current temperature in 16fp9 + 40*C base. 0x0000 - means low error, 0xFFFF - means high error
        int16   dewpoint;           // current dewpoint in x100 *C units
        uint16  rh;                 // current humidity in x100 %
        uint16  absh;               // absolute humidity in x100 g/m3
        uint32  pressure;           // current barometric pressure in x100 hPa
        
    };

    struct SMinimMaxim              // 96 bytes
    {
        uint16  temp_min[STORAGE_MINMAX];   // minimum temperature values in 16fp9 + 40*C
        uint16  temp_max[STORAGE_MINMAX];   // maximum temperature values in 16fp9 + 40*C
        uint16  rh_min[STORAGE_MINMAX];
        uint16  rh_max[STORAGE_MINMAX];
        uint16  absh_min[STORAGE_MINMAX];
        uint16  absh_max[STORAGE_MINMAX];
        uint16  press_min[STORAGE_MINMAX];
        uint16  press_max[STORAGE_MINMAX];
    };


    struct SCoreMeasure
    {
        union UUIdirtybits      dirty;      // flags for new values for user interface update
        struct SMeasurements    measured;   // currently measured and calculated values
        struct SMinimMaxim      minmax;     // minimum and maximum values
        uint8       battery;                // 0 - 100 in % adjusted allready
        uint8       batt_rdout;             // seconds tick for battery value read out
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

    // convert 16fp9 temperature to the given unit in x100 integer format
    int core_utils_temperature2unit( uint16 temp16fp9, enum ETemperatureUnits unit );


    uint32 core_get_clock_counter();
    void core_set_clock_counter( uint32 counter );

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
