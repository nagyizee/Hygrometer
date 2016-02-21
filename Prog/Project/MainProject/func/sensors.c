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

#define PSENS_READ_POLLING_TO  1000


const uint8     psens_set_01_data_event[] = { REGPRESS_DATACFG, (PREG_DATACFG_TDEFE | PREG_DATACFG_PDEFE | PREG_DATACFG_DREM) };     // set up data event signalling for pressure update
const uint8     psens_set_02_interrupt_src[]  = { REGPRESS_CTRL3, PREG_CTRL3_IPOL1 };       // pushpull active high on INT1
const uint8     psens_set_03_interrupt_en[]  = { REGPRESS_CTRL4, PREG_CTRL4_DRDY };         // enable data ready interrupt
const uint8     psens_set_04_interrupt_out[]  = { REGPRESS_CTRL5, PREG_CTRL5_DRDY };        // route data ready interrupt to INT1

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

void local_setpower_free(void)
{
    // called by state machines when a sensor finished operation, or cancelled 
    // need to check if an other sensor needs SLEEP power management.
    // FULL is held only for active bus operations, so if one finishes the no FULL is needed

    // cases when SLEEP is needed
    if ( (ss.hw.rhsens.sm == rhsm_init_01_wait_powerup) ||
         (ss.hw.psens.sm == psm_read_oneshotcmd ) ||
         (ss.hw.rhsens.sm == rhsm_readrh_01_send_request ) ||
         (ss.hw.rhsens.sm == rhsm_readt_01_send_request )     )
        ss.flags.sens_pwr = SENSPWR_SLEEP;
    else
        ss.flags.sens_pwr = SENSPWR_FREE;
}


void local_setpower_sleep(void)
{
    // do not set power management to sleep when an other sensor is operating at full
    if ( ss.hw.bus_busy )
        return;
    ss.flags.sens_pwr = SENSPWR_SLEEP;
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
                if ( I2C_device_write( I2C_DEVICE_PRESSURE, psens_set_02_interrupt_src, sizeof(psens_set_02_interrupt_src), 0 ) )
                    goto _i2c_failure;
                ss.hw.psens.sm = psm_init_02;
                break;
            case psm_init_02:
                if ( I2C_device_write( I2C_DEVICE_PRESSURE, psens_set_03_interrupt_en, sizeof(psens_set_03_interrupt_en), 0 ) )
                    goto _i2c_failure;
                ss.hw.psens.sm = psm_init_03;
                break;
            case psm_init_03:
                if ( I2C_device_write( I2C_DEVICE_PRESSURE, psens_set_04_interrupt_out, sizeof(psens_set_04_interrupt_out), 0 ) )
                    goto _i2c_failure;
                ss.hw.psens.sm = psm_init_04;
                break;
            case psm_init_04:
                ss.hw.bus_busy = busst_none;                // bus is free
                ss.hw.psens.sm = psm_none;                  // no operation on sensor
                ss.status.sensp_ini_request = 0;            // ini request served
                ss.status.initted_p = 1;                    // sensor initted
                ss.flags.sens_fail &= ~SENSOR_PRESS;
                local_setpower_free();
                break;
        }
    }
    else
    {
        // bus is free - send the first command (bus_busy is cleared only when execution is finished)
        if ( I2C_device_write( I2C_DEVICE_PRESSURE, psens_set_01_data_event, sizeof(psens_set_01_data_event), 0 ) == I2CSTATE_NONE )
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
    local_setpower_free();
}


