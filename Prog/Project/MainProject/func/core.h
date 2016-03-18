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
    #include "hw_stuff.h"

    /*
     *
     */

    #define NUM100_MIN      0x20000000  // minimum below value error
    #define NUM100_MAX      0x40000000  // maximum abowe value error

    #define STORAGE_MINMAX      6
    #define STORAGE_TENDENCY    39      // 6'30" worth of data with 10sec sampling/averaging, 19*30' data with 30min sampling

    #define GET_MM_SET_SELECTOR( value, index )             ( ((value) >> (4*(index))) & 0x0f )
    #define SET_MM_SET_SELECTOR( selector, value, index )   do{  (selector) = ( (selector) & ~(0x0f << (4*(index))) ) | ( ((value) & 0x0f) << (4*(index)) ); } while (0)

    #define convert_press_20fp2_16bit( press )  ( ((press) >> 2) - 50000 ) 
    #define convert_press_16bit_20fp2( press )  ( ((uint32)(press)+50000) << 2 )

    #define CORE_SCHED_TEMP_REALTIME    2       // 2x RTC tick - 1sec. rate for temperature sensor
    #define CORE_SCHED_TEMP_MONITOR     10      // 5sec rate
    #define CORE_SCHED_RH_REALTIME      2
    #define CORE_SCHED_RH_MONITOR       10
    #define CORE_SCHED_PRESS_REALTIME   2       // 1 sec for realtime pressure monitor
    #define CORE_SCHED_PRESS_MONITOR    10      // 5sec for pressure monitoring
    #define CORE_SCHED_PRESS_STBY       120     // 1min for pressure readout in standby - combined with battery check
    #define CORE_SCHED_PRESS_ALTIMETER  0xffffffff  // process pressure read at fast rate 
    #define CORE_SCHED_BATTERY_CHRG     10      // 5sec interval for battery carge check (used in fast on-off cycle - minimum pwr consumption)

    #define CORE_SCHED_NONE             0xffffffff

    #define CORE_MMP_TEMP       0
    #define CORE_MMP_RH         1
    #define CORE_MMP_PRESS      3
    #define CORE_MMP_ABSH       2

    #define CORE_MSR_SET        4   // we track 4 parameters for monitoring (see CORE_MMP_xxx)

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

    enum EHumidityUnits
    {
        hu_rh = 0,                  // %RH
        hu_dew,                     // dew point (use units from temperature)
        hu_abs                      // absolute humidity in g/m3
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
        ut_60min                    // 39hr data
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
        uint8                   show_unit_hygro;    // humidity display selector
        uint8                   show_mm_press;      // show min/max set for pressure
        uint16                  show_mm_temp;       // show min/max set for temperature: selectors for displaying location 1,2,3:  ( ssm1 << 0 | mms2 << 4 | mms3 << 8 )
        uint16                  show_mm_hygro;      // show min/max set for hygrometer: selectors for displaying location 1,2,3:  ( ssm1 << 0 | mms2 << 4 | mms3 << 8 )

        uint8                   tim_tend_temp;      // tendency update rate for temperature - see enum EUpdateTimings for values
        uint8                   tim_tend_hygro;     // for hyrometer (RH / AbsH)
        uint8                   tim_tend_press;     // for pressure
    };

    union UCoreOperationFlags
    {
        struct {
            uint32  op_monitoring:1;
            uint32  op_registering:1;
            uint32  op_altimeter:1;
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

        uint32  sch_moni_temp;      // schedule temperature monitoring. 
        uint32  sch_moni_hygro;     // schedule humidity monitoring (RH / AbsH). 
        uint32  sch_moni_press;     // schedule pressure monitoring

        uint16  clk_last_day;       // saved day for checking if day border is passed
        uint16  clk_last_week;      // saved week for checking if week border is passed
    };

    struct SSchedules
    {
        uint32  sch_hygro;          // RTC schedule for the next hygro read. Use CORE_SCHED_NONE if disabled 
        uint32  sch_thermo;         // RTC schedule for the next temperature read. Use CORE_SCHED_NONE if disabled
        uint32  sch_press;          // RTC schedule for the next pressure read - if CORE_SCHED_PRESS_ALTIMETER - the sensor is set up for fast read-out
    };


    struct SMinimMaxim
    {
        uint16  min[STORAGE_MINMAX];    // Temperature: 16fp9 + 40*C,  RH in x100 %,  ABSH in x100 g/m3,  Pressure in Pa+50kPa  ( 110hpa -> 50hpa in 60k->0k value )
        uint16  max[STORAGE_MINMAX];    //                                                                working altitude: -500m -> 4500m
    };

    struct STendencyBuffer
    {
        uint16 value[STORAGE_TENDENCY]; // Temperature: 16fp9 + 40*C,  RH in x100 %,  ABSH in x100 g/m3,  Pressure in Pa+50kPa  ( 110hpa -> 50hpa in 60k->0k value )
        uint8   c;                      // data count                                                     working altitude: -500m -> 4500m
        uint8   w;                      // write pointer 
    };

    struct SSensorReads
    {
        struct SSensorReadAvgMonitoring moni;   // monitoring

        struct SMinimMaxim      minmax[CORE_MSR_SET];      // minimum and maximum values. See CORE_MMP_xxx for indexes.    96 bytes
        struct STendencyBuffer  tendency[CORE_MSR_SET];    // tendency value list. See CORE_MMP_xxx for indexes.           336 bytes
    };


    struct SCoreOperation           // core operations in nonvolatile space
    {
        union UCoreOperationFlags   op_flags;       // operation flags - see CORE_OP_XXX defines
        struct SSchedules           sched;          // sheduled events
        struct SSensorReads         sens_rd;        // sensor read operations
    };


    union UUIdirtybits
    {
        uint32  val;
        struct
        {
            uint32 upd_battery:1;          // battery measurement updated
            uint32 upd_temp:1;             // temperature measurement updated
            uint32 upd_temp_minmax:1;      // temperature minim/maxim updated
            uint32 upd_hygro:1;            // humidity value updated
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
        uint16  dewpoint;           // current dewpoint in 16fp9+40*C
        uint16  rh;                 // current humidity in x100 %
        uint16  absh;               // absolute humidity in x100 g/m3
        uint32  pressure;           // current barometric pressure in 20fp2 Pa
    };


    struct SCoreMeasure
    {
        union UUIdirtybits      dirty;      // flags for new values for user interface update
        struct SMeasurements    measured;   // currently measured and calculated values
        uint8       battery;                // 0 - 100 in % adjusted allready
        uint8       batt_rdout;             // seconds tick for battery value read out
    };


    struct SCoreNonVolatileData
    {
        bool                    dirty;      // set if nonvolatile structure is read out for use (no fast on/off)
        struct SCoreSetup       setup;      // setup
        struct SCoreOperation   op;         // operation
    };


    union UCoreNFStatus
    {
        uint16 val;
        struct
        {
            uint16 first_start:1;

        } b;
    };

    struct SCoreNonVolatileFast
    {
        union UCoreNFStatus     status;                 // 16bit - BKP2     - core status flags
        uint32                  next_schedule;          // 2x16bit - BKP3/4 - the next scheduled operation RTC counter
    };


    #define CORE_UISTATE_START_LONGPRESS    0x01    // wait long press for UI start-up
    #define CORE_UISTATE_SET_DATETIME       0x02    // order date-time setup from UI
    #define CORE_UISTATE_LOW_BATTERY        0x04    // order low batter UI alarm
    #define CORE_UISTATE_CHARGING           0x08    // order charging indication
    #define CORE_UISTATE_EECORRUPTED        0x10    // eeprom content is corrupted
    #define CORE_UISTATE_SENSOR_P_FAIL      0x20    // pressure sensor failure
    #define CORE_UISTATE_SENSOR_RH_FAIL     0x40    // RH/Temp sensor failure

    #define CORE_NVSTATE_UNINITTED          0x00    // NV ram is uninitted - data in core.nv is not set yet
    #define CORE_NVSTATE_PWR_RUNUP          0x01    // NV ram is activated - wait 1ms for start-up
    #define CORE_NVSTATE_OK_IDLE            0x02    // NV ram is in read and put in idle


    union UCoreInternalStatus
    {
        struct 
        {
            uint32  core_bsy:1;         // core operations in progress
            uint32  sched:1;            // scheduled time triggered - must look after scheduled operation
            uint32  sens_read:4;        // wait for sensor read - contains bitmask with the sensors

            uint32  first_run:1;        // first loop - reset afterward
            uint32  first_pwrup:1;      // first power-up - do not load NV operations from FRAM

            uint32  nv_initted:1;       // nonvolatile structure content initted
            uint32  nv_state:2;         // state of nonvolatile memory init

            uint32  sens_real_time:2;   // sensor in real time - see enum ESensorSelect

        } f;
        uint32 val;
    };


    struct SCoreVolatileStatus
    {
        uint32                      ui_order;       // orders made to UI module - set by core, read/processed/cleared by UI module
        union UCoreInternalStatus   int_op;         // internal operations
                                                    // 
    };



    struct SCore
    {
        struct SCoreNonVolatileData nv;         // nonvolatile data - it is written in FRAM at low power entry (unpowering system) and read at startup
        struct SCoreNonVolatileFast nf;         // nonvolatile data with fast access - holds operational status and such - limitted space in the battery backup domain

        struct SCoreVolatileStatus  vstatus;    // operational status in volatile domain (means - can be discarded when power is down)

        struct SCoreMeasure     measure;
    };


    int  core_init( struct SCore **instance );
    void core_poll( struct SEventStruct *evmask );

    // set up RTC alarm for the next operation after low power op.
    void core_pwr_setup_alarm( enum EPowerMode pwr_mode );
    // get the core's power state
    uint32 core_pwr_getstate(void);
    // measure battery
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
    int core_setup_load( bool no_op_load );
    void core_nvfast_save_struct(void);

    // the selected sensor will update it's readings in real time
    void core_op_realtime_sensor_select( enum ESensorSelect sensor );
    // enable or disable the monitoring feature. By disabling - all the tendency values will be cleared
    void core_op_monitoring_switch( bool enable );
    // sets the sample timing of the tendency monitoring. This will clear up the tendency graph of the affected sensor
    void core_op_monitoring_rate( enum ESensorSelect sensor, enum EUpdateTimings timing );
    // reset min/max value set for a specified sensor
    void core_op_monitoring_reset_minmax( enum ESensorSelect sensor, int mmset );



#ifdef __cplusplus
    }
#endif


#endif // CORE_H
