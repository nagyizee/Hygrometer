/*
        Sensor interface routines

        It works based on polling. Since I2C is slow, most of the operations 
        are done asynchronously so freqvent polling is needed

        Estimated i2c speed: 400kHz -> 40kbps - 25us/byte

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




void local_I2C_interface_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    I2C_InitTypeDef  I2C_InitStructure;
    uint16 reg16;

    // enable clock for I2C interface
    RCC_APB1PeriphClockCmd( I2C_APB_SENSOR, ENABLE );

    // set up GPI port for i2c
    GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin     = IO_OUT_SENS_SCL;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_OD;
    GPIO_Init(IO_PORT_SENS_SCL, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin     = IO_IN_SENS_SDA;
    GPIO_Init(IO_PORT_SENS_SDA, &GPIO_InitStructure);

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





void Sensor_Init()
{
    volatile uint32 temp;
    volatile uint8 data;
    // init I2C interface
    local_I2C_interface_init();


    // dev experiments

    // set up register to read in pressure sensor
    I2C_PORT_SENSOR->CR1 |= CR1_START_Set;              // Set start condition
    while ((I2C_PORT_SENSOR->SR1&0x0001) != 0x0001);    // Wait until SB flag is set: EV5
                                                        // 
    I2C_Send7bitAddress( I2C_PORT_SENSOR, 0xC0, I2C_Direction_Transmitter );
    while ((I2C_PORT_SENSOR->SR1&0x0002) != 0x0002);    // Wait for Adress sent (ADDR)
    temp = I2C_PORT_SENSOR->SR2;                        // clear (ADDR) by reading SR2

    I2C_PORT_SENSOR->DR = 0x0C;                         // send:  register "get ID" address
    while ((I2C_PORT_SENSOR->SR1 & 0x00004) != 0x000004);          // Wait for byte transmission including ACK reception (BTF)

    // read it's value
    I2C_PORT_SENSOR->CR1 |= CR1_START_Set;              // Set start condition again - now for reception
    while ((I2C_PORT_SENSOR->SR1&0x0001) != 0x0001);    // Wait until SB flag is set: EV5  (it is cleared by reading SR1 and writing DR)

    I2C_Send7bitAddress( I2C_PORT_SENSOR, 0xC0, I2C_Direction_Receiver );
    while ((I2C_PORT_SENSOR->SR1&0x0002) != 0x0002);    // Wait for Adress sent (ADDR)

    I2C_PORT_SENSOR->CR1 &= CR1_ACK_Reset;
    __disable_irq();
    temp = I2C_PORT_SENSOR->SR2;                        // clear (ADDR)
    I2C_PORT_SENSOR->CR1 |= CR1_STOP_Set;                          // set STOP
    __enable_irq();
    
    while ((I2C_PORT_SENSOR->SR1 & 0x00040) != 0x000040);          // Wait until a data is received in DR register (RXNE = 1) EV7
    data = I2C_PORT_SENSOR->DR;                                    // Read the data
    while ((I2C_PORT_SENSOR->CR1&0x200) == 0x200);                 // Make sure that the STOP bit is cleared by Hardware before CR1 write access */
    I2C_PORT_SENSOR->CR1 |= CR1_ACK_Set;                           // Enable Acknowledgement to be ready for another reception

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

