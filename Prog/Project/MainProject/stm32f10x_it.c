/**
  ******************************************************************************
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and peripherals
  *          interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"

/** @addtogroup STM32F10x_StdPeriph_Examples
  * @{
  */

/** @addtogroup GPIO_JTAG_Remap
  * @{
  */ 

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

extern void TimerSysIntrHandler(void);
extern void TimerRTCIntrHandler(void);
extern void DispHAL_ISR_DMA_Complete(void);
extern void CoreADC_ISR_Complete(void);
extern void Core_ISR_PretriggerCompleted(void);
extern void HW_EXTI_ISR( void );


/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSV_Handler exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/
#if 0

#define IO_PORT_FW_TX           GPIOA
#define IO_PORT_FW_RX           GPIOA
#define IO_OUT_FWUP_TX          GPIO_Pin_9
#define IO_IN_FW_RX             GPIO_Pin_10


#define HW_DBG_Off()        do {  IO_PORT_FW_TX->BRR = IO_OUT_FWUP_TX;   } while (0)
#define HW_DBG_On()         do {  IO_PORT_FW_TX->BSRR = IO_OUT_FWUP_TX;  } while (0)
#define HW_DBG2_Off()        do {  IO_PORT_FW_RX->BRR = IO_IN_FW_RX;   } while (0)      // hack - firmware load RX - set to output
#define HW_DBG2_On()         do {  IO_PORT_FW_RX->BSRR = IO_IN_FW_RX;  } while (0)      // hack - firmware load RX - set to output


#define DBG_ENTER_0     do {    HW_DBG_On(); HW_DBG_Off(); } while(0)
#define DBG_EXIT_0      do {    HW_DBG2_On(); HW_DBG2_Off(); } while(0)
#define DBG_ENTER_1     do {    HW_DBG_On(); asm("nop"); HW_DBG_Off(); } while(0)
#define DBG_EXIT_1      do {    HW_DBG2_On(); asm("nop"); HW_DBG2_Off(); } while(0)
#define DBG_ENTER_2     do {    HW_DBG_On(); asm("nop"); asm("nop"); HW_DBG_Off(); } while(0)
#define DBG_EXIT_2      do {    HW_DBG2_On(); asm("nop"); asm("nop"); HW_DBG2_Off(); } while(0)
#define DBG_ENTER_3     do {    HW_DBG_On(); asm("nop"); asm("nop"); asm("nop"); HW_DBG_Off(); } while(0)
#define DBG_EXIT_3      do {    HW_DBG2_On(); asm("nop"); asm("nop"); asm("nop"); HW_DBG2_Off(); } while(0)
#define DBG_ENTER_4     do {    HW_DBG_On(); asm("nop"); asm("nop"); asm("nop"); asm("nop"); HW_DBG_Off(); } while(0)
#define DBG_EXIT_4      do {    HW_DBG2_On(); asm("nop"); asm("nop"); asm("nop"); asm("nop"); HW_DBG2_Off(); } while(0)
#define DBG_ENTER_5     do {    HW_DBG_On(); asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); HW_DBG_Off(); } while(0)
#define DBG_EXIT_5      do {    HW_DBG2_On(); asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); HW_DBG2_Off(); } while(0)

#else

#define DBG_ENTER_0     do { } while(0)
#define DBG_EXIT_0      do { } while(0)
#define DBG_ENTER_1     do { } while(0)
#define DBG_EXIT_1      do { } while(0)
#define DBG_ENTER_2     do { } while(0)
#define DBG_EXIT_2      do { } while(0)
#define DBG_ENTER_3     do { } while(0)
#define DBG_EXIT_3      do { } while(0)
#define DBG_ENTER_4     do { } while(0)
#define DBG_EXIT_4      do { } while(0)
#define DBG_ENTER_5     do { } while(0)
#define DBG_EXIT_5      do { } while(0)

#endif


#if 0

#define  DBG_PIN_SYSTIM_ENTER   do {  GPIOB->BSRR = GPIO_Pin_8; } while(0)//GPIOB->BSRR = GPIO_Pin_7; } while(0)      // dbg led (43)
#define  DBG_PIN_SYSTIM_EXIT    do {  GPIOB->BRR = GPIO_Pin_8; } while(0)//GPIOB->BRR = GPIO_Pin_7; } while(0)

#define  DBG_PIN_RTC_ENTER      do {  GPIOB->BSRR = GPIO_Pin_8; } while(0)//GPIOA->BSRR = GPIO_Pin_9; } while(0)      // Tx1 (30)
#define  DBG_PIN_RTC_EXIT       do {  GPIOB->BRR = GPIO_Pin_8; } while(0)//GPIOA->BRR = GPIO_Pin_9; } while(0)

