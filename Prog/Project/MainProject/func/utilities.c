#include "utilities.h"

const int mounthLUTny[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };   // days in non leap year
const int mounthLUTly[] = { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };   // days in leap year
const int mounthDays[] =  { 31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };       // days in a mounth


void utils_convert_counter_2_hms( uint32 counter, uint8 *phour, uint8 *pminute, uint8 *psecond)
{
    counter = (counter>>1) % (3600*24);    // get the hour of day
    if ( phour )
        *phour = counter / 3600;
    counter = counter % 3600;
    if ( pminute )
        *pminute = counter / 60;
    if ( psecond )
        *psecond = counter % 60;
}


void utils_convert_counter_2_ymd( uint32 counter, uint16 *pyear, uint8 *pmounth, uint8 *pday )
{
    uint32 year;
    uint32 mounth;
    uint32 day;
    const int *mounthLUT;
    int i;
    counter = (counter>>1) / (3600*24);     // get the day nr from YEAR_START

    year = (counter * 100 + 99) / 36525;   // calculate the year
    counter -= ((year * 36525) / 100);      // leave only the days from year

    // find the mouth
    if ( (year+1) % 4 )
        mounthLUT = mounthLUTny;
    else
        mounthLUT = mounthLUTly;

    for (i=11; i>=0; i--)
    {
        if ( mounthLUT[i] <= counter )
            break;
    }
    mounth = i;

    // find the day
    day = counter - mounthLUT[i];

    if ( pday )
        *pday = day+1;
    if ( pmounth )
        *pmounth = mounth+1;
    if ( pyear )
        *pyear = year+YEAR_START;
}

uint32 utils_convert_date_2_counter( datestruct *pdate, timestruct *ptime )
{
    uint32 counter = 0;
    uint32 myear;
    uint32 days;

    if ( pdate->year < YEAR_START)
        return 0;
    if ( pdate->mounth > 12 )
        return 0;

    myear = pdate->year - YEAR_START;        // starting with leap year

    // add the time
    if ( ptime )
        counter = (ptime->hour*3600 + ptime->minute*60 + ptime->second);

    // add the days
    days = (pdate->day-1);

    // add the mounths
    if ( (myear+1) % 4 )
        days = days + mounthLUTny[pdate->mounth-1];         // add the days of the elapsed mounths considering leap year
    else
        days = days + mounthLUTly[pdate->mounth-1];         // add the days of the elapsed mounths considering non-leap year

    // add the years
    days = days + ((myear * 36525) / 100);                 // the rounding takes care of leap year +1 day addition

    counter = counter + days * 24 * 3600;

    return (counter<<1);
}


uint32 utils_maximum_days_in_mounth( datestruct *pdate )
{
    uint32 days;

    days = mounthDays[pdate->mounth-1];
    if ( days == 0 )
    {
        uint32 myear;
        myear = pdate->year - YEAR_START;   // starting with leap year
        if ( (myear+1) % 4 )
            return 28;
        else
            return 29;
    }
    return days;
}