void local_rhsensor_execute_ini( bool mstick )
{
    if ( ss.hw.rhsens.sm == rhsm_none )
    {
        // first entry in the ini procedure
        ss.hw.rhsens.sm = rhsm_init_01_wait_powerup;
        ss.hw.rhsens.to_ctr = 16;       // 15+1 ms countdown
        local_setpower_sleep();
        return;
    }
    else if ( ss.hw.rhsens.to_ctr )
    {
        // this can happen only in power-up wait phase - decrease the timeout counter at every 1 ms
        if ( mstick )
            ss.hw.rhsens.to_ctr--;

        return;
    }

    if ( ss.hw.bus_busy == busst_pressure )       // bus is busy with the Pressure sensor - not part of this subsystem - exit
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
        switch ( ss.hw.rhsens.sm )
        {
            case rhsm_init_02_read_user_reg:
                {
                    // user register read completed, set it up
                    ss.hw.rhsens.hw_read_val[1] = (ss.hw.rhsens.hw_read_val[0] & RHREG_USER_RESMASK) | rhres_12_14;
                    ss.hw.rhsens.hw_read_val[0] = REGRH_USER_WRITE;
                    if ( I2C_device_write( I2C_DEVICE_RH, ss.hw.rhsens.hw_read_val, 2, 0 ) )
                        goto _i2c_failure;
                    ss.hw.rhsens.sm = rhsm_init_03_write_user_reg;
                }
                break;
            case rhsm_init_03_write_user_reg:
                ss.hw.bus_busy = busst_none;                // bus is free
                ss.hw.rhsens.sm = rhsm_none;                // no operation on sensor
                ss.status.sensrh_ini_request = 0;           // ini request served
                ss.status.initted_rh = 1;                   // sensor initted
                ss.flags.sens_fail &= ~SENSOR_RH;
                local_setpower_free();
                break;
        }
    } 
    else
    {
        // we get here only when the 15ms power-up time expires
        // set up user register - read it first
        if ( I2C_device_read( I2C_DEVICE_RH, REGRH_USER_READ, 1, ss.hw.rhsens.hw_read_val ) == I2CSTATE_NONE )
        {
            ss.hw.bus_busy = busst_rh;                      // mark bus busy
            ss.hw.rhsens.sm = rhsm_init_02_read_user_reg;    // mark the current operation state
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
    ss.hw.rhsens.sm = rhsm_none;
    local_setpower_free();
}




void local_psensor_execute_read( bool tick_ms )
{
    // no need to check for uninitted state - taken care at the initiator routine

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
                local_setpower_sleep();             // system can sleep with 1ms interrupt watch
                break;
            case psm_read_waitresult:
                // pressure data received
                ss.measured.pressure = (( ((uint32)ss.hw.psens.hw_read_val[0] << 16) | 
                                          ((uint32)ss.hw.psens.hw_read_val[1] << 8)  |
                                          ((uint32)ss.hw.psens.hw_read_val[2] )        ) >> 4 );
                ss.hw.bus_busy = busst_none;                // bus is free
                ss.hw.psens.sm = psm_none;                  // no operation on sensor
                ss.flags.sens_busy &= ~SENSOR_PRESS;
                ss.flags.sens_ready |= SENSOR_PRESS;
                local_setpower_free();
                break;
        }

    }
    // if one-shot cmd is sent and bus is free
    else if ( ss.hw.psens.sm == psm_read_oneshotcmd )
    {
        // if one-shot cmd is sent - check for data ready
        if ( HW_PSens_IRQ() )
        {
            // IRQ received - request pressure data
            if ( I2C_device_read( I2C_DEVICE_PRESSURE, REGPRESS_OUTP, 3, ss.hw.psens.hw_read_val ) == I2CSTATE_NONE )
            {
                ss.hw.bus_busy = busst_pressure;        // mark bus busy
                ss.hw.psens.sm = psm_read_waitresult;   // mark the current operation state
                ss.hw.psens.check_ctr = 0;              // reset check counter
                ss.flags.sens_pwr = SENSPWR_FULL;
            }
            else
                goto _i2c_failure;
        }
        else if ( tick_ms )
        {
            if ( ss.hw.psens.check_ctr < PSENS_READ_POLLING_TO )
                ss.hw.psens.check_ctr++;
            //TODO: time_out error handling
        }
    }
    // if this is the first operation (no bus busy)
    else if ( ss.hw.psens.sm == psm_none )
    {
        // bus is free - send the first command (bus_busy is cleared only when execution is finished)
        if ( I2C_device_write( I2C_DEVICE_PRESSURE, psens_cmd_sshot_baro, sizeof(psens_cmd_sshot_baro), 0 ) == I2CSTATE_NONE )
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
    local_setpower_free();
}


