#ifndef __SENSORS_INTERNALS_H
#define __SENSORS_INTERNALS_H

#ifdef __cplusplus
    extern "C" {
#endif

    #include "stm32f10x.h"
    #include "typedefs.h"
    

    #define I2C_DEVICE_PRESSURE     0xC0            // pressure sensor's device address
    #define I2C_DEVICE_TH           0x80            // temperature and humidity sensor's device address
    
    #define REGPRESS_STATUS         0x00            // 1byte pressure sensor status register
    #define REGPRESS_OUTP           0x01            // 3byte barometric data + 2byte thermometric data - pressure data is in Pascales - 20bit: 18.2 from MSB. 
    #define REGPRESS_OUTT           0x04            // 2byte thermometric data - temperature in *C - 12bit: 8.4 from MSB
    #define REGPRESS_ID             0x0C            // 1byte pressure sensor chip ID
    #define REGPRESS_DATACFG        0x13            // 1byte Pressure data, Temperature data and event flag generator
    #define REGPRESS_BAR_IN         0x14            // 2byte (msb/lsb) Barometric input in 2Pa units for altitude calculations, default is 101,326 Pa.
    #define REGPRESS_CTRL1          0x26            // 1byte control register 1
    
    
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
    
    #define PREG_STATUS_PTOW        0x80            // set when pressure/temperature data is overwritten in OUTT or OUTP, cleared when REGPRESS_OUTP is read
    #define PREG_STATUS_POW         0x40            // set when pressure data is overwritten in OUTP, cleared when REGPRESS_OUTP is read
    #define PREG_STATUS_TOW         0x20            // set when temperature data is overwritten in OUTT, cleared when REGPRESS_OUTT is read
    #define PREG_STATUS_PTDR        0x08            // set when pressure/temperature data is updated in OUTT or OUTP, cleared when REGPRESS_OUTP is read
    #define PREG_STATUS_PDR         0x04            // set when pressure data is updated in OUTP, cleared when REGPRESS_OUTP is read
    #define PREG_STATUS_TDR         0x02            // set when temperature data is updated in OUTT, cleared when REGPRESS_OUTT is read
    
    #define PREG_DATACFG_DREM       0x04            // data reay event mode
    #define PREG_DATACFG_PDEFE      0x02            // event detection for new pressure data
    #define PREG_DATACFG_TDEFE      0x02            // event detection for new temperature data
    
    
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



#ifdef __cplusplus
    }
#endif

#endif
