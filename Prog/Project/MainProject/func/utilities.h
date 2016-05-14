#ifndef UTILITIES_H
#define UTILITIES_H


#ifdef __cplusplus
    extern "C" {
#endif

#include "stm32f10x.h"
#include "typedefs.h"


// Start date is 2013-01-01 00:00:00.0
// increments are in 0.5 sec

#define WEEK_START          1               // the start date is a Tuesday (2nd day from a week)
#define DAY_TICKS       (2 * 3600 * 24)
#define WEEK_TICKS      (DAY_TICKS * 7)
#define YEAR_START      2013
#define YEAR_END        2081                // (68 years span on uint32)

// convert RTC counter to hour / minute / second
void utils_convert_counter_2_hms( uint32 counter, uint8 *phour, uint8 *pminute, uint8 *psecond);


void utils_convert_counter_2_ymd( uint32 counter, uint16 *pyear, uint8 *pmounth, uint8 *pday );

// convert date and time to counter. Time can be ommited (NULL) - it puts 00:00:00 in that case
uint32 utils_convert_date_2_counter( datestruct *date, timestruct *time );

uint32 utils_day_of_week( uint32 counter );

uint32 utils_week_of_year( uint32 counter );



uint32 utils_maximum_days_in_mounth( datestruct *pdate );


#ifdef __cplusplus
    }
#endif


#endif // UTILITIES_H