void local_rhsensor_execute_read( bool tick_ms )
{
    // bus is busy with the Pressure sensor - not part of this subsystem - exit
    if ( ss.hw.bus_busy == busst_pressure )       
        return;

    // check for after timeout operations
    if ( tick_ms && ss.hw.rhsens.to_ctr )
    {
        ss.hw.rhsens.to_ctr--;
        if ( ss.hw.rhsens.to_ctr == 0 )
        {
            // timeout reached, no other bus operations
            // try a read-out - it fails with NAK if device is not ready
            if ( I2C_device_poll_read( I2C_DEVICE_RH, 2, ss.hw.rhsens.hw_read_val )  )
                goto _i2c_failure;

            if ( ss.hw.rhsens.sm == rhsm_readrh_01_send_request )
                ss.hw.rhsens.sm = rhsm_readrh_02_wait4read;
            else
                ss.hw.rhsens.sm = rhsm_readt_02_wait4read;

            ss.hw.bus_busy = busst_rh;        // mark bus busy
            ss.flags.sens_pwr = SENSPWR_FULL;
            return;
        }
    }


    if ( ss.hw.bus_busy )
    {
        uint32 result;
        result = I2C_busy();
        if ( result == I2CSTATE_BUSY )
            return;                                                 // I2C still busy - exit
        if ( result == I2CSTATE_FAIL )
        {
            if ( I2C_errorcode() == I2CFAIL_SETST )
                goto _i2c_failure;                                  // signal setup failure
            if ( (ss.hw.rhsens.sm != rhsm_readrh_02_wait4read) &&
                 (ss.hw.rhsens.sm != rhsm_readt_02_wait4read) )
                goto _failure;                                      // NAK failure - process it only if not in read result phase
        }

        switch ( ss.hw.rhsens.sm )
        {
            case rhsm_readrh_01_send_request:
            case rhsm_readt_01_send_request:
                // command sent, we need to wait 85ms for T, 29ms for RH, do a check read at 80ms/24ms
                if ( ss.hw.rhsens.sm == rhsm_readrh_01_send_request)
                    ss.hw.rhsens.to_ctr = 24;
                else
                    ss.hw.rhsens.to_ctr = 80;
                ss.hw.bus_busy = busst_none;        
                local_setpower_sleep();             // system can sleep with 1ms interrupt watch
                break;
            case rhsm_readrh_02_wait4read:
            case rhsm_readt_02_wait4read:
                // response read, or NAK received (device is not ready yet)
                if ( result == I2CSTATE_NONE )
                {
                    // result is ready
                    ss.hw.bus_busy = busst_none;                // bus is free
                    local_setpower_free();                      // mark power management free

                    if ( ss.hw.rhsens.sm == rhsm_readrh_02_wait4read )
                    {
                        ss.measured.rh =  ( ((uint32)ss.hw.rhsens.hw_read_val[0] << 6) | 
                                            ((uint32)ss.hw.rhsens.hw_read_val[1] >> 2)   );
                        ss.flags.sens_busy &= ~SENSOR_RH;
                        ss.flags.sens_ready |= SENSOR_RH;
                        ss.hw.rhsens.sm = rhsm_none;                  // no operation on sensor
                        if ( ss.flags.sens_busy & SENSOR_TEMP )
                            goto _reload_operation;
                    }
                    else
                    {
                        ss.measured.temp =  ( ((uint32)ss.hw.rhsens.hw_read_val[0] << 6) | 
                                            ((uint32)ss.hw.rhsens.hw_read_val[1] >> 2)   );
                        ss.flags.sens_busy &= ~SENSOR_TEMP;
                        ss.flags.sens_ready |= SENSOR_TEMP;
                        ss.hw.rhsens.sm = rhsm_none;                  // no operation on sensor
                        if ( ss.flags.sens_busy & SENSOR_RH )
                            goto _reload_operation;
                    }
                }
                else
                {
                    // NAK received - retry in 5ms
                    if ( ss.hw.rhsens.sm == rhsm_readrh_02_wait4read )
                        ss.hw.rhsens.sm = rhsm_readrh_01_send_request;
                    else
                        ss.hw.rhsens.sm = rhsm_readt_01_send_request;
                    ss.hw.rhsens.to_ctr = 5;
                    ss.hw.bus_busy = busst_none;        
                    local_setpower_sleep();             // system can sleep with 1ms interrupt watch
                }
                break;
        }
    }
    else if ( ss.hw.rhsens.sm == rhsm_none )
    {   
_reload_operation:  // we need this label to reload sensor read operation for the other parameter when RH and Temp are                      
                    // requested simultaneously, otherwise it finishes with RH, exits, RH is read by code, reacquire                        
                    // and the poll will enter again with RH measurement, not leaving chance for Temp to proceed                            
        if ( ss.flags.sens_busy & SENSOR_RH )
        {
            ss.hw.rhsens.hw_read_val[0] = REGRH_TRIG_RH;
            ss.hw.rhsens.sm = rhsm_readrh_01_send_request;      // mark the current operation state
        }
        else
        {
            ss.hw.rhsens.hw_read_val[0] = REGRH_TRIG_TEMP;
            ss.hw.rhsens.sm = rhsm_readt_01_send_request;      // mark the current operation state
        }

        if ( I2C_device_write( I2C_DEVICE_RH, ss.hw.rhsens.hw_read_val, 1 , 20) )
            goto _i2c_failure;

        ss.hw.bus_busy = busst_rh;              // mark bus busy
        ss.hw.rhsens.to_ctr = 0;                // reset check counter
        ss.flags.sens_pwr = SENSPWR_FULL;
    }

    return;
_i2c_failure:
    local_i2c_reinit(); 
_failure:
    ss.hw.bus_busy = busst_none;            // mark bus free, routine will retry the command
    ss.hw.rhsens.sm = rhsm_none;
    local_setpower_free();
}


