/*
        Sensor interface routines

        It works based on polling. Since I2C is slow, all the operations 
        are done asynchronously so freqvent polling is needed

        Estimated i2c speed: 400kHz -> 40kbps - 25us/byte = 400clocks



*/

#include <stdio.h>

#include "sensors.h"
#include "hw_stuff.h"
#include "sensors_internals.h"
#include "i2c.h"


//const uint8     psens_set_01_initial_st[] = { REGPRESS_CTRL1, pos_128 };                  // set initially with multisampling, but do not start it
const uint8     psens_set_02_data_event[] = { REGPRESS_DATACFG, (PREG_DATACFG_TDEFE | PREG_DATACFG_PDEFE | PREG_DATACFG_DREM) };     // set up data event signalling for pressure update
const uint8     psens_set_03_interrupt_src[]  = { REGPRESS_CTRL3, PREG_CTRL3_IPOL1 };       // pushpull active high on INT1
const uint8     psens_set_04_interrupt_en[]  = { REGPRESS_CTRL4, PREG_CTRL4_DRDY };         // enable data ready interrupt
const uint8     psens_set_05_interrupt_out[]  = { REGPRESS_CTRL5, PREG_CTRL5_DRDY };        // route data ready interrupt to INT1

const uint8     psens_cmd_sshot_baro[] = { REGPRESS_CTRL1, ( pos_128 | PREG_CTRL1_OST) };      // start one shot data aq. with 64 sample oversampling (~512ms wait time)


struct SSensorsStruct ss;


void local_i2c_reinit(void)
{
    // init I2C interface
    I2C_interface_init();
    // init DMA for I2C
    HW_DMA_Uninit( DMACH_SENS );
    HW_DMA_Init( DMACH_SENS );

}


void local_psensor_execute_ini( bool mstick )
{
    // no need to check for uninitted state - taken care at the initiator routine

    if ( ss.hw.bus_busy == busst_rh )       // bus is busy with the RH sensor - not part of this subsystem - exit
        return;
        
    if ( ss.hw.bus_busy )
    {
        uint32 result;
        result = I2C_busy();
        if ( result == I2CSTATE_BUSY )
            return;
        if ( result == I2CSTATE_FAIL )
        {
            if ( I2C_errorcode() == I2CFAIL_SETST )
                goto _i2c_failure;
            goto _failure;
        }

        switch ( ss.hw.psens.sm )
        {
            case psm_init_01:
                if ( I2C_device_write( I2C_DEVICE_PRESSURE, psens_set_03_interrupt_src, sizeof(psens_set_03_interrupt_src) ) )
                    goto _i2c_failure;
                ss.hw.psens.sm = psm_init_02;
                break;
            case psm_init_02:
                if ( I2C_device_write( I2C_DEVICE_PRESSURE, psens_set_04_interrupt_en, sizeof(psens_set_04_interrupt_en) ) )
                    goto _i2c_failure;
                ss.hw.psens.sm = psm_init_03;
                break;
            case psm_init_03:
                if ( I2C_device_write( I2C_DEVICE_PRESSURE, psens_set_05_interrupt_out, sizeof(psens_set_05_interrupt_out) ) )
                    goto _i2c_failure;
                ss.hw.psens.sm = psm_init_05;
                break;
/*            case psm_init_04:
                if ( I2C_device_write( I2C_DEVICE_PRESSURE, psens_set_05_interrupt_out, sizeof(psens_set_05_interrupt_out) ) )
                    goto _i2c_failure;
                ss.hw.psens.sm = psm_init_05;
                break;
  */          case psm_init_05:
                if ( I2C_device_write( I2C_DEVICE_PRESSURE, psens_cmd_sshot_baro, sizeof(psens_cmd_sshot_baro) ) )
                    goto _i2c_failure;
                ss.hw.psens.sm = psm_init_06;
                break;
            case psm_init_06:
                ss.hw.bus_busy = busst_none;                // bus is free
                ss.hw.psens.sm = psm_none;                  // no operation on sensor
                ss.status.sensp_ini_request = 0;            // ini request served
                ss.status.initted_p = 1;                    // sensor initted
                ss.flags.sens_fail &= ~SENSOR_PRESS;
                ss.flags.sens_pwr = SENSPWR_FREE;
                break;
        }
    }
    else
    {
        // bus is free - send the first command (bus_busy is cleared only when execution is finished)
        if ( I2C_device_write( I2C_DEVICE_PRESSURE, psens_set_02_data_event, sizeof(psens_set_02_data_event) ) == I2CSTATE_NONE )
        {
            ss.hw.bus_busy = busst_pressure;    // mark bus busy
            ss.hw.psens.sm = psm_init_01;       // mark the current operation state
            ss.flags.sens_pwr = SENSPWR_FULL;
        }
        else
            goto _i2c_failure;
    }
    return;

_i2c_failure:
    local_i2c_reinit(); 
_failure:
    ss.hw.bus_busy = busst_none;            // mark bus free, routine will retry the command
    ss.hw.psens.sm = psm_none;
    ss.flags.sens_pwr = SENSPWR_FREE;
}

