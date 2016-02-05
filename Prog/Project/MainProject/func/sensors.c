/*
        Sensor interface routines

        It works based on polling. Since I2C is slow, most of the operations 
        are done asynchronously so freqvent polling is needed

        Estimated i2c speed: 400kHz -> 40kbps - 25us/byte = 400clocks

        i2c is fiddly -> all the hardware related stuff will be done in this code


    Master transmitter:

    | S |     | Addr+dir | A |             | Data1 | A | Data 2 | A |               | Data n | A |       | P |
        | EV5 |              | EV6 | EV8_1 | EV8 |     | EV8 |      | EV8 | .....                | EV8_2 |
    

    Legend: S= Start, Sr = Repeated Start, P= Stop, A= Acknowledge,
    EVx= Event (with interrupt if ITEVFEN = 1)

    EV5: SB=1, cleared by reading SR1 register followed by writing DR register with Address.
    EV6: ADDR=1, cleared by reading SR1 register followed by reading SR2.
    EV8_1: TxE=1, shift register empty, data register empty, write Data1 in DR.
    EV8: TxE=1, shift register not empty, .data register empty, cleared by writing DR register
    EV8_2: TxE=1, BTF = 1, Program Stop request. TxE and BTF are cleared by hardware by the Stop condition
    EV9: ADD10=1, cleared by reading SR1 register followed by writing DR register

    Notes: 1- The EV5, EV6, EV9, EV8_1 and EV8_2 events stretch SCL low until the end of the corresponding software sequence.
    2- The EV8 software sequence must complete before the end of the current byte transfer. In case EV8 software
    sequence can not be managed before the current byte end of transfer, it is recommended to use BTF instead
    of TXE with the drawback of slowing the communication

    Detecting Acknowledge failure (AF bit): should generate a Stop or reStart condition and repeat the operation


*/

#include <stdio.h>

#include "sensors.h"
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

#define SR1_SB                  ((uint16_t)0x0001)
#define SR1_ADDR                ((uint16_t)0x0002)
#define SR1_BTF                 ((uint16_t)0x0004)


#define I2C_DEVICE_PRESSURE     0xC0            // pressure sensor's device address
#define I2C_DEVICE_TH           0x80            // temperature and humidity sensor's device address

#define REGPRESS_STATUS         0x00            // pressure sensor status register
#define REGPRESS_STATUS_LEN     1
#define REGPRESS_ID             0x0C            // pressure sensor chip ID
#define REGPRESS_ID_LEN         1



enum EI2CStates
{
    sm_none = 0,                                // no operation in progress
    sm_readdev_regaddr_waitaddr,                // read register: sent start condition + address + wrbit - waiting for address sent.
    sm_readdev_regaddr_waittx,                  // read register: sent register address - waiting for tx complete
    sm_readdev_readdata_waitaddr,               // read register: dma set up + start condition + address + rdbit - waiting for addess sent.
    sm_readdev_readdata_waitdata,               // read register: address sent, interface filling memory - wait when DMA transaction is finished
};

struct SI2CStateMachine
{
    enum EI2CStates     sm;
    uint8               *buffer;
    uint8               dev_addr;
    uint8               reg_addr;
    uint8               reg_data_lenght;
} i2c;



//==============================================================
//
//              I2C communication routines
// 
//==============================================================


void local_I2C_internal_setstart( uint8 address, bool wrt )
{
    I2C_PORT_SENSOR->CR1 |= CR1_START_Set;              // Set start condition
    while ((I2C_PORT_SENSOR->SR1 & SR1_SB) != SR1_SB);  // Wait until SB flag is set: EV5

    if ( wrt )
        I2C_PORT_SENSOR->DR = address;
    else
        I2C_PORT_SENSOR->DR = address | 0x0001;         // set the read flag
}

bool local_I2C_internal_waitaddr( void )
{
    if ( I2C_PORT_SENSOR->SR1 & SR1_ADDR )      // EV6
    {
        volatile uint32 temp;
        temp = I2C_PORT_SENSOR->SR2;            // clear (ADDR) by reading SR2

        return true;
    }
    return false;
}

bool local_I2C_internal_waitaddr_sendregaddr( void )
{
    if ( local_I2C_internal_waitaddr() )
    {
        I2C_PORT_SENSOR->DR = i2c.reg_addr;     // send the register address (EV8_1)
        return true;
    }
    return false;
}

