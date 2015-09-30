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
    #define IO_PORT_EE_SB           GPIOA
    #define IO_PORT_DISP_SB         GPIOA
    #define IO_PORT_CAM_PREF        GPIOA
    #define IO_PORT_FW_TX           GPIOA

    #define IO_OUT_EE_SCK           GPIO_Pin_5
    #define IO_OUT_DISP_SCK         GPIO_Pin_5
    #define IO_OUT_EE_MOSI          GPIO_Pin_7
    #define IO_OUT_DISP_MOSI        GPIO_Pin_7
    #define IO_OUT_CAM_PREF         GPIO_Pin_8
    #define IO_OUT_FWUP_TX          GPIO_Pin_9

    #define IO_OUT_DBG_ON_RX        GPIO_Pin_10

    // B
    #define IO_PORT_BUZZER          GPIOB
    #define IO_PORT_EE_CTRL         GPIOB
    #define IO_PORT_DISP_CTRL       GPIOB
    #define IO_PORT_PWR_AN          GPIOB
    #define IO_PORT_DBG_LED         GPIOB
    #define IO_PORT_MIC_MUTE        GPIOB
    #define IO_PORT_CAM_SHTR        GPIOB

    #define IO_OUT_PWR_AN           GPIO_Pin_0
    #define IO_OUT_EE_SEL           GPIO_Pin_1
    #define IO_OUT_DBG_LED          GPIO_Pin_7
    #define IO_OUT_MIC_MUTE         GPIO_Pin_9
    #define IO_OUT_DISP_RESET       GPIO_Pin_10
    #define IO_OUT_DISP_SEL         GPIO_Pin_11
    #define IO_OUT_DISP_PANEL       GPIO_Pin_12
    #define IO_OUT_DISP_DC          GPIO_Pin_13
    #define IO_OUT_BUZZER           GPIO_Pin_14
    #define IO_OUT_CAM_SHTR         GPIO_Pin_15

    #define IO_OUT_DBG_ON_PB8       GPIO_Pin_8

    // C
    #define IO_PORT_PWR             GPIOC

    #define IO_OUT_PWR              GPIO_Pin_13
    
   
    // Inputs
    // A
    #define IO_PORT_EEPROM_IN       GPIOA
    #define IO_PORT_BTN_EMS         GPIOA
    #define IO_PORT_ANALOG_IN       GPIOA
    #define IO_PORT_FW_RX           GPIOA

    #define IO_IN_AN_SND_HI         GPIO_Pin_0
    #define IO_IN_AN_SND_LO         GPIO_Pin_1
    #define IO_IN_AN_LIGHT_LO       GPIO_Pin_2
    #define IO_IN_AN_LIGHT_HI       GPIO_Pin_3
    #define IO_IN_AN_V_BAT          GPIO_Pin_4
    #define IO_IN_SB_MISO           GPIO_Pin_6
    #define IO_IN_FW_RX             GPIO_Pin_10
    #define IO_IN_BTN_ESC           GPIO_Pin_11
    #define IO_IN_BTN_START         GPIO_Pin_12
    #define IO_IN_BTN_MENU          GPIO_Pin_15

    // B
    #define IO_PORT_BTN_MDO         GPIOB

    #define IO_IN_BTN_OK            GPIO_Pin_3
    #define IO_IN_DIAL_P1           GPIO_Pin_4
    #define IO_IN_DIAL_P2           GPIO_Pin_5
    #define IO_IN_BTN_MODE          GPIO_Pin_6


    // PORT SETUPS
    // port A
    #define PORTA_OUTPUT_MODE   GPIO_Mode_Out_PP
    #define PORTA_OUTPUT_PINS   ( IO_OUT_EE_SCK | IO_OUT_DISP_SCK | IO_OUT_EE_MOSI | IO_OUT_DISP_MOSI | IO_OUT_CAM_PREF | IO_OUT_FWUP_TX | IO_IN_FW_RX \
                                ) //| IO_OUT_DBG_ON_RX )        // REMOVE THIS LINE - debug purpose only
    #define PORTA_INPUT_MODE_SB GPIO_Mode_IPD
    #define PORTA_INPUT_PINS_SB ( IO_IN_SB_MISO  )
    #define PORTA_INPUT_MODE    GPIO_Mode_IPU
    #define PORTA_INPUT_PINS    ( /*IO_IN_FW_RX |*/ IO_IN_BTN_ESC | IO_IN_BTN_START | IO_IN_BTN_MENU )
    #define PORTA_ANALOG_MODE   GPIO_Mode_AIN
    #define PORTA_ANALOG_PINS   ( IO_IN_AN_LIGHT_HI | IO_IN_AN_LIGHT_LO | IO_IN_AN_SND_HI | IO_IN_AN_SND_LO | IO_IN_AN_V_BAT )

    // port B
    #define PORTB_OUTPUT_MODE   GPIO_Mode_Out_PP
    #define PORTB_OUTPUT_PINS   ( IO_OUT_PWR_AN | IO_OUT_DBG_LED | IO_OUT_EE_SEL | IO_OUT_MIC_MUTE | IO_OUT_DISP_RESET | IO_OUT_DISP_SEL | IO_OUT_DISP_DC  | IO_OUT_DISP_PANEL  | IO_OUT_BUZZER | IO_OUT_CAM_SHTR \
                                ) //| IO_OUT_DBG_ON_PB8 )       // REMOVE THIS LINE - debug purpose only

    #define PORTB_INPUT_MODE    GPIO_Mode_IPU
    #define PORTB_INPUT_PINS    ( IO_IN_BTN_OK | IO_IN_DIAL_P1 | IO_IN_DIAL_P2 )
    #define PORTB_INPUT_MODE_M  GPIO_Mode_IN_FLOATING
    #define PORTB_INPUT_PIN_M   ( IO_IN_BTN_MODE )

    // port C
    #define PORTC_OUTPUT_MODE   GPIO_Mode_Out_PP
    #define PORTC_OUTPUT_PINS   ( IO_OUT_PWR )


    #define STACK_CHECK_WORD    0x11223344

    // fOsc = 8MHz, - to get 100us we need 2400 clock cycles
    #define SYSTEM_T_10MS_COUNT     20               // 10ms is 40 timer events
    #define SYSTEM_MAX_TICK         (4000 - 1)       // timer ticks for 500us
    #define BUZZER_FREQ             888              // obtaining 9kHz

    #define TIMER_SYSTEM            TIM16               // used as system timer and pwm generator for display backlight and contrast
    #define TIMER_BUZZER            TIM15               // used as system timer and pwm generator for display backlight and contrast
    #define TIMER_ADC               TIM3                // used as timebase for ADC aquisitions
    #define TIMER_SYSTEM_IRQ        TIM1_UP_TIM16_IRQn
    #define TIMER_PRETRIGGER_IRQ    TIM3_IRQn
    #define TIMER_RTC               RTC
    #define RTC_ALARM_FLAG          ((uint16_t)0x0002)


    #define EE_SPI      SPI1
    #define EE_APB      RCC_APB2Periph_SPI1

    #define SPI_DMA_Channel         DMA1_Channel3  
    #define SPI_DMA_IRQ             DMA1_Channel3_IRQn
    #define SPI_DMA_IRQ_FLAGS       ((uint32_t)(DMA_ISR_GIF3 | DMA_ISR_TCIF3 | DMA_ISR_HTIF3 | DMA_ISR_TEIF3))
    #define ADC_DMA_Channel         DMA1_Channel1  
    #define ADC_DMA_IRQ             DMA1_Channel1_IRQn
    #define ADC_DMA_IRQ_FLAGS       ((uint32_t)(DMA_ISR_GIF1 | DMA_ISR_TCIF1 | DMA_ISR_HTIF1 | DMA_ISR_TEIF1))

    #define ADC_IF_EOC                  ((uint32_t)0x00000002)
    #define ADC_IE_EOC                  ((uint32_t)0x00000020)
    
    #define VBAT_MIN        0xB16                   // 2.6V
    #define VBAT_MAX        0xD31                   // 3.1V
    #define VBAT_DIFF       ( VBAT_MAX - VBAT_MIN )

    #define ENCODER_MAX 32


    // all these modes are mutally exclussive per group
    #define SYSSTAT_DISP_BUSY           0x0001          // flag indicating that display is busy
    #define SYSSTAT_CORE_BULK           0x0002          // core needs bulk calculation, need to run main loop continuously
    #define SYSSTAT_CORE_RUN_FULL       0x0004          // core run with light or sound detection, may sleep at end of main loop
    #define SYSSTAT_CORE_RUN_LOW        0x0008          // core run with low speed operations. execution is needed only in 1sec. interval
    #define SYSSTAT_CORE_STOPPED        0x0010          // core stopped. no operation in progress
    #define SYSSTAT_UI_ON               0x0020          // ui is fully functional, display is eventually dimmed 
    #define SYSSTAT_UI_STOPPED          0x0080          // ui stopped, wake up on keypress but keys are not captured
    #define SYSSTAT_UI_STOP_W_ALLKEY    0x0100          // ui stopped, wake up with immediate key action for any key
    #define SYSSTAT_UI_STOP_W_SKEY      0x0200          // ui stopped, wake up with immediate key action for 'Start' key
    #define SYSSTAT_UI_PWROFF           0x8000          // ui.pwroff timeout reached and no user action and no core run. OR power button (long press on pwr/mode button)


    #define HWSLEEP_FULL            0x00            // continuous main loop, no sleep mode in use
    #define HWSLEEP_WAIT            0x01            // sleepmode - wait for irq - cpu clock down only - sysclock is working
    #define HWSLEEP_STOP            0x02            // stop mode - wake-up at keypress or rtc 1sec. clock
    #define HWSLEEP_STOP_W_ALLKEYS  0x03            // same as the mode abowe - but all keys are detected with rising edge - to generate UI keyboard event also 
    #define HWSLEEP_STOP_W_SKEY     0x04            // same as HWSLEEP_STOP - but 'Start' key will be detected with rising edge
    #define HWSLEEP_OFF             0x05            // shuts down the system


    void InitHW(void);

    void HW_SPI_interface_init(uint16 baudrate);


    #define BtnGet_OK()         ( (IO_PORT_BTN_MDO->IDR & IO_IN_BTN_OK) ==0)
    #define BtnGet_Cancel()     ( (IO_PORT_BTN_EMS->IDR & IO_IN_BTN_ESC) ==0 )
    #define BtnGet_Power()      ( (IO_PORT_BTN_MDO->IDR & IO_IN_BTN_MODE) )
    #define BtnGet_Menu()       ( (IO_PORT_BTN_EMS->IDR & IO_IN_BTN_MENU) ==0 )
    #define BtnGet_StartStop()  ( (IO_PORT_BTN_EMS->IDR & IO_IN_BTN_START) ==0 )

