/*
        Sensor interface routines

        It works based on polling. Since I2C is slow, most of the operations 
        are done asynchronously so freqvent polling is needed

        Estimated i2c speed: 400kHz -> 40kbps - 25us/byte = 400clocks



*/

#include <stdio.h>

#include "sensors.h"
#include "hw_stuff.h"
#include "sensors_internals.h"
#include "i2c.h"


const uint8     set_baro[] = { REGPRESS_CTRL1, 0x38 };      // max oversample
const uint8     set_sshot_baro[] = { REGPRESS_CTRL1, ( pos_128 | PREG_CTRL1_OST) };
const uint8     set_bar_in[] = { REGPRESS_BAR_IN, 0xCC, 0xDD };
const uint8     set_data_event[] = { REGPRESS_DATACFG, PREG_DATACFG_PDEFE };

void Sensor_Init()
{
    uint8 data[16];

    // init I2C interface
    I2C_interface_init();

    // init DMA for I2C
    HW_DMA_Uninit( DMACH_SENS );
    HW_DMA_Init( DMACH_SENS );

}




/*
    local_I2C_device_write( I2C_DEVICE_PRESSURE, set_baro, sizeof(set_baro) );
    while ( local_I2C_busy() );
    local_I2C_device_write( I2C_DEVICE_PRESSURE, set_data_event, sizeof(set_data_event) );
    while ( local_I2C_busy() );
    local_I2C_device_write( I2C_DEVICE_PRESSURE, set_bar_in, sizeof(set_bar_in) );
    while ( local_I2C_busy() );
    local_I2C_device_write( I2C_DEVICE_PRESSURE, set_sshot_baro, sizeof(set_sshot_baro) );
    while ( local_I2C_busy() );
    
    
    local_I2C_device_read( I2C_DEVICE_PRESSURE, REGPRESS_STATUS, 1, data );
    while ( local_I2C_busy() );

    local_I2C_device_read( I2C_DEVICE_PRESSURE, REGPRESS_OUTP, 5, data+1 );
    while ( local_I2C_busy() );

    local_I2C_device_read( I2C_DEVICE_PRESSURE, REGPRESS_BAR_IN, 5, data+8 );
    while ( local_I2C_busy() );


    if ( data[3] == data[4] )
    {
        HW_DMA_Uninit( DMACH_SENS );
    }

    // dev experiments


    // read it's value


}

*/

void Sensor_Shutdown( uint32 mask )
{

}

void Sensor_Acquire( uint32 mask )
{

}

uint32 Sensor_Is_Ready(void)
{

    return 0;
}

uint32 Sensor_Is_Busy(void)
{

    return 0;
}

uint32 Sensor_Get_Value( uint32 sensor )
{

    return 0;
}

void Sensor_Poll(bool tick_ms)
{

}

