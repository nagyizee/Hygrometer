/*
    i2c is fiddly -> all the hardware related stuff will be done in this code


*/

#include "i2c.h"
#include "hw_stuff.h"


#define CR1_PE_Set              ((uint16_t)0x0001)
#define CR1_PE_Reset            ((uint16_t)0xFFFE)
#define CR1_START_Set           ((uint16_t)0x0100)
#define CR1_START_Reset         ((uint16_t)0xFEFF)
#define CR1_STOP_Set            ((uint16_t)0x0200)
#define CR1_STOP_Reset          ((uint16_t)0xFDFF)
#define CR1_ACK_Set             ((uint16_t)0x0400)
#define CR1_ACK_Reset           ((uint16_t)0xFBFF)
#define CR1_ENGC_Set            ((uint16_t)0x0040)
#define CR1_ENGC_Reset          ((uint16_t)0xFFBF)
#define CR1_SWRST_Set           ((uint16_t)0x8000)
#define CR1_SWRST_Reset         ((uint16_t)0x7FFF)
#define CR1_PEC_Set             ((uint16_t)0x1000)
#define CR1_PEC_Reset           ((uint16_t)0xEFFF)
#define CR1_ENPEC_Set           ((uint16_t)0x0020)
#define CR1_ENPEC_Reset         ((uint16_t)0xFFDF)
#define CR1_ENARP_Set           ((uint16_t)0x0010)
#define CR1_ENARP_Reset         ((uint16_t)0xFFEF)
#define CR1_NOSTRETCH_Set       ((uint16_t)0x0080)
#define CR1_NOSTRETCH_Reset     ((uint16_t)0xFF7F)
#define CR1_CLEAR_Mask          ((uint16_t)0xFBF5)
#define CR2_DMAEN_Set           ((uint16_t)0x0800)
#define CR2_DMAEN_Reset         ((uint16_t)0xF7FF)
#define CR2_LAST_Set            ((uint16_t)0x1000)
#define CR2_LAST_Reset          ((uint16_t)0xEFFF)
#define CR2_FREQ_Reset          ((uint16_t)0xFFC0)

#define CCR_CCR_Set             ((uint16_t)0x0FFF)
#define CCR_FS_Set              ((uint16_t)0x8000)

#define SR1_SB                  ((uint16_t)0x0001)
#define SR1_ADDR                ((uint16_t)0x0002)
#define SR1_BTF                 ((uint16_t)0x0004)
#define SR1_RXNE                ((uint16_t)0x0040)

#define FREQ_SYS    16000000
#define FREQ_I2C    400000
#define FREQ_SYS10  ((uint16_t)(FREQ_SYS / 1000000))

enum EI2CStates
{
    sm_none = 0,                                // no operation in progress
    sm_readdev_regaddr_waitaddr,                // read register: sent start condition + address + wrbit - waiting for address sent.
    sm_readdev_regaddr_waittx,                  // read register: sent register address - waiting for tx complete
    sm_readdev_readdata_waitaddr,               // read register: start condition + address + rdbit - waiting for addess sent.
    sm_readdev_readdata_waitaddr_DMA,           // read register: dma set up + start condition + address + rdbit - waiting for addess sent.
    sm_readdev_readdata_waitdata,               // read register: address sent, stop condition set up for 1 byte, - wait for data reception.
    sm_readdev_readdata_waitdata_DMA,           // read register: address sent, interface filling memory - wait when DMA transaction is finished
    sm_readdev_last = sm_readdev_readdata_waitdata_DMA,

    sm_writedev_regaddr_waitaddr,               // write register: DMA set up for tx, start condition + address sent - wait for address to be sent
    sm_writedev_waittxc,                        // write register: addr bit cleared, transmission started through DMA - wait for finish tx
    sm_writedev_waitcomplete,                   // DMA disabled, wait till data is shifted out to generate stop condition

};

