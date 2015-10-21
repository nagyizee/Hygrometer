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
    #define RH_FP           8

    #define NUM100_MIN      0x20000000  // minimum below value error
    #define NUM100_MAX      0x40000000  // maximum abowe value error

    #define STORAGE_MINMAX      6
    #define STORAGE_TENDENCY    39      // 6'30" worth of data with 10sec sampling/averaging

    #define GET_MM_SET_SELECTOR( value, index )     ( (value >> (4*index)) & 0x0f )


    #define CORE_SCHED_TEMP_REALTIME    2       // 2x RTC tick - 1sec. rate for temperature sensor
    #define CORE_SCHED_TEMP_MONITOR     5       // 2.5sec rate
    #define CORE_SCHED_RH_REALTIME      2
    #define CORE_SCHED_RH_MONITOR       5
    #define CORE_SCHED_PRESS_REALTIME   1       // 0.5 sec for realtime pressure monitor
    #define CORE_SCHED_PRESS_MONITOR    10      // 5sec for pressure monitoring
    #define CORE_SCHED_PRESS_ALTIMETER  0xffffffff  // process pressure read at fast rate 


    enum ESensorSelect
    {
        ss_none = 0,
        ss_thermo,
        ss_rh,
        ss_pressure
    };

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
        tu_C = 0,                   // *C
        tu_F,                       // *F
        tu_K                        // *K
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

    enum EUpdateTimings
    {
        ut_5sec = 0,                // 3'15" data
        ut_10sec,                   // 6'30" data
        ut_30sec,                   // 19'30" data
        ut_1min,                    // 39" data
        ut_2min,                    // 1.3hr data
        ut_5min,                    // 3.25hr data
        ut_10min,                   // 6.5hr data
        ut_30min,                   // 19.5hr data
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

        uint8                   show_unit_temp;     // see ETemperatureUnits for values
        uint8                   show_mm_press;      // show min/max set for pressure
        uint16                  show_mm_temp;       // show min/max set for temperature: selectors for displaying location 1,2,3:  ( ssm1 << 0 | mms2 << 4 | mms3 << 8 )
        uint16                  show_mm_hygro;      // show min/max set for hygrometer: selectors for displaying location 1,2,3:  ( ssm1 << 0 | mms2 << 4 | mms3 << 8 )

        uint8                   tim_tend_temp;      // tendency update rate for temperature - see enum EUpdateTimings for values
    };

    union UCoreOperationFlags
    {
        struct {
            uint32  op_monitoring:1;
            uint32  op_registering:1;
            uint32  op_altimeter:1;
            uint32  sens_real_time:2;               // sensor in real time - see enum ESensorSelect
            uint32  check_sensor:3;                 // check if result is arrived from a sensor (use it as a mask with SENSOR_XXX defines)
        } b;
        uint32 val;
    };

    struct SSensorReadAvgMonitoring
    {
        uint16  avg_ctr_temp;       // count the averages for temperature
        uint16  avg_ctr_hygro;      // count the averages for RH and absolute humidity
        uint16  avg_ctr_press;      // count the averages for pressure

        uint32  avg_sum_temp;       // sum of values for temperature
        uint32  avg_sum_rh;         // sum of values for rh
        uint32  avg_sum_abshum;     // sum of values for absolute humidity
        uint32  avg_sum_press;      // sum of values for pressure

        uint32  sch_moni_temp;
    };

    struct SSensorRead
    {
        uint32  sch_hygro;          // RTC schedule for the next hygro read
        uint32  sch_thermo;         // RTC schedule for the next temperature read
        uint32  sch_press;          // RTC schedule for the next pressure read - if CORE_SCHED_PRESS_ALTIMETER - the sensor is set up for fast read-out

        struct SSensorReadAvgMonitoring moni;   // monitoring
    };

    struct SCoreOperation
    {
        union UCoreOperationFlags   op_flags;       // operation flags - see CORE_OP_XXX defines
        struct SSensorRead          sread;          // sensor read 
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

    struct STendencyBuffer
    {
        uint16 value[STORAGE_TENDENCY];
        uint8   c;                      // data count
        uint8   w;                      // write pointer 
    };

    struct STendencyValues
    {
        struct STendencyBuffer temp;
        struct STendencyBuffer RH;
        struct STendencyBuffer abshum;
        struct STendencyBuffer press;
    };

    struct SCoreMeasure
    {
        union UUIdirtybits      dirty;      // flags for new values for user interface update
        struct SMeasurements    measured;   // currently measured and calculated values
        struct SMinimMaxim      minmax;     // minimum and maximum values
        struct STendencyValues  tendency;   // tendency value list
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
    int     core_utils_temperature2unit( uint16 temp16fp9, enum ETemperatureUnits unit );
    uint32  core_utils_unit2temperature( int temp100, enum ETemperatureUnits unit );

    // gets or sets the RTC clock
    uint32 core_get_clock_counter();
    void core_set_clock_counter( uint32 counter );

    // generate beeps
    void core_beep( enum EBeepSequence beep );

    // save / load / reset setup in eeprom
    int core_setup_save( void );
    int core_setup_reset( bool save );
    int core_setup_load( void );

    // the selected sensor will update it's readings in real time
    void core_op_realtime_sensor_select( enum ESensorSelect sensor );
    // enable or disable the monitoring feature. By disabling - all the tendency values will be cleared
    void core_op_monitoring_switch( bool enable );
    // sets the sample timing of the tendency monitoring. This will clear up the tendency graph of the affected sensor
    void core_op_monitoring_rate( enum ESensorSelect sensor, enum EUpdateTimings timing );



#ifdef __cplusplus
    }
#endif


#endif // CORE_H
