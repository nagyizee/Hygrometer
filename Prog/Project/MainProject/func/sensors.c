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
#define SR1_RXNE                ((uint16_t)0x0040)

#define I2CFAIL_SETST           0x01            // failed setting start/stop condition -> bus error - need to reinit i2c


#define I2CSTATE_NONE           0x00            // i2c busy state none - means no operation is done
#define I2CSTATE_BUSY           0x01            // i2c communication is in progress
#define I2CSTATE_FAIL           0x02            // i2c communication failed - check the error code in i2c.fail_code and take actions


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



//==============================================================
//
//              I2C communication routines
// 
//==============================================================
#define WAIT( a )   i2c.to_ctr = 0;  do   {   i2c.to_ctr++; if (i2c.to_ctr > 400)  goto _failure;   } while( a )


uint32 local_I2C_internal_setstart( uint8 address, bool wrt )
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

bool local_I2C_internal_waitaddr( bool one_byte_rx )
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

bool local_I2C_internal_waitaddr_sendregaddr( void )
{
    if ( local_I2C_internal_waitaddr(false) )
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


bool local_I2C_internal_waitdata_stop( void )
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

bool local_I2C_internal_waitdata_DMA_stop( void )
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

bool local_I2C_internal_waitTX_DMA_stop( void )
{
    if  ( DMA1->ISR & DMA1_FLAG_TC6 )
    {
        DMA_Cmd(DMA_SENS_TX_Channel, DISABLE);
        DMA1->IFCR = DMA1_FLAG_TC6;             // Clear the DMA Transfer complete flag

        return true;
    }
    return false;
}

bool local_I2C_internal_waitTX_finish( void )
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
// Note: the routine is asynchronous - check data availability and keep
// the buffer available till finishing
uint32 local_I2C_device_read( uint8 dev_address, uint8 dev_reg_addr, uint32 data_lenght, uint8 *buffer )
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
        return 1;
    }

    i2c.sm = sm_readdev_regaddr_waitaddr;
    return 0;
}

// Write a register to a device.
// Note: the routine is asynchronous - check data availability and keep
// the buffer available till finishing
// First byte in data buffer is the register address
uint32 local_I2C_device_write( uint8 dev_address, const uint8 *buffer, uint32 data_lenght )
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
        return 1;
    }
    i2c.sm = sm_writedev_regaddr_waitaddr;
    return 0;
}

// Check if operations are finished - this polls internal state machines also
uint32 local_I2C_busy( void )
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



const uint8     set_baro[] = { REGPRESS_CTRL1, 0x38 };      // max oversample
const uint8     set_sshot_baro[] = { REGPRESS_CTRL1, ( pos_128 | PREG_CTRL1_OST) };
const uint8     set_bar_in[] = { REGPRESS_BAR_IN, 0xCC, 0xDD };
const uint8     set_data_event[] = { REGPRESS_DATACFG, PREG_DATACFG_PDEFE };

void Sensor_Init()
{
    uint8 data[16];


    // init I2C interface
    local_I2C_interface_init();

    // init DMA for I2C
    HW_DMA_Uninit( DMACH_SENS );
    HW_DMA_Init( DMACH_SENS );

    HW_LED_Off();

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

