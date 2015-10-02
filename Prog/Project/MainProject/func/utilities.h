#ifndef UTILITIES_H
#define UTILITIES_H


#ifdef __cplusplus
    extern "C" {
#endif

#include "stm32f10x.h"
#include "typedefs.h"


// convert RTC counter to hour / minute / second
void utils_convert_counter_2_hms( uint32 counter, uint8 *phour, uint8 *pminute, uint8 *psecond);


void utils_convert_counter_2_ymd( uint32 counter, uint16 *pyear, uint8 *pmounth, uint8 *pday );

// convert date and time to counter. Time can be ommited (NULL) - it puts 00:00:00 in that case
uint32 utils_convert_date_2_counter( datestruct *date, timestruct *time );


#ifdef __cplusplus
    }
#endif


#endif // UTILITIES_H
