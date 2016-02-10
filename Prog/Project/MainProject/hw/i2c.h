#ifndef __I2C_H
#define __I2C_H

#ifdef __cplusplus
    extern "C" {
#endif

    #include "stm32f10x.h"
    #include "typedefs.h"
    

    #define I2CFAIL_SETST           0x01            // failed setting start/stop condition -> bus error - need to reinit i2c
    
    #define I2CSTATE_NONE           0x00            // i2c busy state none - means no operation is done
    #define I2CSTATE_BUSY           0x01            // i2c communication is in progress
    #define I2CSTATE_FAIL           0x02            // i2c communication failed - check the error code in i2c.fail_code and take actions

    // init I2C peripherial after power-on reset or do a reinit
    void I2C_interface_init(void);

    // Read a register from a device.
    // Note: the routine is asynchronous - check data availability and keep
    // the buffer available till finishing
    uint32 I2C_device_read( uint8 dev_address, uint8 dev_reg_addr, uint32 data_lenght, uint8 *buffer );
    
    // Write a register to a device.
    // Note: the routine is asynchronous - check data availability and keep
    // the buffer available till finishing
    // First byte in data buffer is the register address
    uint32 I2C_device_write( uint8 dev_address, const uint8 *buffer, uint32 data_lenght );
    
    // Check if operations are finished - this polls internal state machines also
    uint32 I2C_busy( void );

    // Get the error code
    uint32 I2C_errorcode( void );


#ifdef __cplusplus
    }
#endif

#endif
