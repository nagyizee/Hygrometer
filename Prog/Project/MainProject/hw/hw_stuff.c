#include "hw_stuff.h"

    
extern void TimerRTCIntrHandler(void);

    static volatile uint32 wakeup_reason = WUR_NONE;
    static volatile uint32 rtc_next_alarm = 0;
    static volatile uint32 btn_press = 0;

    
    static inline bool local_is_rtc_alarm( void )    
    {
        return ( RTC_GetCounter() == ( ((uint32)(RTC->ALRH) << 16) | RTC->ALRL ) );
    }



    void HWDBG_print( uint32 data )
    {
        uint32 i=8;

        HW_LED_Off();
        HW_LED_On();
        HW_LED_Off();
        __asm("    nop\n"); 
        __asm("    nop\n"); 
        do
        {
            HW_LED_On();
            if ( (data & 0x80) == 0 )
            {
                HW_LED_Off();
                __asm("    nop\n"); 
                __asm("    nop\n"); 
                __asm("    nop\n"); 
                __asm("    nop\n"); 
            }
            else
            {
                __asm("    nop\n"); 
                __asm("    nop\n"); 
                __asm("    nop\n"); 
                __asm("    nop\n"); 
                HW_LED_Off();
            }
            data = (data << 1);
            i--;
        } while (i);
        HW_LED_On();
    }

    void HWDBG_print32( uint32 data )
    {
        HWDBG_print( (data >> 24) & 0xff );
        HWDBG_print( (data >> 16) & 0xff );
        HWDBG_print( (data >> 8) & 0xff );
        HWDBG_print( (data >> 0) & 0xff );
    }




    void InitHW(void)
    {
        /*!< At this stage the microcontroller clock setting is already configured, 
             this is done through SystemInit() function which is called from startup
             file (startup_stm32f10x_xx.s) before to branch to application main.
             To reconfigure the default setting of SystemInit() function, refer to
             system_stm32f10x.c file

             Use VECT_TAB_SRAM in env. variables if code runs in SRAM
           */     
        // Clock and Core is initted allready, SystemInit() is called at start-up
        // we will proceed with the peripherial setup

        GPIO_InitTypeDef            GPIO_InitStructure;
        NVIC_InitTypeDef            NVIC_InitStructure;
        TIM_TimeBaseInitTypeDef     TIM_tb;
        TIM_OCInitTypeDef           TIM_oc = {0, };

        __disable_interrupt();

        RCC_AdjustHSICalibrationValue( 0x0F );      //TBD

        // Vectors are set up at SystemInit, set up only priority group
        NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);

        // ----- Setup HW modules -----
        // Enable clock and release reset
        RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO  |
                               RCC_APB2Periph_TIM15 | RCC_APB2Periph_TIM16 | RCC_APB2Periph_ADC1 , ENABLE);
        RCC_APB2PeriphResetCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC |
                               RCC_APB2Periph_TIM15 | RCC_APB2Periph_TIM16 | RCC_APB2Periph_ADC1 , DISABLE);

        RCC_ADCCLKConfig(RCC_PCLK2_Div4);   // set up ADC clock

        // Make the remapping and Disable JTDI (PA15), JTDO (PB3), JTRST (PB4)
        GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable , ENABLE);

        // Init portA
        GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_2MHz;        // begin with port C since this is the power holder
        GPIO_InitStructure.GPIO_Mode    = PORTC_OUTPUT_MODE;        // begin with port C since this is the power holder
        GPIO_InitStructure.GPIO_Pin     = PORTC_OUTPUT_PINS;
        GPIO_Init(GPIOC, &GPIO_InitStructure);

        HW_PWR_Main_On();                                           // hold the main regulator power on
        HW_PWR_Disp_Off();    

        GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_10MHz;
        GPIO_InitStructure.GPIO_Mode    = PORTA_OUTPUT_MODE;
        GPIO_InitStructure.GPIO_Pin     = PORTA_OUTPUT_PINS;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
        GPIO_InitStructure.GPIO_Mode    = PORTA_INPUT_MODE_BTN;
        GPIO_InitStructure.GPIO_Pin     = PORTA_INPUT_PINS_BTN;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
        GPIO_InitStructure.GPIO_Mode    = PORTA_INPUT_MODE_EE;
        GPIO_InitStructure.GPIO_Pin     = PORTA_INPUT_PINS_EE;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
        GPIO_InitStructure.GPIO_Mode    = PORTA_INPUT_MODE_FLOAT;
        GPIO_InitStructure.GPIO_Pin     = PORTA_INPUT_PINS_FLOAT;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
        GPIO_InitStructure.GPIO_Mode    = PORTA_ANALOG_MODE;
        GPIO_InitStructure.GPIO_Pin     = PORTA_ANALOG_PINS;
        GPIO_Init(GPIOA, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Mode    = PORTB_OUTPUT_MODE;
        GPIO_InitStructure.GPIO_Pin     = PORTB_OUTPUT_PINS;
        GPIO_Init(GPIOB, &GPIO_InitStructure);
        GPIO_InitStructure.GPIO_Mode    = PORTB_OUTPUT_MODE_I2C;
        GPIO_InitStructure.GPIO_Pin     = PORTB_OUTPUT_PINS_I2C;
        GPIO_Init(GPIOB, &GPIO_InitStructure);
        GPIO_InitStructure.GPIO_Mode    = PORTB_INPUT_MODE;
        GPIO_InitStructure.GPIO_Pin     = PORTB_INPUT_PINS;
        GPIO_Init(GPIOB, &GPIO_InitStructure);
        GPIO_InitStructure.GPIO_Mode    = PORTB_INPUT_MODE_BTN;
        GPIO_InitStructure.GPIO_Pin     = PORTB_INPUT_PINS_BTN;
        GPIO_Init(GPIOB, &GPIO_InitStructure);

        // set up exti pins for portB, portA is by default:
        GPIO_EXTILineConfig( GPIO_PortSourceGPIOB, GPIO_PinSource3 );
        GPIO_EXTILineConfig( GPIO_PortSourceGPIOB, GPIO_PinSource4 );
        GPIO_EXTILineConfig( GPIO_PortSourceGPIOB, GPIO_PinSource5 );
        
        HW_LED_On();

        // RTC setup
        // -- use external 32*1024 crystal. Clock divider ck/32 -> 1024 ticks / second ( ~1ms )
        // - use alarm feature for wake up (EXTI17) and second interrupt generator
        // Timer counter capability: 48 days = 4,246,732,800 counter value

        // setup RTC interrupt reception
        NVIC_InitStructure.NVIC_IRQChannel = RTC_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);

        // program the RTC    ( 372 bytes of code)

        // activate the backup domain after reset
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);        // enable APB clocks for RTC and Backup Domanin
        PWR_BackupAccessCmd(ENABLE);
        // Reset Tamper Pin alarm to regain IO control and sustain power
        // From alarm pulse till here -> 5.5ms, minimum on time 20ms ( after turning power off -> regulator will be on ~15ms - decay on EN pin with 20nf capacitors )
        BKP_RTCOutputConfig( BKP_RTCOutputSource_None );

        if ( BKP_ReadBackupRegister(BKP_DR1) != 0xA957 )            // magic nr. for init testing purposes
        {
            // reset the RTC/BCP block
            BKP_DeInit();
            // start LSE oscillator and enable the clock
            RCC_LSEConfig(RCC_LSE_ON);
            while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)     //OPT: may be optimized
            {}
            RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
            RCC_RTCCLKCmd(ENABLE);
            // wait till last command is finished
            RTC_WaitForSynchro();
            RTC_WaitForLastTask();
            // Enable the RTC Alarm interrupt
            RTC_ITConfig(RTC_IT_ALR, ENABLE);
            // set prescalers and alarm
            RTC_WaitForLastTask();
            RTC_SetPrescaler(TIMER_RTC_PRESCALE);
            RTC_WaitForLastTask();
            RTC_SetAlarm( RTC_GetCounter() + 1 ); 
            RTC_WaitForLastTask();
            // set indicator in backup domain that RTC is configured
            BKP_WriteBackupRegister(BKP_DR1, 0xA957);

            wakeup_reason = WUR_FIRST;        // wake-up produced by RTC
        }
        else
        {
            uint32 rtc_ctr;
            RTC_WaitForSynchro();
            // Enable the RTC Alarm interrupt
            RTC_ITConfig(RTC_IT_ALR, ENABLE);
            // set prescalers and alarm
            RTC_WaitForLastTask();
            rtc_ctr = RTC_GetCounter();
            RTC_SetAlarm( rtc_ctr + 1 );     // obtain 1/2 second pulses in run-time
            RTC_WaitForLastTask();

            if ( BtnGet_Mode() )
                wakeup_reason = WUR_USR;        // wake-up produced by power button pressed 
            else
                wakeup_reason = WUR_RTC;        // wake-up produced by RTC

        }

        // setup period clock

        // ----- System Timer Init -----
        // Enable Timer1 clock and release reset
        // done abowe at RCC_APB2PeriphClockCmd / Reset

        // Set timer period 1ms
        TIM_tb.TIM_Prescaler           = 0;   //1;
        TIM_tb.TIM_CounterMode         = TIM_CounterMode_Up;
        TIM_tb.TIM_Period              = SYSTEM_MAX_TICK;
        TIM_tb.TIM_ClockDivision       = TIM_CKD_DIV1;
        TIM_tb.TIM_RepetitionCounter   = 0;
        TIM_TimeBaseInit(TIMER_SYSTEM, &TIM_tb);
        TIM_tb.TIM_Prescaler           = 0;   //10;
        TIM_tb.TIM_Period              = BUZZER_FREQ;
        TIM_TimeBaseInit(TIMER_BUZZER, &TIM_tb);

        // set up PWM for buzzer
        TIM_oc.TIM_OCMode       = TIM_OCMode_PWM1;
        TIM_oc.TIM_OutputState  = TIM_OutputState_Enable;
        TIM_oc.TIM_OCPolarity   = TIM_OCPolarity_Low;
        TIM_oc.TIM_Pulse        = BUZZER_FREQ/4;
        TIM_OC1Init(TIMER_BUZZER, &TIM_oc);
        TIM_ARRPreloadConfig(TIMER_BUZZER, ENABLE);
        TIM_CtrlPWMOutputs( TIMER_BUZZER, ENABLE ); 
        // set up AFIO for PWM OUTPUTS
        GPIO_InitStructure.GPIO_Pin     = IO_OUT_BUZZER;
        GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_PP;
        GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_2MHz;
        GPIO_Init(IO_PORT_BUZZER, &GPIO_InitStructure);


        // Clear update interrupt bit
        TIM_ClearITPendingBit( TIMER_SYSTEM, TIM_FLAG_Update );
        // Enable update interrupt
        TIM_ITConfig( TIMER_SYSTEM, TIM_FLAG_Update, ENABLE );

        NVIC_InitStructure.NVIC_IRQChannel              = TIMER_SYSTEM_IRQ;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority   = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd           = ENABLE;
        NVIC_Init( &NVIC_InitStructure );

        // stop timer counter when debugging, keep core alive when debugging in stop/sleep mode
        DBGMCU_Config( DBGMCU_I2C1_SMBUS_TIMEOUT | DBGMCU_TIM15_STOP | DBGMCU_SLEEP | DBGMCU_STOP , ENABLE );         // stop only the system timer

        // Enable timer counting
        TIM_Cmd( TIMER_SYSTEM, ENABLE);

        __enable_interrupt();

    }//END: InitHW


    void HW_SPI_interface_init( SPI_TypeDef* spi, uint16 baudrate )
    {
        SPI_InitTypeDef  SPI_InitStructure;
        GPIO_InitTypeDef GPIO_InitStructure;
        uint16 pin_mosi_ck;
        uint16 pin_miso;
        uint16 pin_cs;
        GPIO_TypeDef *port_sckmiso;
        GPIO_TypeDef *port_cs;

        // activate the SPI APB module
        if ( spi == SPI_PORT_EE )
        {
            RCC_APB2PeriphClockCmd( SPI_APB_EE, ENABLE );
            RCC_APB2PeriphResetCmd( SPI_APB_EE, ENABLE );
            RCC_APB2PeriphResetCmd( SPI_APB_EE, DISABLE );
            pin_mosi_ck = IO_OUT_EE_SCK | IO_OUT_EE_MOSI;
            pin_miso = IO_IN_EE_MISO;
            pin_cs = IO_OUT_EE_CS;
            port_sckmiso = IO_PORT_EE_SPI;
            port_cs = IO_PORT_EE_CS;
        }
        else
        {
            RCC_APB1PeriphClockCmd( SPI_APB_DISP, ENABLE );
            RCC_APB1PeriphResetCmd( SPI_APB_DISP, ENABLE );
            RCC_APB1PeriphResetCmd( SPI_APB_DISP, DISABLE );
            pin_mosi_ck = IO_OUT_DISP_SCK | IO_OUT_DISP_MOSI;
            pin_miso = 0;
            pin_cs = IO_OUT_DISP_CS;
            port_sckmiso = IO_PORT_DISP_SPI;
            port_cs = IO_PORT_DISP_CS;
        }

        // Configure Pins 
        // high speed alt1 push-pull for SPI clock and MOSI
        GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Pin     = pin_mosi_ck;
        GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_PP;
        GPIO_Init(port_sckmiso, &GPIO_InitStructure);
        // high speed input for SPI MISO
        if ( pin_miso )
        {
            GPIO_InitStructure.GPIO_Pin     = pin_miso;
            GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_IN_FLOATING;  
            GPIO_Init(port_sckmiso, &GPIO_InitStructure);
        }
        // set up chip select
        GPIO_InitStructure.GPIO_Pin     = pin_cs;
        GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_Out_PP;  
        GPIO_Init(port_cs, &GPIO_InitStructure);

        // SPI configuration
        SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
        SPI_InitStructure.SPI_Mode      = SPI_Mode_Master;
        SPI_InitStructure.SPI_DataSize  = SPI_DataSize_8b;
        SPI_InitStructure.SPI_CPOL      = SPI_CPOL_Low;
        SPI_InitStructure.SPI_CPHA      = SPI_CPHA_1Edge;
        SPI_InitStructure.SPI_NSS       = SPI_NSS_Soft;
        SPI_InitStructure.SPI_FirstBit  = SPI_FirstBit_MSB;
        SPI_InitStructure.SPI_CRCPolynomial = 7;
        SPI_InitStructure.SPI_BaudRatePrescaler = baudrate;
        SPI_Init(spi, &SPI_InitStructure);

        // enable the spi interface for eeprom
        SPI_Cmd(spi, ENABLE);
    }


    void HW_SPI_Set_Rx_mode_only( SPI_TypeDef* spi, bool on )
    {
        uint16_t tmpreg = 0;

        SPI_Cmd(spi, DISABLE);
        tmpreg = spi->CR1;
        if ( on )
            tmpreg |= SPI_Direction_2Lines_RxOnly;
        else
            tmpreg &= ~SPI_Direction_2Lines_RxOnly;
        spi->CR1 = tmpreg;
        SPI_Cmd(spi, ENABLE);
    }


    void HW_DMA_Init(uint32 dma_ch)
    {
        NVIC_InitTypeDef    NVIC_InitStructure;

        switch ( dma_ch & 0xff )
        {
            case DMACH_DISP:
                DMA_DISP_TX_Channel->CCR = ( DMA_DISP_TX_Channel->CCR & 0xFFFF800F ) |      // filter.value copied from stm32f10x_dma.c - not defined anywhere
                                           ( DMA_DIR_PeripheralDST     | DMA_Mode_Normal               | DMA_PeripheralInc_Disable |
                                             DMA_MemoryInc_Enable      | DMA_PeripheralDataSize_Byte   | DMA_MemoryDataSize_Byte   |
                                             DMA_Priority_High         | DMA_M2M_Disable );
                DMA_DISP_TX_Channel->CPAR     = (uint32_t)(&SPI_PORT_DISP->DR );            // base address for SPI data register
                // set up the interrupt request channel
                NVIC_InitStructure.NVIC_IRQChannel              = DMA_DISP_TX_IRQ;
                NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
                NVIC_InitStructure.NVIC_IRQChannelSubPriority   = 0;
                NVIC_InitStructure.NVIC_IRQChannelCmd           = ENABLE;
                NVIC_Init( &NVIC_InitStructure );
                break;
            case DMACH_EE:
                if ( dma_ch & 0x100 )
                {
                    DMA_EE_TX_Channel->CCR = ( DMA_EE_TX_Channel->CCR & 0xFFFF800F ) |          // filter.value copied from stm32f10x_dma.c - not defined anywhere
                                               ( DMA_DIR_PeripheralDST     | DMA_Mode_Normal               | DMA_PeripheralInc_Disable |
                                                 DMA_MemoryInc_Disable     | DMA_PeripheralDataSize_Byte   | DMA_MemoryDataSize_Byte   |
                                                 DMA_Priority_Medium       | DMA_M2M_Disable );
                }
                else
                {
                    DMA_EE_TX_Channel->CCR = ( DMA_EE_TX_Channel->CCR & 0xFFFF800F ) |          // filter.value copied from stm32f10x_dma.c - not defined anywhere
                                               ( DMA_DIR_PeripheralDST     | DMA_Mode_Normal               | DMA_PeripheralInc_Disable |
                                                 DMA_MemoryInc_Enable      | DMA_PeripheralDataSize_Byte   | DMA_MemoryDataSize_Byte   |
                                                 DMA_Priority_Medium       | DMA_M2M_Disable );
                }
                DMA_EE_TX_Channel->CPAR     = (uint32_t)(&SPI_PORT_EE->DR );                // base address for SPI data register
                DMA_EE_RX_Channel->CCR = ( DMA_EE_RX_Channel->CCR & 0xFFFF800F ) |          // filter.value copied from stm32f10x_dma.c - not defined anywhere
                                           ( DMA_DIR_PeripheralSRC     | DMA_Mode_Normal               | DMA_PeripheralInc_Disable |
                                             DMA_MemoryInc_Enable      | DMA_PeripheralDataSize_Byte   | DMA_MemoryDataSize_Byte   |
                                             DMA_Priority_High         | DMA_M2M_Disable );
                DMA_EE_RX_Channel->CPAR     = (uint32_t)(&SPI_PORT_EE->DR );                // base address for SPI data register
                break;
            case DMACH_SENS:
                DMA_SENS_TX_Channel->CCR = ( DMA_SENS_TX_Channel->CCR & 0xFFFF800F ) |          // filter.value copied from stm32f10x_dma.c - not defined anywhere
                                           ( DMA_DIR_PeripheralDST     | DMA_Mode_Normal               | DMA_PeripheralInc_Disable |
                                             DMA_MemoryInc_Enable      | DMA_PeripheralDataSize_Byte   | DMA_MemoryDataSize_Byte   |
                                             DMA_Priority_Medium         | DMA_M2M_Disable );
                DMA_SENS_TX_Channel->CPAR     = (uint32_t)(&I2C_PORT_SENSOR->DR );              // base address for SPI data register
                DMA_SENS_RX_Channel->CCR = ( DMA_SENS_RX_Channel->CCR & 0xFFFF800F ) |          // filter.value copied from stm32f10x_dma.c - not defined anywhere
                                           ( DMA_DIR_PeripheralSRC     | DMA_Mode_Normal               | DMA_PeripheralInc_Disable |
                                             DMA_MemoryInc_Enable      | DMA_PeripheralDataSize_Byte   | DMA_MemoryDataSize_Byte   |
                                             DMA_Priority_VeryHigh     | DMA_M2M_Disable );
                DMA_SENS_RX_Channel->CPAR     = (uint32_t)(&I2C_PORT_SENSOR->DR );              // base address for SPI data register
                break;
        }
    }


    void HW_DMA_Uninit(uint32 dma_ch)
    {
        // uninit interrupt request channel
        NVIC_InitTypeDef            NVIC_InitStructure;
        DMA_Channel_TypeDef *DMAtx = NULL;
        DMA_Channel_TypeDef *DMArx = NULL;
        uint32 clear_flags = 0;

        switch ( dma_ch )
        {
            case DMACH_DISP:
                DMAtx = DMA_DISP_TX_Channel;
                clear_flags = DMA_DISP_IRQ_FLAGS;
                NVIC_InitStructure.NVIC_IRQChannel              = DMA_DISP_TX_IRQ;
                NVIC_InitStructure.NVIC_IRQChannelCmd           = DISABLE;
                NVIC_Init( &NVIC_InitStructure );
                break;
            case DMACH_EE:
                DMAtx = DMA_EE_TX_Channel;
                DMArx = DMA_EE_RX_Channel;
                clear_flags = DMA_EE_RX_IRQ_FLAGS | DMA_EE_TX_IRQ_FLAGS;
                break;
            case DMACH_SENS:
                DMAtx = DMA_SENS_TX_Channel;
                DMArx = DMA_SENS_RX_Channel;
                clear_flags = DMA_SENS_RX_IRQ_FLAGS | DMA_SENS_TX_IRQ_FLAGS;
                break;
        }

        while ( DMAtx )
        {
            DMAtx->CCR &= (uint16_t)(~DMA_CCR1_EN);
            DMAtx->CCR  = 0;
            DMAtx->CNDTR = 0;
            DMAtx->CPAR  = 0;
            DMAtx->CMAR = 0;

            DMAtx = DMArx;      // switch to DMArx - if it is null it will exit anyway
            DMArx = NULL;       // prevent reload
        }
        DMA1->IFCR |= DMA_DISP_IRQ_FLAGS;
    }

    void HW_DMA_Send( uint32 dma_ch, const uint8 *ptr, uint32 size )
    {
        DMA_Channel_TypeDef *DMAch = NULL;
        switch ( dma_ch )
        {
            case DMACH_DISP:    DMAch = DMA_DISP_TX_Channel; break;
            case DMACH_EE:      DMAch = DMA_EE_TX_Channel;   break;
            case DMACH_SENS:    DMAch = DMA_SENS_TX_Channel; break;
            default:
                return;
        }

        DMAch->CCR     &= (uint16_t)(~DMA_CCR1_EN);     // to be able to reload the count register
        DMAch->CMAR     = (uint32_t)ptr;                // base address for data block
        DMAch->CNDTR    = size;                         // data block size
        DMAch->CCR     |= DMA_CCR1_EN;                  // enable dma channel - transmission begins

        // enable ISR for transfer complete interrupt for display
        if ( dma_ch == DMACH_DISP )
            DMAch->CCR     |= DMA_IT_TC;
    }

    void HW_DMA_Receive( uint32 dma_ch, const uint8 *ptr, uint32 size )
    {
        DMA_Channel_TypeDef *DMAch = NULL;
        switch ( dma_ch )
        {
            case DMACH_EE:      DMAch = DMA_EE_RX_Channel;   break;
            case DMACH_SENS:    DMAch = DMA_SENS_RX_Channel; break;
            default:
                return;
        }

        DMAch->CCR     &= (uint16_t)(~DMA_CCR1_EN);     // to be able to reload the count register
        DMAch->CMAR     = (uint32_t)ptr;                // base address for data block
        DMAch->CNDTR    = size;                         // data block size
        DMAch->CCR     |= DMA_CCR1_EN;                  // enable dma channel - transmission begins
    }


    #define CR1_CLEAR_Mask              ((uint32_t)0xFFF0FEDF)
    #define CR2_CLEAR_Mask              ((uint32_t)0xFFF1F7FD)
    #define SQR1_CLEAR_Mask             ((uint32_t)0xFF0FFFFF)

    #define CR2_ADON_Set                ((uint32_t)0x00000001)
    #define CR2_ADON_Reset              ((uint32_t)0xFFFFFFFE)
    #define CR2_CAL_Set                 ((uint32_t)0x00000004)
    #define CR2_RSTCAL_Set              ((uint32_t)0x00000008)
    #define CR2_EXTTRIG_SWSTART_Set     ((uint32_t)0x00500000)
    #define CR2_DMA_Set                 ((uint32_t)0x00000100)
    #define CR2_EXTTRIG_Set             ((uint32_t)0x00100000)

    static void HW_internal_ADC_recalibrate()
    {
        // recalibrate
        ADC1->CR2 |= CR2_RSTCAL_Set;
        while( ADC1->CR2 & CR2_RSTCAL_Set );
        ADC1->CR2 |= CR2_CAL_Set;
        while( ADC1->CR2 & CR2_CAL_Set );
    }

    uint32 HW_ADC_GetBattery(void)
    {   
        uint32 value;
        // - will activate the ADC, do a measurement and deactivate the ADC

        // Init the ADC for 1 channel sampling on software trigger
        // setup CR1
        value = ADC1->CR1;
        value &= CR1_CLEAR_Mask;            // See ADC_Init() - 1<<8 must be set for scan mode
        ADC1->CR1 = value;
        // setup CR2
        value = ADC1->CR2;
        value &= CR2_CLEAR_Mask;
        value |= ADC_ExternalTrigConv_None; // set for software trigger
        ADC1->CR2 = value;
        // setup sequence register for 1 conversion with channel 0
        ADC1->SQR1 = (0x00 << 20);      // L3:0 = 0000b => 1 conversion, rest of fields doesn't matter
        // setup channel parameters
        ADC1->SMPR2 = ADC_SampleTime_13Cycles5;     //  sample timer for channel 0  (shift << 3*chnl applies for other channels)
        // setup channel sequence - only channel 0 for a single conversion 
        ADC1->SQR3 = ( 0x00 << 0 );     // Vbat channel is Ch 0
        // Power up the ADC
        ADC1->CR2 |= CR2_ADON_Set;

        HW_internal_ADC_recalibrate();

        // start conversion
        ADC1->CR2 |= CR2_EXTTRIG_SWSTART_Set;
        while ( (ADC1->SR & ADC_SR_EOC) == 0 );
        value = ADC1->DR;

        // power down the ADC
        ADC1->CR2 &= CR2_ADON_Reset;

        return value;
    }

    static void internal_setup_stop_mode(bool stdby)
    {
        uint32_t tmpreg = 0;

        tmpreg = PWR->CR;
        tmpreg &= 0xFFFFFFFC;
        tmpreg |= PWR_Regulator_LowPower;
        if (stdby)
            tmpreg |= 0x02;         // standby

        PWR->CR = tmpreg;
        SCB->SCR |= SCB_SCR_SLEEPDEEP;
    }

    void HW_pwr_off_with_alarm( uint32 alarm, bool shutdown )
    {
        uint32 crt_rtc;
        // set up alarm (be sure that current RTC counter is smaller)
        RTC_WaitForLastTask();
        RTC_WaitForSynchro();
        crt_rtc =  RTC_GetCounter();
        if ( alarm <= crt_rtc )
            alarm = crt_rtc+1;
        RTC_SetAlarm( alarm ); 
        RTC_WaitForLastTask();      // wait RTC setup before going in low power
        // power down the system
        if ( shutdown )
        {
            internal_setup_stop_mode( false );      // do not use standby because it wakes up
            HW_PWR_Main_Off();
            HW_LED_Off();
            BKP_RTCOutputConfig( BKP_RTCOutputSource_Alarm );
            __asm("    wfi\n");     // do not spend power
            while (1);
        }
    }

    void HW_Seconds_Start(void)
    {
        RTC_WaitForSynchro();
        RTC_WaitForLastTask();
        RTC_SetAlarm( RTC_GetCounter() + 1024 ); 
        TIMER_RTC->CRL &= (uint16)~RTC_ALARM_FLAG;
    }

    void HW_Seconds_Restore(void)
    {
        // TBI

    }


    void HW_SetRTC_NextAlarm( uint32 alarm )
    {
        rtc_next_alarm = alarm;
    }



    void HW_Delay(uint32 us)
    {
        uint32 i;
        us = us * 2;
        for (i=0; i<us; i++)
        {
            __asm("    nop\n"); 
            __asm("    nop\n"); 
            __asm("    nop\n"); 
            __asm("    nop\n"); 
            __asm("    nop\n"); 
        }
    }

    void HW_Buzzer_On( int pulse )
    {
        GPIO_InitTypeDef            GPIO_InitStructure;

        GPIO_InitStructure.GPIO_Pin     = IO_OUT_BUZZER;
        GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_PP;
        GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_2MHz;
        GPIO_Init(IO_PORT_BUZZER, &GPIO_InitStructure);

        TIMER_BUZZER->ARR = (uint16)pulse;
        TIMER_BUZZER->CCR1 = (uint16)(pulse >> 1);  // 50% duty cycle
        TIM_Cmd( TIMER_BUZZER, ENABLE );
    }

    void HW_Buzzer_Off(void)
    {
        GPIO_InitTypeDef            GPIO_InitStructure;

        // disable PWM timer
        TIM_Cmd( TIMER_BUZZER, DISABLE);

        // disconnect pin - leave it HiZ
        GPIO_InitStructure.GPIO_Pin     = IO_OUT_BUZZER;
        GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_IN_FLOATING;
        GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_2MHz;
        GPIO_Init(IO_PORT_BUZZER, &GPIO_InitStructure);
    }


    // the multidirectional knob operates in a weird manner:
    // - downpress:     Center   - only
    // - up:            1 + 3 + Center      - center comes last
    // - down:          4 + 6 + Center
    // - left:          1 + 4 + Center
    // - right:         3 + 6 + Center
    //
    // The rule is - when we capture Center - need to look to the 1,3,4,6 if they are set
    //                                        if not - center is the signal -> generating OK button event
    //                                      - if 1/3/4/6 - do not send OK, send only the button for the right combination
    #define HWBTN_1     0x01
    #define HWBTN_3     0x02
    #define HWBTN_4     0x04
    #define HWBTN_6     0x08
    #define HWBTN_C     0x10

    bool BtnGet_OK()
    {
        // check for Center
        if ( BtnIs_Center() )       // get center as new press only if first it was released
        {
            if (btn_press == 0)
                btn_press |= HWBTN_C;
        }
        else
        {
            btn_press = 0;
            return false;
        }

        // collect the switch combination
        if ( BtnIs_1() )
            btn_press |= HWBTN_1;
        if ( BtnIs_3() )
            btn_press |= HWBTN_3;
        if ( BtnIs_4() )
            btn_press |= HWBTN_4;
        if ( BtnIs_6() )
            btn_press |= HWBTN_6;

        // decide if it is OK or not
        if ( btn_press == HWBTN_C )
            return true;
        return false;
    }

    bool BtnGet_Up()
    {
        if ( btn_press == (HWBTN_1 | HWBTN_3 | HWBTN_C) )
            return true;
        return false;
    }

    bool BtnGet_Down()
    {
        if ( btn_press == (HWBTN_4 | HWBTN_6 | HWBTN_C) )
            return true;
        return false;
    }

    bool BtnGet_Left()
    {
        if ( btn_press == (HWBTN_1 | HWBTN_4 | HWBTN_C) )
            return true;
        return false;
    }

    bool BtnGet_Right()
    {
        if ( btn_press == (HWBTN_3 | HWBTN_6 | HWBTN_C) )
            return true;
        return false;
    }




    static void internal_HW_set_EXTI( enum EPowerMode mode )
    {
        NVIC_InitTypeDef    NVIC_InitStructure;
        uint32              mask = 0;
        uint32              rising = 0;
        uint32              falling = 0;

        if ( mode )
        {
            switch ( mode )
            {
                case pm_hold_btn:
                    // all keys to be active when waking up UI: rising or falling for all
                    mask = IO_IN_BTN_ESC | IO_IN_BTN_OK | IO_IN_BTN_PP | IO_IN_BTN_1 | IO_IN_BTN_3 | IO_IN_BTN_4 | IO_IN_BTN_6 | IO_IN_SENS_IRQ |0x00020000;    // all the buttons + 
                    rising = mask;
                    falling = IO_IN_BTN_ESC | IO_IN_BTN_OK | IO_IN_BTN_PP | IO_IN_BTN_1 | IO_IN_BTN_3 | IO_IN_BTN_4 | IO_IN_BTN_6; 
                    break;
                case pm_hold:
                    // keys to be inactive when waking up UI: rising front for esc, start, menu, ok; falling for mode; any for p1 and p2
                    mask = IO_IN_BTN_PP | IO_IN_SENS_IRQ | 0x00020000;           // just power button / RTC alarm
                    rising = IO_IN_BTN_PP | IO_IN_SENS_IRQ | 0x00020000;
                    falling = 0x00;
                    break;
            }
        }

        EXTI->IMR = mask;           // interrupt mask register
        EXTI->EMR = 0;
        EXTI->RTSR = rising;
        EXTI->FTSR = falling;
        EXTI->PR = mask;            // clear the flags on the selected EXTI lines

        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority   = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd           = mask ? ENABLE : DISABLE;
        NVIC_InitStructure.NVIC_IRQChannel              = EXTI1_IRQn;       // ESC
        NVIC_Init( &NVIC_InitStructure );
        NVIC_InitStructure.NVIC_IRQChannel              = EXTI3_IRQn;       // BTN1
        NVIC_Init( &NVIC_InitStructure );
        NVIC_InitStructure.NVIC_IRQChannel              = EXTI4_IRQn;       // BTN4
        NVIC_Init( &NVIC_InitStructure );
        NVIC_InitStructure.NVIC_IRQChannel              = EXTI9_5_IRQn;     // BTN6 / SensorIRQ
        NVIC_Init( &NVIC_InitStructure );
        NVIC_InitStructure.NVIC_IRQChannel              = EXTI15_10_IRQn;   // BTN_PP / BTN3 / BTN_OK
        NVIC_Init( &NVIC_InitStructure );

        NVIC_InitStructure.NVIC_IRQChannel              = RTCAlarm_IRQn;    // RTC alarm
        NVIC_Init( &NVIC_InitStructure );
    }


    void HW_EXTI_ISR( void )
    {
        uint32 irq_source;

        HW_LED_On();
        // clean up the interrupt flags and notify that wake up reason was a button action
        irq_source = EXTI->PR;
        EXTI->PR = IO_IN_BTN_ESC | IO_IN_BTN_OK | IO_IN_BTN_PP | IO_IN_BTN_1 | IO_IN_BTN_3 | IO_IN_BTN_4 | IO_IN_BTN_6 | IO_IN_SENS_IRQ | 0x00020000;

        // check for RTC
        if ( irq_source & 0x00020000 )
        {
            irq_source &= ~0x00020000;      // remove the RTC flag
            wakeup_reason |= WUR_RTC;
            TimerRTCIntrHandler();
        }
        // check for sensor IRQ
        if ( irq_source & IO_IN_SENS_IRQ )
        {
            irq_source &= ~IO_IN_SENS_IRQ;      // remove the RTC flag
            wakeup_reason |= WUR_SENS_IRQ;
        }
        // check for any others
        if ( irq_source )
        {
            wakeup_reason |= WUR_USR;
        }
    }


    uint32 HW_Sleep( enum EPowerMode mode )
    {
        uint32 ret;
        wakeup_reason = WUR_NONE;
        switch ( mode )
        {
            case pm_full:                       // full cpu power - does nothing
                HW_LED_Off();
                HW_LED_On();
                break;
            case pm_sleep:                      // wait for interrupt sleep (stop cpu clock, but peripherals are running)
                HW_LED_Off();
                __asm("    wfi\n");            
                break;
            case pm_hold:                       // stop cpu and peripheral clocks - wakes up at RTC alarm, sensor IRQ or on EXTI event
            case pm_hold_btn:
                __disable_interrupt();
                // enter in low power mode
                internal_setup_stop_mode(false);
                // set up timer alarm
                HW_pwr_off_with_alarm( rtc_next_alarm, false );
                // clear pending RTC irq, since it is invalid now
                TIMER_RTC->CRL &= (uint16)~RTC_ALARM_FLAG;
                // set up EXTI interrupt handling for buttons and RTC
                internal_HW_set_EXTI( mode );
                __enable_interrupt();
                HW_LED_Off();
                HW_LED_On();
                HW_LED_Off();
                __asm("    wfi\n");       
                // exit from low power mode
                internal_HW_set_EXTI( pm_full );
                SCB->SCR &= (uint32_t)~((uint32_t)SCB_SCR_SLEEPDEEP);  
                break;
            case pm_down:
                __disable_interrupt();
                HW_LED_Off();
                HW_LED_On();
                HW_LED_Off();
                HW_LED_On();
                if ( rtc_next_alarm )
                    HW_pwr_off_with_alarm( rtc_next_alarm, true );
                HW_PWR_Main_Off();                   // powers off the system
                break;
        }

        return wakeup_reason;
    }


    uint32 HW_GetWakeUpReason(void)
    {
        return wakeup_reason;
    }


/*

    Power check using the 2016-03-27 code:

    Battery: 4.1V
    
    Gauge with display on           - avg: 11mA. max 12mA (pressure sensor read) (AC 4mA)
               display dim (hold)   - avg: 6.82mA ct, 8.7mA for 300ms/1sec -sensor read. (AC 3.6mA) 
               display off (hold)   - avg: 219uA ct - CPU in stop - waiting button/RTC event for wake-up

    Standby (powered down):         - 14.5uA
            wake up for battery / pressure read - see oscilloscope captures crt_sby_xxx.png
                                                - Shunt resistor 1K -> 1mV / uA


*/