struct SI2CStateMachine
{
    enum EI2CStates     sm;
    uint8               *buffer;
    uint8               dev_addr;
    uint8               reg_addr;
    uint8               reg_data_lenght;
    uint32              to_ctr;                 // safety TimeOut counter
    uint32              fail_code;
} i2c;


#define WAIT( a )   i2c.to_ctr = 0;  do   {   i2c.to_ctr++; if (i2c.to_ctr > 400)  goto _failure;   } while( a )


static uint32 local_I2C_internal_setstart( uint8 address, bool wrt )
{
    I2C_PORT_SENSOR->CR1 |= CR1_START_Set;              // Set start condition
    WAIT( (I2C_PORT_SENSOR->SR1 & SR1_SB) != SR1_SB );
    
    if ( wrt )
        I2C_PORT_SENSOR->DR = address;
    else
        I2C_PORT_SENSOR->DR = address | 0x0001;         // set the read flag

    return 0;
_failure:
    return 1;
}

static bool local_I2C_internal_waitaddr( bool one_byte_rx )
{
    if ( I2C_PORT_SENSOR->SR1 & SR1_ADDR )      // EV6
    {
        volatile uint32 temp;

        if ( one_byte_rx == false )
            temp = I2C_PORT_SENSOR->SR2;            // clear (ADDR) by reading SR2
        else
        {
            // for 1 byte reads we must program for NAK before clearing (ADDR), 
            // and stop should be sent immediately as receiving starts
            I2C_PORT_SENSOR->CR1 &= CR1_ACK_Reset;  // set up to send NAK after receiving byte
            __disable_irq();
            temp = I2C_PORT_SENSOR->SR2;            // clear (ADDR) by reading SR2
            I2C_PORT_SENSOR->CR1 |= CR1_STOP_Set;   // set up stop condition
            __enable_irq();
        }

        return true;
    }
    return false;
}

static bool local_I2C_internal_waitaddr_sendregaddr( void )
{
    if ( local_I2C_internal_waitaddr(false) )
    {
        I2C_PORT_SENSOR->DR = i2c.reg_addr;     // send the register address (EV8_1)
        return true;
    }
    return false;
}

static bool local_I2C_internal_wait_regaddr_tx_setreceive( void )
{
    if (I2C_PORT_SENSOR->SR1 & SR1_BTF )          // Wait for byte transmission finish including ACK reception (BTF)
    {
        if ( i2c.sm == sm_readdev_regaddr_waittx )
        {
            // if called for data reading

            if ( i2c.reg_data_lenght > 1 )
            {
                // set up DMA for receiving data
                HW_DMA_Receive( DMACH_SENS, i2c.buffer, i2c.reg_data_lenght );
                I2C_PORT_SENSOR->CR2 |= CR2_LAST_Set;          // Set Last bit to have a NACK on the last received byte
                I2C_PORT_SENSOR->CR2 |= CR2_DMAEN_Set;         // Enable I2C DMA requests
            }

            // send the start condition and device address
            if ( local_I2C_internal_setstart( i2c.dev_addr, false ) )
            {
                i2c.fail_code |= I2CFAIL_SETST;
                return false;
            }
        }
        return true;
    }
    return false;
}


static bool local_I2C_internal_waitdata_stop( void )
{
    if ( I2C_PORT_SENSOR->SR1 & SR1_RXNE )
    {
        i2c.buffer[0] = I2C_PORT_SENSOR->DR;            // read the data
        WAIT (I2C_PORT_SENSOR->CR1 & CR1_STOP_Set);     // wait for stop to clear

        I2C_PORT_SENSOR->CR1 |= CR1_ACK_Set;            // set back ACK generation
        return true;
    }
    return false;
_failure:
    i2c.fail_code |= I2CFAIL_SETST;
    return false;
}

