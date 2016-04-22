/**
  ******************************************************************************
  *  hardware related defines and low level access routines
  * 
  *  - for pin mapping see the excell document
  ******************************************************************************
  */ 

#ifndef __HW_STUFF_H
#define __HW_STUFF_H

    #include "stm32f10x.h"
    #include "typedefs.h"

    // Outputs
    // A
    #define IO_PORT_COMM_TX         GPIOA
    #define IO_PORT_EE_SPI          GPIOA
    #define IO_PORT_DISP_RESET      GPIOA
    #define IO_PORT_DISP_CS         GPIOA
    #define IO_PORT_DISP_PWR        GPIOA

    #define IO_OUT_COMM_TX          GPIO_Pin_2
    #define IO_OUT_EE_SCK           GPIO_Pin_5
    #define IO_OUT_EE_MOSI          GPIO_Pin_7
    #define IO_OUT_DISP_RESET       GPIO_Pin_8
    #define IO_OUT_DISP_CS          GPIO_Pin_9
    #define IO_OUT_DISP_PWR         GPIO_Pin_10

    // B
    #define IO_PORT_EE_CS           GPIOB
    #define IO_PORT_LED             GPIOB
    #define IO_PORT_SENS_SCL        GPIOB
    #define IO_PORT_BUZZER          GPIOB
    #define IO_PORT_DISP_SPI        GPIOB
    #define IO_PORT_DISP_DC         GPIOB

    #define IO_OUT_EE_CS            GPIO_Pin_0
    #define IO_OUT_LED              GPIO_Pin_1
    #define IO_OUT_SENS_SCL         GPIO_Pin_6
    #define IO_OUT_SENS_SDA         GPIO_Pin_7
    #define IO_OUT_BUZZER           GPIO_Pin_8
    #define IO_OUT_DISP_SCK         GPIO_Pin_13
    #define IO_OUT_DISP_DC          GPIO_Pin_14
    #define IO_OUT_DISP_MOSI        GPIO_Pin_15

    // C
    #define IO_PORT_PON             GPIOC

    #define IO_OUT_PON              GPIO_Pin_13
    
    // Inputs
    // A
    #define IO_PORT_AN_VBAT         GPIOA
    #define IO_PORT_COMM_RX         GPIOA
    #define IO_PORT_BTN_OK_ESC_PP_3 GPIOA
      
    #define IO_IN_AN_VBAT           GPIO_Pin_0
    #define IO_IN_BTN_ESC           GPIO_Pin_1
    #define IO_IN_COMM_RX           GPIO_Pin_3
    #define IO_IN_EE_MISO           GPIO_Pin_6
    #define IO_IN_BTN_PP            GPIO_Pin_11
    #define IO_IN_BTN_3             GPIO_Pin_12
    #define IO_IN_BTN_OK            GPIO_Pin_15

    // B
    #define IO_PORT_BTN_1_4_6       GPIOB   
    #define IO_PORT_SENS_SDA        GPIOB 
    #define IO_PORT_SENS_IRQ        GPIOB
    #define IO_PORT_CHARGE          GPIOB

    #define IO_IN_BTN_1             GPIO_Pin_3
    #define IO_IN_BTN_4             GPIO_Pin_4
    #define IO_IN_BTN_6             GPIO_Pin_5
    #define IO_IN_SENS_IRQ          GPIO_Pin_9
    #define IO_IN_CHARGE            GPIO_Pin_12

    // PORT SETUPS
    // port A
    #define PORTA_OUTPUT_MODE       GPIO_Mode_Out_PP
    #define PORTA_OUTPUT_PINS       ( IO_OUT_COMM_TX | IO_OUT_EE_SCK | IO_OUT_EE_MOSI | IO_OUT_DISP_RESET | IO_OUT_DISP_CS | IO_OUT_DISP_PWR )

    #define PORTA_INPUT_MODE_BTN    GPIO_Mode_IPU
    #define PORTA_INPUT_PINS_BTN    ( IO_IN_BTN_ESC | IO_IN_BTN_3 | IO_IN_BTN_OK )
    #define PORTA_INPUT_MODE_EE     GPIO_Mode_IPD
    #define PORTA_INPUT_PINS_EE     ( IO_IN_EE_MISO | IO_IN_COMM_RX )
    #define PORTA_INPUT_MODE_FLOAT  GPIO_Mode_IN_FLOATING;
    #define PORTA_INPUT_PINS_FLOAT  ( IO_IN_BTN_PP )

    #define PORTA_ANALOG_MODE       GPIO_Mode_AIN
    #define PORTA_ANALOG_PINS       ( IO_IN_AN_VBAT )

    // port B
    #define PORTB_OUTPUT_MODE       GPIO_Mode_Out_PP 
    #define PORTB_OUTPUT_PINS       ( IO_OUT_EE_CS | IO_OUT_LED | IO_OUT_BUZZER | IO_OUT_DISP_SCK | IO_OUT_DISP_DC | IO_OUT_DISP_MOSI )
    #define PORTB_OUTPUT_MODE_I2C   GPIO_Mode_Out_OD 
    #define PORTB_OUTPUT_PINS_I2C   ( IO_OUT_SENS_SCL )

    #define PORTB_INPUT_MODE_BTN    GPIO_Mode_IPU
    #define PORTB_INPUT_PINS_BTN    ( IO_IN_BTN_1 | IO_IN_BTN_4 | IO_IN_BTN_6 )
    #define PORTB_INPUT_MODE        GPIO_Mode_IN_FLOATING
    #define PORTB_INPUT_PINS        ( IO_OUT_SENS_SDA | IO_IN_SENS_IRQ | IO_IN_CHARGE )

    // port C
    #define PORTC_OUTPUT_MODE       GPIO_Mode_Out_PP
    #define PORTC_OUTPUT_PINS       ( IO_OUT_PON )


    #define STACK_CHECK_WORD    0x11223344

    // fOsc = 16MHz, - to get 1ms we need 16000 clock cycles
    #define SYSTEM_T_10MS_COUNT     10               // 10ms is 10 timer events
    #define SYSTEM_MAX_TICK         (16000 - 1)      // timer ticks for 1ms
    #define BUZZER_FREQ             1776             // obtaining 9kHz

    #define TIMER_SYSTEM            TIM15               // used as system timer
    #define TIMER_BUZZER            TIM16               // buzzer pwm generator
    #define TIMER_SYSTEM_IRQ        TIM1_BRK_TIM15_IRQn
    #define TIMER_RTC               RTC
    #define TIMER_RTC_PRESCALE      ( ((32 * 1024) / 2) - 1 )   // 0.5sec pulses
    #define RTC_ALARM_FLAG          ((uint16_t)0x0002)

    #define SPI_PORT_EE             SPI1                    // spi port for eeprom
    #define SPI_APB_EE              RCC_APB2Periph_SPI1     // apb bus and module for eeprom spi
    #define SPI_PORT_DISP           SPI2                    // spi port for display
    #define SPI_APB_DISP            RCC_APB1Periph_SPI2     // apb bus and module for display spi
    #define I2C_PORT_SENSOR         I2C1                    // i2c port for sensors
    #define I2C_APB_SENSOR          RCC_APB1Periph_I2C1     // apb bus and module for sensors i2c
    #define UART_PORT_COMM          USART2                  // uart port for communication
    #define UART_APB_COMM           RCC_APB1Periph_USART2   // apb bus and module for comm. uart            

    // NOTE: UART should be limmitted to 1Mbps beacuse of the 4k7 resistor in between uC and USB chip - pin capacitance gives 6MHz bandwidth only

    #define DMA_EE_TX_Channel       DMA1_Channel3  
    #define DMA_EE_TX_IRQ           DMA1_Channel3_IRQn
    #define DMA_EE_TX_IRQ_FLAGS     ((uint32_t)(DMA_ISR_GIF3 | DMA_ISR_TCIF3 | DMA_ISR_HTIF3 | DMA_ISR_TEIF3))
    #define DMA_EE_RX_Channel       DMA1_Channel2  
    #define DMA_EE_RX_IRQ           DMA1_Channel2_IRQn
    #define DMA_EE_RX_IRQ_FLAGS     ((uint32_t)(DMA_ISR_GIF2 | DMA_ISR_TCIF2 | DMA_ISR_HTIF2 | DMA_ISR_TEIF2))
    #define DMA_DISP_TX_Channel     DMA1_Channel5  
    #define DMA_DISP_TX_IRQ         DMA1_Channel5_IRQn
    #define DMA_DISP_IRQ_FLAGS      ((uint32_t)(DMA_ISR_GIF5 | DMA_ISR_TCIF5 | DMA_ISR_HTIF5 | DMA_ISR_TEIF5))
    #define DMA_SENS_TX_Channel     DMA1_Channel6 
    #define DMA_SENS_TX_IRQ         DMA1_Channel6_IRQn
    #define DMA_SENS_TX_IRQ_FLAGS   ((uint32_t)(DMA_ISR_GIF6 | DMA_ISR_TCIF6 | DMA_ISR_HTIF6 | DMA_ISR_TEIF6))
    #define DMA_SENS_RX_Channel     DMA1_Channel7  
    #define DMA_SENS_RX_IRQ         DMA1_Channel7_IRQn
    #define DMA_SENS_RX_IRQ_FLAGS   ((uint32_t)(DMA_ISR_GIF7 | DMA_ISR_TCIF7 | DMA_ISR_HTIF7 | DMA_ISR_TEIF7))
    #define DMA_COMM_TX_Channel     DMA_SENS_RX_Channel  
    #define DMA_COMM_TX_IRQ         DMA_SENS_RX_IRQ
    #define DMA_COMM_TX_IRQ_FLAGS   DMA_SENS_RX_IRQ_FLAGS

    #define VBAT_MIN        0xB90       // 3.2V
    #define VBAT_MAX        0xF23       // 4.2V
    #define VBAT_DIFF       ( VBAT_MAX - VBAT_MIN )

    // power modes
    enum EPowerMode         //old: HWSLEEP_FULL
    {
        pm_full = 0,            // full cpu power
        pm_sleep,               // executes one loop after an interrupt source ( interrupt sources in simu are - 1ms tick timer, 0.5s (or scheduled) RTC timer ticks )
        pm_hold_btn,            // CPU core/periph. stopped, Exti and RTC wake-up only - wake up uppon button operation and RTC alarm
        pm_hold,                // CPU core/periph. stopped, RTC wake-up and Pwr button wake up only.
        pm_down,                // all electronics switched off, RTC alarm will wake it up, Starts from reset state
                                //    - in simulation - app. will respond only for pwr button and RTC alarm, by starting from INIT
    };

    // power mode bitmasks
    #define PM_FULL     (1<<pm_full)
    #define PM_SLEEP    (1<<pm_sleep)
    #define PM_HOLD_BTN (1<<pm_hold_btn)
    #define PM_HOLD     (1<<pm_hold)
    #define PM_DOWN     (1<<pm_down)
    #define PM_MASK     0xff

    // power modes/component
    // all these modes are mutally exclussive per group
    #define SYSSTAT_DISP_BUSY           PM_SLEEP        // flag indicating that display is busy

    // wake up reasons
    #define WUR_NONE        0x00
    #define WUR_FIRST       0x01        // first startup
    #define WUR_USR         0x02        // user produced wake-up condition (button pressed) - from InitHW (reset state): when Power button pressed by user
                                        //                                                  - from HW_Sleep(): when Power or other buttons are pressed by user (power mode dependent)
    #define WUR_RTC         0x04        // timer produced wake-up condition - from InitHW or HW_Sleep(): when RTC counter = Alarm counter  - can be combined with WUR_USR if button pressed 
    #define WUR_SENS_IRQ    0x08        // sensor IRQ line produced wake-up from HW_Sleep() - when IRQ line is toggled in stopped state


    void SetSysClock(void);

    void InitHW(void);

    void HW_SPI_interface_init( SPI_TypeDef* spi, uint16 baudrate);
    void HW_SPI_Set_Rx_mode_only( SPI_TypeDef* spi, bool on );

    // get button status
    bool BtnGet_OK();           // NOTE: this should be called first from the sequence of OK/Up/Down/Left/Right
    bool BtnGet_Up();           //
    bool BtnGet_Down();         //  
    bool BtnGet_Left();         //    reading any of these should be preceded by BtnGet_OK()
    bool BtnGet_Right();        //   

    void HWDBG_print( uint32 data );
    void HWDBG_print32( uint32 data );


    #define BtnGet_Esc()       ( (IO_PORT_BTN_OK_ESC_PP_3->IDR & IO_IN_BTN_ESC) == 0 )
    #define BtnGet_Mode()      ( (IO_PORT_BTN_OK_ESC_PP_3->IDR & IO_IN_BTN_PP) )

    #define BtnIs_1()          ( (IO_PORT_BTN_1_4_6->IDR & IO_IN_BTN_1) == 0 )
    #define BtnIs_3()          ( (IO_PORT_BTN_OK_ESC_PP_3->IDR & IO_IN_BTN_3) == 0 )
    #define BtnIs_4()          ( (IO_PORT_BTN_1_4_6->IDR & IO_IN_BTN_4) == 0 )
    #define BtnIs_6()          ( (IO_PORT_BTN_1_4_6->IDR & IO_IN_BTN_6) == 0 )

    #define BtnIs_Center()     ( (IO_PORT_BTN_OK_ESC_PP_3->IDR & IO_IN_BTN_OK) == 0 )  