#define  DBG_PIN_DMA_ENTER      do {  GPIOB->BSRR = GPIO_Pin_8; } while(0)//GPIOA->BSRR = GPIO_Pin_10; } while(0)     // Rx1 (31)
#define  DBG_PIN_DMA_EXIT       do {  GPIOB->BRR = GPIO_Pin_8; } while(0)//GPIOA->BRR = GPIO_Pin_10; } while(0)

#define  DBG_PIN_ADC_ENTER      do {  GPIOB->BSRR = GPIO_Pin_8; } while(0)//GPIOB->BSRR = GPIO_Pin_9; } while(0)      // Mic mute (46)
#define  DBG_PIN_ADC_EXIT       do {  GPIOB->BRR = GPIO_Pin_8; } while(0)//GPIOB->BRR = GPIO_Pin_9; } while(0)

#define  DBG_PIN_EXTI_ENTER     do {  GPIOB->BSRR = GPIO_Pin_8; } while(0)      // PB8 (45)
#define  DBG_PIN_EXTI_EXIT      do {  GPIOB->BRR = GPIO_Pin_8; } while(0)


#else 


#define  DBG_PIN_SYSTIM_ENTER   do { } while(0)     // dbg led (43)
#define  DBG_PIN_SYSTIM_EXIT    do { } while(0)

#define  DBG_PIN_RTC_ENTER      do { } while(0)     // Tx1 (30)
#define  DBG_PIN_RTC_EXIT       do { } while(0)

#define  DBG_PIN_DMA_ENTER      do { } while(0)     // Rx1 (31)
#define  DBG_PIN_DMA_EXIT       do { } while(0)

#define  DBG_PIN_ADC_ENTER      do { } while(0)     // Mic mute (46)
#define  DBG_PIN_ADC_EXIT       do { } while(0)

#define  DBG_PIN_EXTI_ENTER     do { } while(0)     // PB8 (45)
#define  DBG_PIN_EXTI_EXIT      do { } while(0)

#endif



/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

// Timer ISR routines
void TIM1_BRK_TIM15_IRQHandler(void)
{
    // System timer
    DBG_ENTER_2;
    DBG_PIN_SYSTIM_ENTER;
    TimerSysIntrHandler();
    DBG_PIN_SYSTIM_EXIT;
    DBG_EXIT_2;
}

void RTC_IRQHandler(void)
{
    DBG_ENTER_4;
    DBG_PIN_RTC_ENTER;
    TimerRTCIntrHandler();
    DBG_PIN_RTC_EXIT;
    DBG_EXIT_4;
}


// DMA isr routine for the display
void DMA1_Channel5_IRQHandler(void)
{
    DBG_ENTER_3;
    DBG_PIN_DMA_ENTER;
    DispHAL_ISR_DMA_Complete();
    DBG_PIN_DMA_EXIT;
    DBG_EXIT_3;
}


// We need for the switches the following EXTI handlers: 1,3,4,5,9,11,12,15,17
void EXTI1_IRQHandler(void)
{
    DBG_ENTER_0;
    DBG_PIN_EXTI_ENTER;
    HW_EXTI_ISR();
    DBG_PIN_EXTI_EXIT;
    DBG_EXIT_0;
}

void EXTI3_IRQHandler(void)
{
    DBG_ENTER_0;
    DBG_PIN_EXTI_ENTER;
    HW_EXTI_ISR();
    DBG_PIN_EXTI_EXIT;
    DBG_EXIT_0;
}

void EXTI4_IRQHandler(void)
{
    DBG_ENTER_0;
    DBG_PIN_EXTI_ENTER;
    HW_EXTI_ISR();
    DBG_PIN_EXTI_EXIT;
    DBG_EXIT_0;
}

void EXTI9_5_IRQHandler(void)
{
    DBG_ENTER_0;
    DBG_PIN_EXTI_ENTER;
    HW_EXTI_ISR();
    DBG_PIN_EXTI_EXIT;
    DBG_EXIT_0;
}

void EXTI15_10_IRQHandler(void)
{
    DBG_ENTER_0;
    DBG_PIN_EXTI_ENTER;
    HW_EXTI_ISR();
    DBG_PIN_EXTI_EXIT;
    DBG_EXIT_0;
}

void RTCAlarm_IRQHandler()
{
    DBG_ENTER_0;
    DBG_PIN_EXTI_ENTER;
    HW_EXTI_ISR();
    DBG_PIN_EXTI_EXIT;
    DBG_EXIT_0;
}

/**
  * @}
  */

/**
  * @}
  */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