void local_init_pressure_sensor(void)
{
    if ( ss.status.initted_p )
        return;
    ss.status.sensp_ini_request = 1;
}


void local_init_rh_sensor(void)
{
    if ( ss.status.initted_rh )
        return;
    ss.status.sensrh_ini_request = 1;
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
        if ( (ss.hw.psens.sm != psm_none) && (ss.hw.bus_busy == busst_pressure) )
        {
            // if there is an active action on the sensor 
            uint32 result;
            do
            {
                // wait till comm. finishes
                result = I2C_busy();
            } while ( result == I2CSTATE_BUSY );

            // check for error
            if ( (result == I2CSTATE_FAIL) && (I2C_errorcode() == I2CFAIL_SETST) )
                local_i2c_reinit(); 

            ss.hw.bus_busy = busst_none;            // mark bus free, routine will retry the command
        }

        // reset everything on the sensor
        ss.hw.psens.sm = psm_none;
        ss.hw.psens.check_ctr = 0;
        if ( ss.hw.bus_busy == busst_none )         // set power management free only if the other sensor is not operated
            local_setpower_free();

        ss.flags.sens_ready &= ~SENSOR_PRESS;
        ss.flags.sens_busy &= ~SENSOR_PRESS;

        ss.status.sensp_ini_request = 0;
        ss.status.initted_p = 0;
    }

}