static bool local_I2C_internal_waitdata_DMA_stop( void )
{
    if ( DMA1->ISR & DMA1_FLAG_TC7 )
    {
        DMA_Cmd(DMA_SENS_RX_Channel, DISABLE);
        DMA1->IFCR = DMA1_FLAG_TC7;                     // clear the transfer ready flag

        I2C_PORT_SENSOR->CR1 |= CR1_STOP_Set;
        WAIT (I2C_PORT_SENSOR->CR1 & CR1_STOP_Set);     // wait for stop to clear

        I2C_PORT_SENSOR->CR2 &= CR2_DMAEN_Reset;        // Disable DMA requests for i2c

        return true;
    }
    return false;
_failure:
    i2c.fail_code |= I2CFAIL_SETST;
    return false;
}

static bool local_I2C_internal_waitTX_DMA_stop( void )
{
    if  ( DMA1->ISR & DMA1_FLAG_TC6 )
    {
        DMA_Cmd(DMA_SENS_TX_Channel, DISABLE);
        DMA1->IFCR = DMA1_FLAG_TC6;             // Clear the DMA Transfer complete flag

        return true;
    }
    return false;
}

static bool local_I2C_internal_waitTX_finish( void )
{
    if ( I2C_PORT_SENSOR->SR1 & SR1_BTF )
    {
        I2C_PORT_SENSOR->CR1 |= CR1_STOP_Set;
        WAIT (I2C_PORT_SENSOR->CR1 & CR1_STOP_Set);    // wait for stop to clear
        I2C_PORT_SENSOR->CR2 &= CR2_DMAEN_Reset;        // Disable DMA requests for i2c

        return true;
    }
    return false;
_failure:
    i2c.fail_code |= I2CFAIL_SETST;
    return false;
}