#if 1       // SET THIS TO 1  - 0 for dbg purpose 
    #define HW_LED_Off()        do {  IO_PORT_DBG_LED->BRR = IO_OUT_DBG_LED;   } while (0)
    #define HW_LED_On()         do {  IO_PORT_DBG_LED->BSRR = IO_OUT_DBG_LED;  } while (0)
#else
    #define HW_LED_Off()        do {    } while (0)
    #define HW_LED_On()         do {    } while (0)
#endif

    #define HW_DBG_Off()        do {  IO_PORT_FW_TX->BRR = IO_OUT_FWUP_TX;   } while (0)
    #define HW_DBG_On()         do {  IO_PORT_FW_TX->BSRR = IO_OUT_FWUP_TX;  } while (0)
    #define HW_DBG2_Off()        do {  IO_PORT_FW_RX->BRR = IO_IN_FW_RX;   } while (0)      // hack - firmware load RX - set to output
    #define HW_DBG2_On()         do {  IO_PORT_FW_RX->BSRR = IO_IN_FW_RX;  } while (0)      // hack - firmware load RX - set to output

    #define HW_PWR_Dig_Off()    do {  IO_PORT_PWR->BRR = IO_OUT_PWR;   } while (0)
    #define HW_PWR_Dig_On()     do {  IO_PORT_PWR->BSRR = IO_OUT_PWR;  } while (0)
    #define HW_PWR_An_Off()     do {  IO_PORT_PWR_AN->BSRR = IO_OUT_PWR_AN;   } while (0)
    #define HW_PWR_An_On()      do {  IO_PORT_PWR_AN->BRR = IO_OUT_PWR_AN;  } while (0)
    #define HW_Disp_VPanelOff()     do {  IO_PORT_DISP_CTRL->BRR = IO_OUT_DISP_PANEL;    } while (0)    // turn off Vpanel
    #define HW_Disp_VPanelOn()      do {  IO_PORT_DISP_CTRL->BSRR = IO_OUT_DISP_PANEL;   } while (0)    // turn on Vpanel

    #define HW_Shutter_Prefocus_ON()  do {  IO_PORT_CAM_PREF->BSRR = IO_OUT_CAM_PREF;   } while (0)         // set signal to low to chip select
    #define HW_Shutter_Prefocus_OFF() do {  IO_PORT_CAM_PREF->BRR = IO_OUT_CAM_PREF;    } while (0)         // set signal to high to chip disable
    #define HW_Shutter_Release_ON()   do {  IO_PORT_CAM_SHTR->BSRR = IO_OUT_CAM_SHTR;   } while (0)         // set signal to low to chip select
    #define HW_Shutter_Release_OFF()  do {  IO_PORT_CAM_SHTR->BRR = IO_OUT_CAM_SHTR;    } while (0)         // set signal to high to chip disable

    #define HW_EEProm_Enable()  do {  IO_PORT_EE_CTRL->BRR = IO_OUT_EE_SEL;      } while (0)         // set signal to low to chip select
    #define HW_EEProm_Disable() do {  IO_PORT_EE_CTRL->BSRR = IO_OUT_EE_SEL;     } while (0)         // set signal to high to chip disable

    #define HW_Disp_Enable()        do {  IO_PORT_DISP_CTRL->BRR = IO_OUT_DISP_SEL;    } while (0)     // 0 to /CS pin
    #define HW_Disp_Disable()       do {  IO_PORT_DISP_CTRL->BSRR = IO_OUT_DISP_SEL;   } while (0)     // 1 to /CS pin

    #define HW_Disp_Reset()         do {  IO_PORT_DISP_CTRL->BRR = IO_OUT_DISP_RESET;    } while (0)    //signal low on reset
    #define HW_Disp_UnReset()       do {  IO_PORT_DISP_CTRL->BSRR = IO_OUT_DISP_RESET;   } while (0)    //signal high on reset

    #define HW_Disp_BusData()       do {  IO_PORT_DISP_CTRL->BSRR = IO_OUT_DISP_DC;  } while (0)     // 1 for data    
    #define HW_Disp_BusCommand()    do {  IO_PORT_DISP_CTRL->BRR = IO_OUT_DISP_DC;   } while (0)     // 0 for command   

    #define HW_SPI_DMA_Enable()      do {  EE_SPI->CR2 |= SPI_I2S_DMAReq_Tx;  } while (0)    // enable DMA request for SPI port
    #define HW_SPI_DMA_Disable()      do {  EE_SPI->CR2 &= (uint16_t)~SPI_I2S_DMAReq_Tx; } while (0)    // disable DMA request for SPI port
    void HW_SPI_DMA_Init();
    void HW_SPI_DMA_Uninit();
    void HW_SPI_DMA_Send( const uint8* data, uint32 size );

    uint32 HW_ADC_GetBattery(void); // NOTE!!! call this only if ADC isn't running with DMA bulk mode
    void HW_ADC_StartAcquisition( uint32 buffer_addr, int group );
    void HW_ADC_StopAcquisition( void );
    void HW_ADC_SetupPretrigger( uint32 value );
    void HW_ADC_ResetPretrigger( void );

    void HW_Seconds_Start(void);    // set up RTC 1 second interrupt for period beginning from this moment
    void HW_Seconds_Restore(void);  // restore the original interrupt interval

    void HW_Buzzer_On(int pulse);   // Pulse is in 8MHz units
    void HW_Buzzer_Off(void);


    #define HW_ASSERT()         do { /*TODO */ } while(0)       

    uint32 BtnGet_Encoder();

    void HW_Encoder_Poll(void);


    // power management
    bool HW_Sleep( int mode );


#endif