bool local_I2C_internal_wait_regaddr_tx_setreceive( void )
{
    if (I2C_PORT_SENSOR->SR1 & SR1_BTF )          // Wait for byte transmission finish including ACK reception (BTF)
    {
        if ( i2c.sm == sm_readdev_regaddr_waittx )
        {
            // if called for data reading

            // set up DMA for receiving data
            HW_DMA_Receive( DMACH_SENS, i2c.buffer, i2c.reg_data_lenght );
            I2C_PORT_SENSOR->CR2 |= CR2_LAST_Set;          // Set Last bit to have a NACK on the last received byte
            I2C_PORT_SENSOR->CR2 |= CR2_DMAEN_Set;         // Enable I2C DMA requests
            // send the start condition and device address
            local_I2C_internal_setstart( i2c.dev_addr, false );
        }
        return true;
    }
    return false;
}

bool local_I2C_internal_waitdata_stop( void )
{
    if ( DMA1->ISR & DMA1_FLAG_TC7 )
    {
        DMA1->IFCR = DMA1_FLAG_TC7;                     // clear the transfer ready flag
        I2C_PORT_SENSOR->CR2 &= CR2_DMAEN_Reset;        // Disable DMA requests for i2c

        I2C_PORT_SENSOR->CR1 |= CR1_STOP_Set;
        while ( I2C_PORT_SENSOR->CR1 & CR1_STOP_Set ); // wait till stop is cleared by hardware

        return true;
    }
    return false;
}


// init I2C peripherial after power-on reset
void local_I2C_interface_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    I2C_InitTypeDef  I2C_InitStructure;

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
    while( !(IO_PORT_SENS_SCL->IDR & IO_OUT_SENS_SCL) || !(IO_PORT_SENS_SCL->IDR & IO_OUT_SENS_SDA) );  // 3. Check SCL and SDA High level in GPIOx_IDR.
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

    // configure the interface
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x0C;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 400000;
    I2C_Init(I2C1, &I2C_InitStructure);
}


// Read a register from a device.
// Note: the routine is asynchronous for larger ammount of data - check data availability and keep
// the buffer available till finishing
uint32 local_I2C_device_read( uint8 dev_address, uint8 dev_reg_addr, uint32 data_lenght, uint8 *buffer )
{
    if ( i2c.sm != sm_none )
        return 1;

    i2c.dev_addr = dev_address;
    i2c.reg_addr = dev_reg_addr;
    i2c.reg_data_lenght = data_lenght;
    i2c.buffer = buffer;
    local_I2C_internal_setstart( dev_address, true );
    i2c.sm = sm_readdev_regaddr_waitaddr;
    return 0;
}


// Check if operations are finished - this polls internal state machines also
uint32 local_I2C_busy( void )
{
    if ( i2c.sm == sm_none )
        return 0;

    switch ( i2c.sm )
    {
        case sm_readdev_regaddr_waitaddr:   
            if (local_I2C_internal_waitaddr_sendregaddr() )
                i2c.sm = sm_readdev_regaddr_waittx;
            break;
        case sm_readdev_regaddr_waittx:
            if (local_I2C_internal_wait_regaddr_tx_setreceive() )
                i2c.sm = sm_readdev_readdata_waitaddr;
            break;
        case sm_readdev_readdata_waitaddr:
            if (local_I2C_internal_waitaddr() )
                i2c.sm = sm_readdev_readdata_waitdata;
            break;
        case sm_readdev_readdata_waitdata:
            if ( local_I2C_internal_waitdata_stop() )
            {
                i2c.sm = sm_none;
                return 0;
            }
            break;
    }
    return 1;
}




void Sensor_Init()
{
    uint8 data[16];

    // init I2C interface
    local_I2C_interface_init();

    // init DMA for I2C
    HW_DMA_Uninit( DMACH_SENS );
    HW_DMA_Init( DMACH_SENS );
    
    local_I2C_device_read( I2C_DEVICE_PRESSURE, REGPRESS_ID, REGPRESS_ID_LEN, data );
    while ( local_I2C_busy() );

    local_I2C_device_read( I2C_DEVICE_PRESSURE, REGPRESS_STATUS, REGPRESS_STATUS_LEN, data+1 );
    while ( local_I2C_busy() );

    local_I2C_device_read( I2C_DEVICE_PRESSURE, REGPRESS_ID, REGPRESS_ID_LEN, data+2 );
    while ( local_I2C_busy() );

    if ( data[3] == data[4] )
    {
        HW_DMA_Uninit( DMACH_SENS );
    }

    // dev experiments


    // read it's value


}

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