// init I2C peripherial after power-on reset
void I2C_interface_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    uint16_t tmpreg = 0;

    memset( &i2c, 0, sizeof(i2c) );

    // enable clock for I2C interface
    RCC_APB1PeriphClockCmd( I2C_APB_SENSOR, ENABLE );

    // set up GPI port for i2c
    GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin     = IO_OUT_SENS_SCL | IO_OUT_SENS_SDA;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_OD;
    GPIO_Init(IO_PORT_SENS_SCL, &GPIO_InitStructure);
    // --- to prevent noise filter lockup - use the method described in errata sheet: 2.10.7
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_Out_OD;
    GPIO_Init(IO_PORT_SENS_SCL, &GPIO_InitStructure);
    IO_PORT_SENS_SCL->BSRR = ( IO_OUT_SENS_SCL | IO_OUT_SENS_SDA );          // 2. Configure the SCL and SDA I/Os as General Purpose Output Open-Drain, High level
    while( !(IO_PORT_SENS_SCL->IDR & IO_OUT_SENS_SCL) || !(IO_PORT_SENS_SCL->IDR & IO_OUT_SENS_SDA) )  // 3. Check SCL and SDA High level in GPIOx_IDR.
    {
        // peripheral may hold the lines low - try to unlock by generating ~8 clock cycles
        while( !(IO_PORT_SENS_SCL->IDR & IO_OUT_SENS_SDA) )
        {
            IO_PORT_SENS_SCL->BRR  = IO_OUT_SENS_SCL;
            HW_Delay( 1 );
            IO_PORT_SENS_SCL->BSRR  = IO_OUT_SENS_SCL;
            HW_Delay( 1 );
        }
    }
    IO_PORT_SENS_SCL->BRR  = IO_OUT_SENS_SDA;                               // 4. Configure the SDA I/O as General Purpose Output Open-Drain, Low level
    while( IO_PORT_SENS_SCL->IDR & IO_OUT_SENS_SDA );                       // 5. Check SDA Low level in GPIOx_IDR.
    IO_PORT_SENS_SCL->BRR  = IO_OUT_SENS_SCL;                               // 6. Configure the SCL I/O as General Purpose Output Open-Drain, Low level
    while( IO_PORT_SENS_SCL->IDR & IO_OUT_SENS_SCL );                       // 7. Check SCL Low level in GPIOx_IDR.
    IO_PORT_SENS_SCL->BSRR = IO_OUT_SENS_SCL;                               // 8. Configure the SCL I/O as General Purpose Output Open-Drain, High level
    while( !(IO_PORT_SENS_SCL->IDR & IO_OUT_SENS_SCL) );                    // 9. Check SCL High level in GPIOx_IDR.
    IO_PORT_SENS_SCL->BSRR = IO_OUT_SENS_SDA;                               // 10.Configure the SDA I/O as General Purpose Output Open-Drain , High level
    while( !(IO_PORT_SENS_SCL->IDR & IO_OUT_SENS_SDA) );                    // 11.Check SDA High level in GPIOx_IDR.
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_OD;                      // 12.Configure the SCL and SDA I/Os as Alternate function Open-Drain
    GPIO_Init(IO_PORT_SENS_SCL, &GPIO_InitStructure);
    // the rest is taken care at the reset interface

    // reset the interface
    RCC_APB1PeriphResetCmd( I2C_APB_SENSOR, ENABLE );
    RCC_APB1PeriphResetCmd( I2C_APB_SENSOR, DISABLE );

    // Configure the I2C peripheral - ini code brought and optimized from stm32f10x_i2c.c
    // I2Cx CR2 Configuration
    tmpreg = I2C_PORT_SENSOR->CR2;
    tmpreg &= CR2_FREQ_Reset;       // Clear frequency FREQ[5:0] bits
    tmpreg |= FREQ_SYS10;               // Set frequency bits depending on pclk1 value
    I2C_PORT_SENSOR->CR2 = tmpreg;  // Write to I2Cx CR2

    // I2Cx CCR Configuration
    I2C_PORT_SENSOR->CR1 &= CR1_PE_Reset;                               // Disable the selected I2C peripheral to configure TRISE
    tmpreg = 0;                                                         // Reset tmpreg value, Clear F/S, DUTY and CCR[11:0] bits
    tmpreg |= (uint16_t)( (FREQ_SYS / (FREQ_I2C * 3)) | CCR_FS_Set);    // Set speed value and set F/S bit for fast mode - FREQ_SYS / (i2c_speed * 3)) - Fast mode speed calculate: Tlow/Thigh = 2 
    I2C_PORT_SENSOR->TRISE = (uint16_t)(((FREQ_SYS10 * (uint16_t)300) / (uint16_t)1000) + (uint16_t)1);  // Set Maximum Rise Time for fast mode
    
    I2C_PORT_SENSOR->CCR = tmpreg;          // Write to I2Cx CCR
    I2C_PORT_SENSOR->CR1 |= CR1_PE_Set;     // Enable the selected I2C peripheral

    // I2Cx CR1 Configuration
    tmpreg = I2C_PORT_SENSOR->CR1;                      // Get the I2Cx CR1 value
    tmpreg &= CR1_CLEAR_Mask;               // Clear ACK, SMBTYPE and  SMBUS bits
    tmpreg |= (uint16_t)((uint32_t)I2C_Mode_I2C | I2C_Ack_Enable);  // Configure I2Cx: mode and acknowledgement, Set SMBTYPE and SMBUS bits according to I2C_Mode value, Set ACK bit according to I2C_Ack value
    I2C_PORT_SENSOR->CR1 = tmpreg;          // Write to I2Cx CR1

    // Set the own address
    I2C_PORT_SENSOR->OAR1 = ( I2C_AcknowledgedAddress_7bit | 0x0C); // Set I2Cx Own Address (0x0C) and acknowledged address
}