#if 1       // SET THIS TO 1  - 0 for dbg purpose 
    #define HW_LED_Off()        do {  IO_PORT_LED->BRR = IO_OUT_LED;   } while (0)
    #define HW_LED_On()         do {  IO_PORT_LED->BSRR = IO_OUT_LED;  } while (0)
#else
    #define HW_LED_Off()        do {    } while (0)
    #define HW_LED_On()         do {    } while (0)
#endif

    #define HW_PSens_IRQ()      ( IO_PORT_SENS_IRQ->IDR & IO_IN_SENS_IRQ )
    #define HW_Charge_Detect()  ( IO_PORT_CHARGE->IDR & IO_IN_CHARGE )

    // power switches
    #define HW_PWR_Main_Off()   do {  IO_PORT_PON->BRR = IO_OUT_PON;   } while (0)
    #define HW_PWR_Main_On()    do {  IO_PORT_PON->BSRR = IO_OUT_PON;  } while (0)
    #define HW_PWR_Disp_Off()   do {  IO_PORT_DISP_PWR->BRR = IO_OUT_DISP_PWR;    } while (0)    // turn off Vpanel
    #define HW_PWR_Disp_On()    do {  IO_PORT_DISP_PWR->BSRR = IO_OUT_DISP_PWR;   } while (0)    // turn on Vpanel
    
    // chip switches
    #define HW_Chip_EEProm_Enable()  do {  IO_PORT_EE_CS->BRR = IO_OUT_EE_CS;      } while (0)         // set signal to low to chip select
    #define HW_Chip_EEProm_Disable() do {  IO_PORT_EE_CS->BSRR = IO_OUT_EE_CS;     } while (0)         // set signal to high to chip disable
    #define HW_Chip_Disp_Enable()        do {  IO_PORT_DISP_CS->BRR = IO_OUT_DISP_CS;    } while (0)     // 0 to /CS pin
    #define HW_Chip_Disp_Disable()       do {  IO_PORT_DISP_CS->BSRR = IO_OUT_DISP_CS;   } while (0)     // 1 to /CS pin

    #define HW_Chip_Disp_Reset()         do {  IO_PORT_DISP_RESET->BRR = IO_OUT_DISP_RESET;    } while (0)    //signal low on reset
    #define HW_Chip_Disp_UnReset()       do {  IO_PORT_DISP_RESET->BSRR = IO_OUT_DISP_RESET;   } while (0)    //signal high on reset
    #define HW_Chip_Disp_BusData()       do {  IO_PORT_DISP_DC->BSRR = IO_OUT_DISP_DC;  } while (0)     // 1 for data    
    #define HW_Chip_Disp_BusCommand()    do {  IO_PORT_DISP_DC->BRR = IO_OUT_DISP_DC;   } while (0)     // 0 for command   

    // dma device control
    #define DMAREQ_TX               SPI_I2S_DMAReq_Tx
    #define DMAREQ_RX               SPI_I2S_DMAReq_Rx

    #define HW_DMA_EE_Enable(a)      do {  SPI_PORT_EE->CR2 |= (a);  } while (0)                // enable DMA request for SPI port
    #define HW_DMA_EE_Disable(a)     do {  SPI_PORT_EE->CR2 &= (uint16_t)~(a); } while (0)    // disable DMA request for SPI port
    #define HW_DMA_Disp_Enable(a)    do {  SPI_PORT_DISP->CR2 |= (a);  } while (0)              // enable DMA request for SPI port
    #define HW_DMA_Disp_Disable(a)   do {  SPI_PORT_DISP->CR2 &= (uint16_t)~(a); } while (0)    // disable DMA request for SPI port


    #define DMACH_DISP          1       
    #define DMACH_EE            2
    #define DMACH_SENS          3

    void HW_DMA_Init( uint32 dma_ch );
    void HW_DMA_Uninit( uint32 dma_ch );
    void HW_DMA_Send( uint32 dma_ch, const uint8 *ptr, uint32 size );
    void HW_DMA_Receive( uint32 dma_ch, const uint8 *ptr, uint32 size );

    uint32 HW_ADC_GetBattery(void); // NOTE!!! call this only if ADC isn't running with DMA bulk mode

    void HW_Seconds_Start(void);    // set up RTC 1 second interrupt for period beginning from this moment
    void HW_Seconds_Restore(void);  // restore the original interrupt interval
    void HW_SetRTC(uint32 RTCctr);
    void HW_SetRTC_NextAlarm( uint32 alarm );   // set up the next alarm point - will be effective only at stop/power off sleep modes

    void HW_Delay(uint32 us);

    void HW_Buzzer_On(int pulse);   // Pulse is in 16MHz units
    void HW_Buzzer_Off(void);

    #define HW_ASSERT()         do { /*TODO */ } while(0)       

    // power management - enter in sleep or power down - returns the wake-up reason
    uint32 HW_Sleep( enum EPowerMode mode );
    // gets the wake-up reason in this loop (main loop or startup till HW_Sleep)
    uint32 HW_GetWakeUpReason(void);

#endif


/*********************************************
Power caracterizations [sw tag: PWR_01 ]:

Charging:
    - USB battery charge from 3.62V -> 322mA 

Consumptions:
    - using basic main loop without any functionality
      circuits are powered up, everything in initial state, no measurements, display or comm.
      power scheme at 3.65V:
        - full CPU cycle + LED:                 9.96mA
        - cpu SLEEP, everything powered         4.65mA
        - cpu STOP, everything powered          1.7mA
        - power down, RTC alarm only            13uA


    











**********************************************/