#define PSENS_READ_POLLING  5

void local_psensor_execute_read( bool tick_ms )
{
    // no need to check for uninitted state - taken care at the initiator routine
return;
    // if one-shot cmd is sent - increase the counter on every 1ms
    if ( tick_ms && (ss.hw.psens.sm == psm_read_oneshotcmd) )
    {
        if ( ss.hw.psens.check_ctr < PSENS_READ_POLLING )
            ss.hw.psens.check_ctr++;
    }
    // bus is busy with the RH sensor - not part of this subsystem - exit
    if ( ss.hw.bus_busy == busst_rh )       
        return;

    // if bus is busy with the pressure sensor - proceed the next state if i2c transaction is terminated
    if ( ss.hw.bus_busy )
    {
        uint32 result;
        result = I2C_busy();
        if ( result == I2CSTATE_BUSY )
            return;
        if ( result == I2CSTATE_FAIL )
        {
            if ( I2C_errorcode() == I2CFAIL_SETST )
                goto _i2c_failure;
            goto _failure;
        }
        // i2c operation finished
        switch ( ss.hw.psens.sm )
        {
            case psm_read_oneshotcmd:
                // free the bus, since we will poll on 5ms basis, and leave the bus free for other comm.
                ss.hw.bus_busy = busst_none;        
                ss.flags.sens_pwr = SENSPWR_SLEEP;              // system can sleep with 1ms interrupt watch
                break;
            case psm_read_waitevent:
                // status register received
                if ( ss.hw.psens.hw_read_val[0] & PREG_STATUS_PDR )
                {
                    if ( I2C_device_read( I2C_DEVICE_PRESSURE, REGPRESS_OUTP, 3, ss.hw.psens.hw_read_val ) == I2CSTATE_NONE )
                    {
                        ss.hw.psens.sm = psm_read_waitresult;   // set the next operation - wait for data, bus remains busy
                        break;
                    }
                    else 
                        goto _i2c_failure;
                }
                ss.hw.bus_busy = busst_none;            // mark the bus free
                ss.hw.psens.sm = psm_read_oneshotcmd;   // set state for one-shot command processing - command sent allready, wait the timeout
                ss.flags.sens_pwr = SENSPWR_SLEEP;
                break;
            case psm_read_waitresult:
                // pressure data received
                ss.measured.pressure = (( ((uint32)ss.hw.psens.hw_read_val[0] << 16) | 
                                          ((uint32)ss.hw.psens.hw_read_val[1] << 8)  |
                                          ((uint32)ss.hw.psens.hw_read_val[2] )        ) >> 4 );
                ss.hw.bus_busy = busst_none;                // bus is free
                ss.hw.psens.sm = psm_none;                  // no operation on sensor
                ss.status.sensp_read_request = 0;           // ini request served
                ss.status.sensp_data_ready = 1;             // sensor initted
                ss.flags.sens_busy &= ~SENSOR_PRESS;
                ss.flags.sens_ready |= SENSOR_PRESS;
                ss.flags.sens_pwr = SENSPWR_FREE;
                break;
        }

    }
    // if one-shot cmd is sent and bus is free
    else if ( (ss.hw.psens.sm == psm_read_oneshotcmd) &&
              (ss.hw.psens.check_ctr == PSENS_READ_POLLING) )
    {
        // status request
        if ( I2C_device_read( I2C_DEVICE_PRESSURE, REGPRESS_STATUS, 1, ss.hw.psens.hw_read_val ) == I2CSTATE_NONE )
        {
            ss.hw.bus_busy = busst_pressure;        // mark bus busy
            ss.hw.psens.sm = psm_read_waitevent;    // mark the current operation state
            ss.hw.psens.check_ctr = 0;              // reset check counter
            ss.flags.sens_pwr = SENSPWR_FULL;
        }
        else
            goto _i2c_failure;
    }
    // if this is the first operation (no bus busy)
    else if ( ss.hw.psens.sm == psm_none )
    {
        // bus is free - send the first command (bus_busy is cleared only when execution is finished)
        if ( I2C_device_write( I2C_DEVICE_PRESSURE, psens_cmd_sshot_baro, sizeof(psens_cmd_sshot_baro) ) == I2CSTATE_NONE )
        {
            ss.hw.bus_busy = busst_pressure;        // mark bus busy
            ss.hw.psens.sm = psm_read_oneshotcmd;   // mark the current operation state
            ss.hw.psens.check_ctr = 0;              // reset check counter
            ss.flags.sens_pwr = SENSPWR_FULL;
        }
        else
            goto _i2c_failure;
    }
    return;

_i2c_failure:
    local_i2c_reinit(); 
_failure:
    ss.hw.bus_busy = busst_none;            // mark bus free, routine will retry the command
    ss.hw.psens.sm = psm_none;
    ss.flags.sens_pwr = SENSPWR_FREE;
}



