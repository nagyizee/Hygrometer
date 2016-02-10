/**
  ******************************************************************************
  * 
  *   Sensor handling routines
  *   Used for acquiring measurements from the thermo/hygro and
  *   barometer sensors
  * 
  ******************************************************************************
  */ 

#ifndef __SENSORS_H
#define __SENSORS_H

#ifdef __cplusplus
    extern "C" {
#endif

    #include "stm32f10x.h"
    #include "typedefs.h"
    
    
    #define SENSOR_TEMP     0x01
    #define SENSOR_RH       0x02
    #define SENSOR_PRESS    0x04
    
    #define SENSOR_VALUE_FAIL 0xffffffff

    #define SENSPWR_FULL    0x02        // sensors module request full processing power for quick poll
    #define SENSPWR_SLEEP   0x01        // sensors module tolerates sleep/w timer interrupt 
    #define SENSPWR_FREE    0x00        // sensors module is not processing - can do stop or power down
    
    // init sensor module
    void Sensor_Init();
    // shut down individual sensor block ( RH and temp are in one - they need to be provided in pair )
    void Sensor_Shutdown( uint32 mask );
    // set up acquire request on one or more sensors ( it will wake up the sensor if needed )
    void Sensor_Acquire( uint32 mask );
    // check the sensor state - returning a mask with the sensors which have read out value
    uint32 Sensor_Is_Ready(void);
    // check the sensor state - returning a mask busy sensors
    uint32 Sensor_Is_Busy(void);
    // get the acquired value from the sensor (returns the base formatted value from a sensor - Temp: 16fp9+40*, RH: 16fp8, Press: 18.2 pascals) and clears the ready flag
    uint32 Sensor_Get_Value( uint32 sensor );
    // sensor submodule polling
    void Sensor_Poll(bool tick_ms);
    // get sensors module power status
    uint32 Sensor_GetPwrStatus(void);

#ifdef __cplusplus
    }
#endif

#endif
