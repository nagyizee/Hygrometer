/**
  ******************************************************************************
  *
  *   Camera remote with timing, light and sound trigger
  *
  ******************************************************************************
  */ 
#include <stdio.h>
#include <stdlib.h>

#include "stm32f10x.h"
#include "typedefs.h"
#include "hw_stuff.h"

// IMPORTANT NOTE:   !!!
// !!! KEEP THIS IN SYNC WITH STACK SIZES defined in linker configuration
#ifdef VECT_TAB_SRAM
    #define STACK_SIZE      0x080
#else
    #define STACK_SIZE      0x300
#endif

extern void main_entry( uint32 *stack_top );
extern void main_loop(void);

volatile uint32 *stackguard;

static inline void StackGuardInit(void)
{
    volatile uint32 a;
    register uint32 i;

    a = STACK_CHECK_WORD;

    stackguard = &a;        // stack start pointer
    for ( i = 0; i <=((STACK_SIZE / 4) - 5); i++) 
    {
       stackguard--;
       *stackguard = STACK_CHECK_WORD;
    }
}

int main(void)
{
    // IMPORTANT NOTE - DO NOT CHANGE ANYTHING IN THIS ROUTINE OR IN THE StackGuardInit() ROUTINE
    // - it can mess up the stack start pointer for filling the check words
    StackGuardInit();

    main_entry( (uint32*)stackguard );
    main_loop();

    while (1);  // for safety

}//END: main





#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}

#endif

/**
  * @}
  */

/**
  * @}
  */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
