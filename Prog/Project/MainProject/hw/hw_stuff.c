

#include "hw_stuff.h"

    uint32 enc_dir = 0;
    uint32 enc_old = 0;
    static volatile bool btn_wakeup = false;

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

        // startup timeout
        {
            volatile uint32 start_dly = 150000;
            while (start_dly--);
        }

        // Vectors are set up at SystemInit, set up only priority group
        //TODO: see if we need this
        NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);

        // ----- Setup HW modules -----
        // Enable clock and release reset
        RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO  |
                               RCC_APB2Periph_TIM15 | RCC_APB2Periph_TIM16 | RCC_APB2Periph_ADC1 , ENABLE);
        RCC_APB2PeriphResetCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC |
                               RCC_APB2Periph_TIM15 | RCC_APB2Periph_TIM16 | RCC_APB2Periph_ADC1 , DISABLE);

        RCC_ADCCLKConfig(RCC_PCLK2_Div2);   // set up ADC clock

        // Make the remapping and Disable JTDI (PA15), JTDO (PB3), JTRST (PB4)
        GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable , ENABLE);

        // Init portA
        GPIO_InitStructure.GPIO_Mode    = PORTC_OUTPUT_MODE;        // begin with port C since this is the power holder
        GPIO_InitStructure.GPIO_Pin     = PORTC_OUTPUT_PINS;
        GPIO_Init(GPIOC, &GPIO_InitStructure);

        HW_PWR_Dig_On();        // hold the 2.5V regulator power on
        HW_PWR_An_Off();
        HW_Disp_VPanelOff();    

        GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_10MHz;
        GPIO_InitStructure.GPIO_Mode    = PORTA_OUTPUT_MODE;
        GPIO_InitStructure.GPIO_Pin     = PORTA_OUTPUT_PINS;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
        GPIO_InitStructure.GPIO_Mode    = PORTA_INPUT_MODE;
        GPIO_InitStructure.GPIO_Pin     = PORTA_INPUT_PINS;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
        GPIO_InitStructure.GPIO_Mode    = PORTA_INPUT_MODE_SB;
        GPIO_InitStructure.GPIO_Pin     = PORTA_INPUT_PINS_SB;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
        GPIO_InitStructure.GPIO_Mode    = PORTA_ANALOG_MODE;
        GPIO_InitStructure.GPIO_Pin     = PORTA_ANALOG_PINS;
        GPIO_Init(GPIOA, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Mode    = PORTB_OUTPUT_MODE;
        GPIO_InitStructure.GPIO_Pin     = PORTB_OUTPUT_PINS;
        GPIO_Init(GPIOB, &GPIO_InitStructure);
        GPIO_InitStructure.GPIO_Mode    = PORTB_INPUT_MODE;
        GPIO_InitStructure.GPIO_Pin     = PORTB_INPUT_PINS;
        GPIO_Init(GPIOB, &GPIO_InitStructure);
        GPIO_InitStructure.GPIO_Mode    = PORTB_INPUT_MODE_M;
        GPIO_InitStructure.GPIO_Pin     = PORTB_INPUT_PIN_M;
        GPIO_Init(GPIOB, &GPIO_InitStructure);

        // set up exti pins for portB, portA is by default:
        GPIO_EXTILineConfig( GPIO_PortSourceGPIOB, GPIO_PinSource3 );
        GPIO_EXTILineConfig( GPIO_PortSourceGPIOB, GPIO_PinSource4 );
        GPIO_EXTILineConfig( GPIO_PortSourceGPIOB, GPIO_PinSource5 );
        GPIO_EXTILineConfig( GPIO_PortSourceGPIOB, GPIO_PinSource6 );
        

        HW_LED_On();

        // wait for button release
        while( BtnGet_Power() )
        { }

        // RTC setup
        // -- use external 32*1024 crystal. Clock divider ck/32 -> 1024 ticks / second ( ~1ms )
        // - use alarm feature for wake up (EXTI17) and second interrupt generator
        // Timer counter capability: 48 days = 4,246,732,800 counter value

        // setup RTC interrupt reception
        NVIC_InitStructure.NVIC_IRQChannel = RTC_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);

        // program the RTC    ( 372 bytes of code)
        // TODO: if off-power clock will be used - check if backup registers are OK, only otherwise do the RTC init

        // activate the backup domain after reset
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);        // enable APB clocks for RTC and Backup Domanin
        PWR_BackupAccessCmd(ENABLE);

        if ( BKP_ReadBackupRegister(BKP_DR1) != 0xA957 )
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
            // enable interrupt for alarm
            RTC_ITConfig(RTC_IT_ALR, ENABLE);
            // set prescalers and alarm
            RTC_WaitForLastTask();
            RTC_SetPrescaler(31);
            RTC_WaitForLastTask();
            RTC_SetAlarm( RTC_GetCounter() + 1024 ); 
            RTC_WaitForLastTask();
            // set indicator in backup domain that RTC is configured
            BKP_WriteBackupRegister(BKP_DR1, 0xA957);
        }
        else
        {
            RTC_WaitForSynchro();
            // Enable the RTC Second
            RTC_ITConfig(RTC_IT_ALR, ENABLE);
            // set prescalers and alarm
            RTC_WaitForLastTask();
            RTC_SetPrescaler(31);
            RTC_WaitForLastTask();
            RTC_SetAlarm( RTC_GetCounter() + 1024 ); 
            RTC_WaitForLastTask();
        }

        // setup period clock

        // ----- System Timer Init -----
        // Enable Timer1 clock and release reset
        // done abowe at RCC_APB2PeriphClockCmd / Reset

        // Set timer period 62.5us
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
        TIM_oc.TIM_OCPolarity   = TIM_OCPolarity_High;
        TIM_oc.TIM_Pulse        = BUZZER_FREQ/2;
        TIM_OC1Init(TIMER_BUZZER, &TIM_oc);
        TIM_ARRPreloadConfig(TIMER_BUZZER, ENABLE);
        TIM_CtrlPWMOutputs( TIMER_BUZZER, ENABLE ); 
        // set up AFIO for PWM OUTPUTS
        GPIO_PinRemapConfig(GPIO_Remap_TIM15, ENABLE);
        GPIO_InitStructure.GPIO_Pin     = IO_OUT_BUZZER;
        GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_PP;
        GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_10MHz;
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

        // stop timer counter when debugging
        DBGMCU_Config( DBGMCU_TIM15_STOP, ENABLE );
        DBGMCU_Config( DBGMCU_TIM16_STOP, ENABLE );

        // Enable timer counting
        TIM_Cmd( TIMER_SYSTEM, ENABLE);

        HW_LED_Off();

        __enable_interrupt();

    }//END: InitHW


    void HW_SPI_interface_init( uint16 baudrate )
    {
        SPI_InitTypeDef  SPI_InitStructure;
        GPIO_InitTypeDef GPIO_InitStructure;

        // activate the SPI APB module
        RCC_APB2PeriphClockCmd( EE_APB, ENABLE );
        RCC_APB2PeriphResetCmd( EE_APB, ENABLE );
        RCC_APB2PeriphResetCmd( EE_APB, DISABLE );

        // Configure Pins 
        // high speed alt1 push-pull for SPI clock and MOSI
        GPIO_InitStructure.GPIO_Pin     = IO_OUT_EE_SCK | IO_OUT_EE_MOSI;
        GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_PP;
        GPIO_Init(IO_PORT_EE_SB, &GPIO_InitStructure);
        // high speed input for SPI MISO
        GPIO_InitStructure.GPIO_Pin     = IO_IN_SB_MISO;
        GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_IN_FLOATING;  
        GPIO_Init(IO_PORT_EEPROM_IN, &GPIO_InitStructure);
        // set up chip select
        GPIO_InitStructure.GPIO_Pin     = IO_OUT_EE_SEL;
        GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_Out_PP;  
        GPIO_Init(IO_PORT_EE_CTRL, &GPIO_InitStructure);

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
        SPI_Init(EE_SPI, &SPI_InitStructure);

        // enable the spi interface for eeprom
        SPI_Cmd(EE_SPI, ENABLE);

    }



    void HW_SPI_DMA_Init(void)
    {
        NVIC_InitTypeDef            NVIC_InitStructure;

        SPI_DMA_Channel->CCR = ( SPI_DMA_Channel->CCR & 0xFFFF800F ) |      // number copied from stm32f10x_dma.c
                                ( DMA_DIR_PeripheralDST     | DMA_Mode_Normal               | DMA_PeripheralInc_Disable |
                                  DMA_MemoryInc_Enable      | DMA_PeripheralDataSize_Byte   | DMA_MemoryDataSize_Byte   |
                                  DMA_Priority_High         | DMA_M2M_Disable );

        SPI_DMA_Channel->CPAR     = (uint32_t)(&EE_SPI->DR );             // base address for SPI data register

        // set up the interrupt request channel
        NVIC_InitStructure.NVIC_IRQChannel              = SPI_DMA_IRQ;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority   = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd           = ENABLE;
        NVIC_Init( &NVIC_InitStructure );
    }


    void HW_SPI_DMA_Uninit(void)
    {
        // uninit interrupt request channel
        NVIC_InitTypeDef            NVIC_InitStructure;

        NVIC_InitStructure.NVIC_IRQChannel              = SPI_DMA_IRQ;
        NVIC_InitStructure.NVIC_IRQChannelCmd           = DISABLE;
        NVIC_Init( &NVIC_InitStructure );

        SPI_DMA_Channel->CCR &= (uint16_t)(~DMA_CCR1_EN);
        SPI_DMA_Channel->CCR  = 0;
        SPI_DMA_Channel->CNDTR = 0;
        SPI_DMA_Channel->CPAR  = 0;
        SPI_DMA_Channel->CMAR = 0;
        DMA1->IFCR |= SPI_DMA_IRQ_FLAGS;

    }

    void HW_SPI_DMA_Send( const uint8 *ptr, uint32 size )
    {
        SPI_DMA_Channel->CCR     &= (uint16_t)(~DMA_CCR1_EN);   // to be able to reload the count register
        SPI_DMA_Channel->CMAR     = (uint32_t)ptr;          // base address for uart fifo
        SPI_DMA_Channel->CNDTR    = size;                   // maximum memory size
        SPI_DMA_Channel->CCR     |= DMA_CCR1_EN;            // enable dma channel - transmission begins

        // enable ISR for transfer complete interrupt
        SPI_DMA_Channel->CCR     |= DMA_IT_TC;
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
        ADC1->SMPR2 = ADC_SampleTime_7Cycles5;     //  sample timer for channel 0  (shift << 3*chnl applies for other channels)
        // setup channel sequence - only channel 0 for a single conversion 
        ADC1->SQR3 = ( 0x04 << 0 );     // Vbat channel is Ch 4
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


    void HW_ADC_StartAcquisition( uint32 buffer_addr, int group )
    {
        uint32 value;
        TIM_TimeBaseInitTypeDef     TIM_tb;
        
        // setup DMA 

        NVIC_InitTypeDef            NVIC_InitStructure;

        ADC_DMA_Channel->CCR = ( SPI_DMA_Channel->CCR & 0xFFFF800F ) |      // number copied from stm32f10x_dma.c
                                ( DMA_DIR_PeripheralSRC     | DMA_Mode_Circular                 | DMA_PeripheralInc_Disable |
                                  DMA_MemoryInc_Enable      | DMA_PeripheralDataSize_HalfWord   | DMA_MemoryDataSize_Word   |
                                  DMA_Priority_VeryHigh     | DMA_M2M_Disable );

        ADC_DMA_Channel->CPAR     = (uint32_t)(&ADC1->DR );     // peripheral address
        ADC_DMA_Channel->CMAR     = (uint32_t)buffer_addr;       // memory address
        ADC_DMA_Channel->CCR     &= (uint16_t)(~DMA_CCR1_EN);   // to be able to reload the count register
        ADC_DMA_Channel->CNDTR    = 4;                          // maximum memory size
        ADC_DMA_Channel->CCR     |= DMA_CCR1_EN;                // enable dma channel - transmission begins

        // set up ADC

        // clear interrupt flag
        ADC1->SR &= ~( ADC_IF_EOC );
        // setup CR1
        value = ADC1->CR1;
        value &= ( CR1_CLEAR_Mask );
        value |= (( 1 << 8 ) | ADC_IE_EOC); // enable Scan mode and interrupt
        ADC1->CR1 = value;

        // setup CR2
        value = ADC1->CR2;
        value &= CR2_CLEAR_Mask;
        value |= (ADC_ExternalTrigConv_T3_TRGO | CR2_DMA_Set | CR2_EXTTRIG_Set);  // Timer3 trigger output used - set up timer 3 with CR2->MMS = 010b (trigger out event on timer counter reload)
        ADC1->CR2 = value;
        // setup sequence register for 1 conversion with channel 0
        ADC1->SQR1 = (0x03 << 20);              // L3:0 = 0011b => 4 conversion, rest of fields doesn't matter
        // setup channel parameters
        if ( group == 0 )       // light sensor
        {
            ADC1->SMPR2 = ADC_SampleTime_7Cycles5 | (ADC_SampleTime_7Cycles5 << (3*1)) | (ADC_SampleTime_7Cycles5 << (3*2));     //  sample timer for channel 0  (shift << 3*chnl applies for other channels)
            ADC1->SQR3 = ( 4 << 0 ) | ( 2 << 5 ) | ( 3 << 10 ) | ( 16 << 15 );      // Vbat, LightL, LightH, Temp
        }
        else
        {
            ADC1->SMPR2 = ADC_SampleTime_7Cycles5 | (ADC_SampleTime_7Cycles5 << (3*3)) | (ADC_SampleTime_7Cycles5 << (3*4));     //  sample timer for channel 0  (shift << 3*chnl applies for other channels)
            ADC1->SQR3 = ( 4 << 0 ) | ( 1 << 5 ) | ( 0 << 10 ) | ( 16 << 15 );      // Vbat, SndL, SndH, Temp
        }

        // Power up the ADC and calibrate
        ADC1->CR2 |= CR2_ADON_Set;
        HW_internal_ADC_recalibrate();

        // set up the interrupt request for ADC dma channel
        NVIC_InitStructure.NVIC_IRQChannel              = ADC1_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 4;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority   = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd           = ENABLE;
        NVIC_Init( &NVIC_InitStructure );

        // Set up Timer3 for triggering the ADC at 100us obtaining 10kHz on each channel
        // enable timer clock
        RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM3, ENABLE);
        RCC_APB1PeriphResetCmd( RCC_APB1Periph_TIM3, DISABLE);
        // init timer
        TIM_tb.TIM_Prescaler           = 0;                 //1;
        TIM_tb.TIM_CounterMode         = TIM_CounterMode_Up;
        TIM_tb.TIM_Period              = ( 800 - 1 );       // 100us -> 10kHz sampling rate
        TIM_tb.TIM_ClockDivision       = TIM_CKD_DIV1;
        TIM_tb.TIM_RepetitionCounter   = 0;
        TIM_TimeBaseInit(TIMER_ADC, &TIM_tb);
        TIM_SelectOutputTrigger( TIMER_ADC, TIM_TRGOSource_Update );
        DBGMCU_Config( DBGMCU_TIM3_STOP, ENABLE );
        // Enable timer counting
        TIM_Cmd( TIMER_ADC, ENABLE);
    }


    void HW_ADC_StopAcquisition( void )
    {
        // stop the trigger timer
        TIM_Cmd( TIMER_ADC, DISABLE );
        RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM3, DISABLE );

        // power down the ADC
        ADC1->CR2 &= CR2_ADON_Reset;
        ADC1->SR &= ~( ADC_IF_EOC );

        // stop the DMA channel
        ADC_DMA_Channel->CCR     &= (uint16_t)(~DMA_CCR1_EN);
    }


    void HW_ADC_SetupPretrigger( uint32 value )
    {
        TIM_TimeBaseInitTypeDef     TIM_tb;
        NVIC_InitTypeDef            NVIC_InitStructure;

        if ( value == 1 )
            value++;        // timer doesn't like 1 values

        // pretrigger is using ADC sample timer
        RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM3, ENABLE );
        TIM_tb.TIM_Prescaler           = ( 800 - 1 );           // 100us timer pulse;
        TIM_tb.TIM_CounterMode         = TIM_CounterMode_Up;
        TIM_tb.TIM_Period              = ( value - 1 );         // 100us -> 10kHz sampling rate
        TIM_tb.TIM_ClockDivision       = TIM_CKD_DIV1;
        TIM_tb.TIM_RepetitionCounter   = 0;
        TIM_TimeBaseInit(TIMER_ADC, &TIM_tb);
        TIMER_ADC->CNT = 0;  

        // Clear update interrupt bit
        TIM_ClearITPendingBit( TIMER_ADC, TIM_FLAG_Update );
        // Enable update interrupt
        TIM_ITConfig( TIMER_ADC, TIM_FLAG_Update, ENABLE );

        NVIC_InitStructure.NVIC_IRQChannel              = TIMER_PRETRIGGER_IRQ;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority   = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd           = ENABLE;
        NVIC_Init( &NVIC_InitStructure );

        // set counter to 0
        TIM_Cmd( TIMER_ADC, ENABLE );
    }


    void HW_ADC_ResetPretrigger()
    {
        NVIC_InitTypeDef            NVIC_InitStructure;

        // stop the trigger timer
        TIM_Cmd( TIMER_ADC, DISABLE );

        // disable intrerupt
        TIM_ITConfig( TIMER_ADC, TIM_FLAG_Update, DISABLE );
        NVIC_InitStructure.NVIC_IRQChannel              = TIMER_PRETRIGGER_IRQ;
        NVIC_InitStructure.NVIC_IRQChannelCmd           = DISABLE;
        NVIC_Init( &NVIC_InitStructure );

        RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM3, DISABLE );
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

    void HW_Buzzer_On( int pulse )
    {
        GPIO_InitTypeDef            GPIO_InitStructure;

        GPIO_InitStructure.GPIO_Pin     = IO_OUT_BUZZER;
        GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_PP;
        GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_10MHz;
        GPIO_Init(IO_PORT_BUZZER, &GPIO_InitStructure);

        TIMER_BUZZER->ARR = (uint16)pulse;
        TIMER_BUZZER->CCR1 = (uint16)(pulse >> 1);  // 50% duty cycle
        TIM_Cmd( TIMER_BUZZER, ENABLE );
    }

    void HW_Buzzer_Off(void)
    {
        GPIO_InitTypeDef            GPIO_InitStructure;

        TIM_Cmd( TIMER_BUZZER, DISABLE);

        GPIO_InitStructure.GPIO_Pin     = IO_OUT_BUZZER;
        GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_IN_FLOATING;
        GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_10MHz;
        GPIO_Init(IO_PORT_BUZZER, &GPIO_InitStructure);
    }






    uint32 BtnGet_Encoder()
    {
        uint32 res = enc_dir;
        enc_dir = 0;
        return res;
    }

    void HW_Encoder_Poll(void)
    {
        uint32 enc_crt = 0;

        if ( IO_PORT_BTN_MDO->IDR & IO_IN_DIAL_P2 )
            enc_crt = 0x01;
        if ( IO_PORT_BTN_MDO->IDR & IO_IN_DIAL_P1 )
            enc_crt |= 0x02;

        if ( enc_crt == enc_old) 
             return;

        if ( (enc_crt ^ enc_old) == 0x03 ) // check if sensor state is updated
        {
            enc_old = enc_crt;
            return;                        
        }

        if ( ((enc_crt == 0x00) && (enc_old == 0x02)) ||
             ((enc_crt == 0x03) && (enc_old == 0x01))   )
        {
            enc_dir = 1;    // plus
        }

        else if ( ((enc_crt == 0x00) && (enc_old == 0x01)) ||
                  ((enc_crt == 0x03) && (enc_old == 0x02))  )
        {
            enc_dir = 2;    // minus
        }
        else if ( (enc_crt ^ enc_old) == 0x03 )
        {
            enc_dir = 3;    // woke up from stopped state by dial action
        }

        enc_old = enc_crt;

    }


    static inline void internal_HW_set_EXTI( uint32 mode )
    {
        NVIC_InitTypeDef    NVIC_InitStructure;
        uint32              mask = 0;
        uint32              rising = 0;
        uint32              falling = 0;

        if ( mode )
        {
            // enable for all
            mask = IO_IN_BTN_ESC | IO_IN_BTN_START | IO_IN_BTN_MENU | IO_IN_BTN_OK | IO_IN_DIAL_P1 | IO_IN_DIAL_P2 | IO_IN_BTN_MODE;
            switch ( mode )
            {
                case HWSLEEP_STOP_W_ALLKEYS:
                    // all keys to be active when waking up UI: rising or falling for all
                    rising = mask;
                    falling = mask;
                    break;
/*rewr                case HWSLEEP_STOP_W_PKEY:
                    // 'Start' and 'OK' to be active when waking up UI: use both fronts for START and OK
                    rising = IO_IN_BTN_ESC | IO_IN_BTN_START | IO_IN_BTN_MENU | IO_IN_BTN_OK | IO_IN_DIAL_P1 | IO_IN_DIAL_P2;
                    falling = IO_IN_BTN_MODE | IO_IN_DIAL_P1 | IO_IN_DIAL_P2  | IO_IN_BTN_OK | IO_IN_BTN_START;
                    break;
                    */
                case HWSLEEP_STOP:
                    // keys to be inactive when waking up UI: rising front for esc, start, menu, ok; falling for mode; any for p1 and p2
                    rising = IO_IN_BTN_ESC | IO_IN_BTN_START | IO_IN_BTN_MENU | IO_IN_BTN_OK | IO_IN_DIAL_P1 | IO_IN_DIAL_P2;
                    falling = IO_IN_BTN_MODE | IO_IN_DIAL_P1 | IO_IN_DIAL_P2;
                    break;
            }
        }

        EXTI->IMR = mask;
        EXTI->EMR = 0;
        EXTI->RTSR = rising;
        EXTI->FTSR = falling;
        EXTI->PR = mask;

        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 9;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority   = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd           = mode ? ENABLE : DISABLE;
        NVIC_InitStructure.NVIC_IRQChannel              = EXTI3_IRQn;       // OK button
        NVIC_Init( &NVIC_InitStructure );
        NVIC_InitStructure.NVIC_IRQChannel              = EXTI4_IRQn;       // Encoder P1
        NVIC_Init( &NVIC_InitStructure );
        NVIC_InitStructure.NVIC_IRQChannel              = EXTI9_5_IRQn;     // Encoder P2 and Power button
        NVIC_Init( &NVIC_InitStructure );
        NVIC_InitStructure.NVIC_IRQChannel              = EXTI15_10_IRQn;   // ESC, START, MENU buttons
        NVIC_Init( &NVIC_InitStructure );
    }


    void HW_EXTI_ISR( void )
    {
        // clean up the interrupt flags and notify that wake up reason was a button action
        EXTI->PR = IO_IN_BTN_ESC | IO_IN_BTN_START | IO_IN_BTN_MENU | IO_IN_BTN_OK | IO_IN_DIAL_P1 | IO_IN_DIAL_P2 | IO_IN_BTN_MODE;
        btn_wakeup = true;
    }


    bool HW_Sleep( enum EPowerMode mode )
    {
        bool ret;

        switch ( mode )
        {
            case HWSLEEP_FULL:                      // does nothing - full cpu power
                break;
            case HWSLEEP_WAIT:                      // wait for interrupt sleep (stop cpu clock, peripehrals are running)
                HW_LED_Off();
                __asm("    wfi\n");                 // 3.6mA in standby with everything fully functional, 6.12mA in full time cpu clock mode
                HW_LED_On();
                break;
            case HWSLEEP_STOP:                      // stop cpu and peripheral clocks - wakes up at 1sec intervals or on EXTI event
            case HWSLEEP_STOP_W_ALLKEYS:
/*rewr            case HWSLEEP_STOP_W_PKEY:
                // for the moment STOP mode isn't working - we will simulate it by disabling the system timer
                TIM_Cmd( TIMER_SYSTEM, DISABLE);
                // set up EXTI interrupt handling for buttons
                internal_HW_set_EXTI( mode );
                // enter in low power mode
                HW_LED_Off();
                __asm("    wfi\n");       
                HW_LED_On();
                // exit from low power mode
                internal_HW_set_EXTI( 0 );
                // start the system timer
                TIM_Cmd( TIMER_SYSTEM, ENABLE);
                ret = true;
                break;
              */
            case HWSLEEP_OFF:
                HW_PWR_Dig_Off();                   // powers off the system
                break;
        }

        ret = btn_wakeup;
        btn_wakeup = false;
        return ret;
    }