void Sensor_Acquire( uint32 mask )
{
    if ( (mask & SENSOR_PRESS) && ((ss.flags.sens_busy & SENSOR_PRESS) == 0) )
    {
        if ( ss.status.initted_p == 0 )
            local_init_pressure_sensor();       // mark for init if not done yet
        ss.flags.sens_busy |= SENSOR_PRESS;     // mark sensor for read
        ss.flags.sens_ready &= ~SENSOR_PRESS;   // clear the ready flag (if not cleared by a previous read)
    }
    if ( mask & (SENSOR_RH | SENSOR_TEMP) )
    {
        if ( ss.status.initted_p == 0 )
            local_init_pressure_sensor();       // mark for init if not done yet

        if ( (mask & SENSOR_RH) && ((ss.flags.sens_busy & SENSOR_RH) == 0) )
        {
            ss.flags.sens_busy |= SENSOR_RH;     // mark sensor for read
            ss.flags.sens_ready &= ~SENSOR_RH;   // clear the ready flag (if not cleared by a previous read)
        }
        if ( (mask & SENSOR_TEMP) && ((ss.flags.sens_busy & SENSOR_TEMP) == 0) )
        {
            ss.flags.sens_busy |= SENSOR_TEMP;     // mark sensor for read
            ss.flags.sens_ready &= ~SENSOR_TEMP;   // clear the ready flag (if not cleared by a previous read)
        }
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
            if ( (ss.flags.sens_ready & SENSOR_TEMP) == 0 )
                return SENSOR_VALUE_FAIL;
            ss.flags.sens_ready &= ~SENSOR_TEMP;
            // temperature is provided in 16fp9 + 40*C
            //   (175.75 * x) / 2^14  - (46.85-40*C)*2^9
            //          175.72 << 9 (17bit)        (14bit) 
            if ( ss.measured.temp < 639 )       // -40*C
                return 0;
            if ( ss.measured.temp > 12573)      // 88*C
                return SENSOR_VAL_MAX;
            return ( ( ((17572 << TEMP_FP)/100) * (uint32)ss.measured.temp ) >> 14) - ( ((4685-4000) << TEMP_FP)/100 );
        case SENSOR_PRESS:
            if ( (ss.flags.sens_ready & SENSOR_PRESS) == 0 )
                return SENSOR_VALUE_FAIL;
            ss.flags.sens_ready &= ~SENSOR_PRESS;
            // Pressure is provided in 100xPa
            // measurement: 20fp2 -> 18bit pascal + 2bit fractional
            // Pa*100 = (press * 100) / 4
            return ((ss.measured.pressure * 100) >> 2 );
            // return ss.measured.pressure;
        case SENSOR_RH:
            if ( (ss.flags.sens_ready & SENSOR_RH) == 0 )
                return SENSOR_VALUE_FAIL;
            ss.flags.sens_ready &= ~SENSOR_RH;
            // RH is provided in 16FP8 %
            //   (125*2^8 * x) / 2^14 - 6*2^9
            return ( ((125 << RH_FP) * (uint32)ss.measured.rh) >> 14 ) - ( 6 << RH_FP );
        default:
            return SENSOR_VALUE_FAIL;
    }
}


void Sensor_Poll(bool tick_ms)
{
    // max 24us, min 7us on RAM
    if ( ss.status.sensp_ini_request || ss.status.sensrh_ini_request )  // this can be set only on uninitted device
    {
        if ( ss.status.sensp_ini_request )
            local_psensor_execute_ini( tick_ms );
        if ( ss.status.sensrh_ini_request )
            local_rhsensor_execute_ini( tick_ms );
    }

    if ( ss.flags.sens_busy )
    {
        if ( (ss.flags.sens_busy & SENSOR_PRESS) &&
             (ss.status.initted_p) )       // execute read request only if no other setup operation in progress, flag is filtered allready by initiator
            local_psensor_execute_read( tick_ms );
        if ( (ss.flags.sens_busy & (SENSOR_RH | SENSOR_TEMP)) &&
             (ss.status.initted_rh) )
            local_rhsensor_execute_read( tick_ms );
    }

}


uint32 Sensor_GetPwrStatus(void)
{
    return (uint32)ss.flags.sens_pwr;
}