void local_init_pressure_sensor(void)
{
    if ( ss.status.initted_p )
        return;
    ss.status.sensp_ini_request = 1;
}


void local_init_rh_sensor(void)
{

}



void Sensor_Init()
{
    memset( &ss, 0, sizeof(ss) );

    local_i2c_reinit();
    
    // begin init pressure sensor
    local_init_pressure_sensor();
    local_init_rh_sensor();
}

void Sensor_Shutdown( uint32 mask )
{
    if (mask & SENSOR_PRESS)
    {
        ss.status.sensp_data_ready = 0;
        ss.flags.sens_ready &= ~SENSOR_PRESS;
        // TODO the rest
    }

}

void Sensor_Acquire( uint32 mask )
{
    if ( (mask & SENSOR_PRESS) && (ss.status.sensp_read_request == 0) )
    {
        if ( ss.status.initted_p == 0 )
            local_init_pressure_sensor();
        ss.status.sensp_data_ready = 0;
        ss.status.sensp_read_request = 1;       // request data aquire
        ss.flags.sens_busy |= SENSOR_PRESS;
        ss.flags.sens_ready &= ~SENSOR_PRESS;
    }
}

uint32 Sensor_Is_Ready(void)
{
    return (uint32)ss.flags.sens_ready;
}

uint32 Sensor_Is_Busy(void)
{
    return (uint32)ss.flags.sens_busy;
}

uint32 Sensor_Get_Value( uint32 sensor )
{
    switch ( sensor )
    {
        case SENSOR_TEMP:
            if ( ss.status.senst_data_ready == 0 )
                return SENSOR_VALUE_FAIL;
            ss.status.senst_data_ready = 0;
            ss.flags.sens_ready &= ~SENSOR_TEMP;
            return ss.measured.temp;
        case SENSOR_PRESS:
            if ( ss.status.sensp_data_ready == 0 )
                return SENSOR_VALUE_FAIL;
            ss.status.sensp_data_ready = 0;
            ss.flags.sens_ready &= ~SENSOR_PRESS;
            return ss.measured.pressure;
        case SENSOR_RH:
            if ( ss.status.sensrh_data_ready == 0 )
                return SENSOR_VALUE_FAIL;
            ss.status.sensrh_data_ready = 0;
            ss.flags.sens_ready &= ~SENSOR_RH;
            return ss.measured.rh;
        default:
            return SENSOR_VALUE_FAIL;
    }
}


void Sensor_Poll(bool tick_ms)
{
    if ( ss.status.sensp_ini_request )          // this can be set only on uninitted device
    {
        local_psensor_execute_ini( tick_ms );
    }
    else if ( ss.status.sensp_read_request )    // execute read request only if no other setup operation in progress, flag is filtered allready by initiator
    {
        local_psensor_execute_read( tick_ms );
    }
}


uint32 Sensor_GetPwrStatus(void)
{
    return (uint32)ss.flags.sens_pwr;
}

