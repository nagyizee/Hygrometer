#ifndef __SENSORS_INTERNALS_H
#define __SENSORS_INTERNALS_H

#ifdef __cplusplus
    extern "C" {
#endif

    #include "stm32f10x.h"
    #include "typedefs.h"
    

    #define I2C_DEVICE_PRESSURE     0xC0            // pressure sensor's device address
    #define I2C_DEVICE_RH           0x80            // temperature and humidity sensor's device address
    
    #define REGPRESS_STATUS         0x00            // 1byte pressure sensor status register
    #define REGPRESS_OUTP           0x01            // 3byte barometric data + 2byte thermometric data - pressure data is in Pascales - 20bit: 18.2 from MSB. 
    #define REGPRESS_OUTT           0x04            // 2byte thermometric data - temperature in *C - 12bit: 8.4 from MSB
    #define REGPRESS_ID             0x0C            // 1byte pressure sensor chip ID
    #define REGPRESS_DATACFG        0x13            // 1byte Pressure data, Temperature data and event flag generator
    #define REGPRESS_BAR_IN         0x14            // 2byte (msb/lsb) Barometric input in 2Pa units for altitude calculations, default is 101,326 Pa.
    #define REGPRESS_CTRL1          0x26            // 1byte control register 1
    #define REGPRESS_CTRL3          0x28            // 1byte control register 3 - interrupt pin config
    #define REGPRESS_CTRL4          0x29            // 1byte control register 4 - interrupt enable register
    #define REGPRESS_CTRL5          0x2A            // 1byte control register 5 - interrupt cfg. register
    
    #define PREG_CTRL1_ALT          0x80            // SET: altimeter mode, RESET: barometer mode
    #define PREG_CTRL1_RAW          0x40            // SET: raw data output mode - data directly from sensor - The FIFO must be disabled and all other functionality: Alarms, Deltas, and other interrupts are disabled
    #define PREG_CTRL1_OSMASK       0x38            // 3bit oversample ratio - it is 2^x,  0 - means 1 sample, 7 means 128 sample, see enum EPressOversampleRatio
    #define PREG_CTRL1_RST          0x04            // SET: software reset
    #define PREG_CTRL1_OST          0x02            // SET: initiate a measurement immediately. If the SBYB bit is set to active, setting the OST bit will initiate an immediate measurement, the part will then return to acquiring data as per the setting of the ST bits in CTRL_REG2. In this mode, the OST bit does not clear itself and must be cleared and set again to initiate another immediate measurement. One Shot: When SBYB is 0, the OST bit is an auto-clear bit. When OST is set, the device initiates a measurement by going into active mode. Once a Pressure/Altitude and Temperature measurement is completed, it clears the OST bit and comes back to STANDBY mode. User shall read the value of the OST bit before writing to this bit again
    #define PREG_CTRL1_SBYB         0x01            // SET: sets the active mode. system makes periodic measurements set by ST in CTRL2 register.
    #define PREG_GET_OS( a )        ( (a) & PREG_CTRL1_OSMASK )
    #define PREG_SET_OS( a, b )     do {                                            \
                                        ( (a) &= ~PREG_CTRL1_OSMASK;                \
                                        ( (a) |= ( (b) & PREG_CTRL1_OSMASK )        \
                                    while ( 0 ) 

    #define PREG_CTRL3_IPOL1        0x20            // SET: INT1 pin active high
    #define PREG_CTRL3_PPOD1        0x10            // SET: open drain output
    #define PREG_CTRL3_IPOL2        0x02            // SET: INT2 pin active high
    #define PREG_CTRL3_PPOD2        0x01            // SET: open drain output

    #define PREG_CTRL4_DRDY         0x80            // SET: enable data ready interrupt

    #define PREG_CTRL5_DRDY         0x80            // SET: data ready interrupt routed to INT1, RESET: routed to INT2 pin
    
    #define PREG_STATUS_PTOW        0x80            // set when pressure/temperature data is overwritten in OUTT or OUTP, cleared when REGPRESS_OUTP is read
    #define PREG_STATUS_POW         0x40            // set when pressure data is overwritten in OUTP, cleared when REGPRESS_OUTP is read
    #define PREG_STATUS_TOW         0x20            // set when temperature data is overwritten in OUTT, cleared when REGPRESS_OUTT is read
    #define PREG_STATUS_PTDR        0x08            // set when pressure/temperature data is updated in OUTT or OUTP, cleared when REGPRESS_OUTP is read
    #define PREG_STATUS_PDR         0x04            // set when pressure data is updated in OUTP, cleared when REGPRESS_OUTP is read
    #define PREG_STATUS_TDR         0x02            // set when temperature data is updated in OUTT, cleared when REGPRESS_OUTT is read
    
    #define PREG_DATACFG_DREM       0x04            // data reay event mode
    #define PREG_DATACFG_PDEFE      0x02            // event detection for new pressure data
    #define PREG_DATACFG_TDEFE      0x01            // event detection for new temperature data


    #define REGRH_USER_READ         0xE7            // read user register
    #define REGRH_USER_WRITE        0xE6            // write user register
    #define REGRH_TRIG_TEMP         0xF3            // trigger temperature measurement
    #define REGRH_TRIG_RH           0xF5

    #define RHREG_USER_RESMASK      0x81            // mask thr resolution in user register, see enum ERHresolution
    #define RHREG_USER_RDSTATUS     0x40            // power status - read 1: Vdd<2.25V
    #define RHREG_USER_CHIPHEATER   0x04            // on chip heater on
    #define RHREG_USER_NO_OTP_REL   0x02            // disable OTP reload

    
    enum EPressOversampleRatio
    {                           // minimum times between data samples:
        pos_none = 0x00,        // 6ms
        pos_2 = 0x08,           // 10ms
        pos_4 = 0x10,           // 18ms
        pos_8 = 0x18,           // 34ms
        pos_16 = 0x20,          // 66ms
        pos_32 = 0x28,          // 130ms
        pos_64 = 0x30,          // 258ms
        pos_128 = 0x38          // 512ms
    };

    enum ERHresolution
    {
        rhres_12_14 = 0x00,     // RH:12bit T:14bit
        rhres_8_12 = 0x01,      // RH:8bit  T:12bit
        rhres_10_13 = 0x80,     // RH:10bit T:13bit
        rhres_11_11 = 0x81      // RH:11bit T:11bit
    };
    
    enum ESensorsOpState
    {
        ss_none = 0,
        ss_initp_set_datacfg,       // init pressure sensor - setting up data config event signalling

    };

    enum ESensorBusStatus
    {
        busst_none = 0,
        busst_pressure,
        busst_rh,
    };

    enum EPessureSensorStateMachine
    {
        psm_none = 0,

        psm_init_01,        // init phase - data event setup sent, wait for completion
        psm_init_02,        // init phase
        psm_init_03,        // init phase
        psm_init_04,        // init phase

        psm_read_oneshotcmd,        // read phase - one shot command sent, wait for completion
        psm_read_waitevent,         // read phase - read status register - wait for result
        psm_read_waitresult,        // read phase - read the pressure data
    };

    enum ERHsensorStateMachine
    {
        rhsm_none = 0,

        rhsm_init_01_wait_powerup,      // init phase - wait 15ms for the sensor to power-up (usually init is called after power-up)
        rhsm_init_02_read_user_reg,
        rhsm_init_03_write_user_reg,

        rhsm_readrh_01_send_request,    // read phase - sent RH measurement request
        rhsm_readt_01_send_request,     // read phase - sent Temp measurement request
        rhsm_readrh_02_wait4read,       // read phase - time delay terminated, read polled, waiting result
        rhsm_readt_02_wait4read,        // read phase - time delay terminated, read polled, waiting result
    };

    struct SSensorStatus
    {
        uint32  initted_p:1;            // pressure sensor initted
        uint32  initted_rh:1;           // RH/T sensor initted
        uint32  failed_p:1;             // pressure sensor failure
        uint32  failed_rh:1;            // RH/T sensor failure

        uint32  sensp_ini_request:1;    // request for pressure sensor init
        uint32  sensrh_ini_request:1;   // request for RH sensor init
                                        
    };


    struct SPressureSensorStatus
    {
        enum EPessureSensorStateMachine sm;
        uint16                          check_ctr;      // time counter for polling period
        uint16                          fail_ctr;       // failure retrial counter
        uint8                           hw_read_val[4]; // read value from the sensor in i2c
    };

    struct SRHSensorStatus
    {
        enum ERHsensorStateMachine  sm;
        uint16                      to_ctr;         // time out counter
        uint16                      fail_ctr;       // failure retrial counter
        uint8                       hw_read_val[4];
    };

    struct SSensorHardware
    {
        enum ESensorBusStatus           bus_busy;   
        struct SPressureSensorStatus    psens;
        struct SRHSensorStatus          rhsens;
    };


    struct SSensorValues
    {
        uint32  pressure;
        uint16  temp;
        uint16  rh;
    };

    struct SSensorQuickFlags
    {
        uint8                       sens_pwr;
        uint8                       sens_busy;
        uint8                       sens_ready;
        uint8                       sens_fail;
    };

    struct SSensorsStruct
    {
        enum ESensorsOpState        sm;         // state machine status
        struct SSensorStatus        status;     // status of sensor setup / acquire / etc.
        struct SSensorHardware      hw;         // sensor hardware layer    
        struct SSensorValues        measured;   // measured values, entries are valid only if _data_ready flags are set
        struct SSensorQuickFlags    flags;      // quick access flags for the sensors - bitmask lists
    };
    




#ifdef __cplusplus
    }
#endif

#endif