// Read a register from a device.
// Note: the routine is asynchronous - check data availability and keep
// the buffer available till finishing
uint32 I2C_device_read( uint8 dev_address, uint8 dev_reg_addr, uint32 data_lenght, uint8 *buffer )
{
    if ( i2c.sm != sm_none )
        return 1;

    i2c.dev_addr = dev_address;
    i2c.reg_addr = dev_reg_addr;
    i2c.reg_data_lenght = data_lenght;
    i2c.buffer = buffer;
    if ( local_I2C_internal_setstart( dev_address, true ) )
    {
        i2c.fail_code = I2CFAIL_SETST;
        return I2CSTATE_FAIL;
    }

    i2c.sm = sm_readdev_regaddr_waitaddr;
    return I2CSTATE_NONE;
}

// Write a register to a device.
// Note: the routine is asynchronous - check data availability and keep
// the buffer available till finishing
// First byte in data buffer is the register address
uint32 I2C_device_write( uint8 dev_address, const uint8 *buffer, uint32 data_lenght )
{
    if ( i2c.sm != sm_none )
        return 1;

    i2c.dev_addr = dev_address;
    i2c.reg_addr = 0;
    i2c.reg_data_lenght = data_lenght;
    i2c.buffer = 0;

    HW_DMA_Send( DMACH_SENS, buffer, data_lenght );
    I2C_PORT_SENSOR->CR2 |= CR2_DMAEN_Set;         // Enable I2C DMA requests

    if ( local_I2C_internal_setstart( dev_address, true ) )
    {
        i2c.fail_code = I2CFAIL_SETST;
        return I2CSTATE_FAIL;
    }
    i2c.sm = sm_writedev_regaddr_waitaddr;
    return I2CSTATE_NONE;
}

// Check if operations are finished - this polls internal state machines also
uint32 I2C_busy( void )
{
    if ( i2c.sm == sm_none )
        return I2CSTATE_NONE;

    if ( i2c.sm <= sm_readdev_last )
    {
        switch ( i2c.sm )
        {
            case sm_readdev_regaddr_waitaddr:   
                if (local_I2C_internal_waitaddr_sendregaddr() )
                    i2c.sm = sm_readdev_regaddr_waittx;
                break;
            case sm_readdev_regaddr_waittx:
                if (local_I2C_internal_wait_regaddr_tx_setreceive() )
                {
                    if ( i2c.reg_data_lenght > 1 )
                        i2c.sm = sm_readdev_readdata_waitaddr_DMA;
                    else
                        i2c.sm = sm_readdev_readdata_waitaddr;
                }
                break;
            case sm_readdev_readdata_waitaddr:
                if (local_I2C_internal_waitaddr(true) )
                    i2c.sm = sm_readdev_readdata_waitdata;
                break;
            case sm_readdev_readdata_waitaddr_DMA:
                if (local_I2C_internal_waitaddr(false) )
                    i2c.sm = sm_readdev_readdata_waitdata_DMA;
                break;
            case sm_readdev_readdata_waitdata:
                if ( local_I2C_internal_waitdata_stop() )
                {
                    i2c.sm = sm_none;
                    return I2CSTATE_NONE;
                }
                break;
            case sm_readdev_readdata_waitdata_DMA:
                if ( local_I2C_internal_waitdata_DMA_stop() )
                {
                    i2c.sm = sm_none;
                    return I2CSTATE_NONE;
                }
                break;
        }
    }
    else
    {
        switch ( i2c.sm )
        {
            case sm_writedev_regaddr_waitaddr: 
                if (local_I2C_internal_waitaddr(false) )
                    i2c.sm = sm_writedev_waittxc;
                break;
            case sm_writedev_waittxc: 
                if (local_I2C_internal_waitTX_DMA_stop() )
                    i2c.sm = sm_writedev_waitcomplete;
                break;
            case sm_writedev_waitcomplete:
                if (local_I2C_internal_waitTX_finish() )
                {
                    i2c.sm = sm_none;
                    return I2CSTATE_NONE;
                }
                break;

        }
    }

    if ( i2c.fail_code )
        return I2CSTATE_FAIL;
    return I2CSTATE_BUSY;
}


uint32 I2C_errorcode( void )
{
    return i2c.fail_code;
}
