#include <stdio.h>
#include "stdlib_extension.h"

/*
static int atoi( char *string )
{
    int negative = 0;
    int value = 0;
    char *start;
    
    if (string == NULL) // if data passed in is null
        return 0;
    
    start = string;
    
    while(*start == ' ') // ignore pre-whitespaces
        start++;
    
    if (*start == '-')
    { 
        negative = 1;
        start++;
    }
    
    while (*start != '\0')
    {
        if (*start >= '0' && *start <= '9')
        { 
            // numeric characters 
            if (negative == 0 && value * 10 + ((int)*start - '0') < value)
            { 
                // check for + buffer overflow
                return -1;
            }
            if (negative == 1 && (((-value) * 10) - ((int)*start - '0') > -value))
            { 
                // check for - buffer overflow
                return -1;
            }
            value = value * 10 + ((int)*start - '0');
        }
        start++;
    }
    
    if (negative)
        value = -value;
    
    return value;
}
*/

void itoa( int input, char *string, int radix )
{
    int digits = 0;
    int value = input;
    char *str_val = string;
    char *end;

    if ( radix == 0 )
    {
        *str_val = 0x00;
        return;
    }

    if (value == 0)
    { 
        *str_val = '0';
        *(++str_val) = '\0';
        return;
    }
    if (value < 0) 
    { 
        /* negative value requires an extra digit */
        digits++; 
    }

    while (value != 0)
    {
        value /= radix;
        digits++;
    }

    value = input; /* reset value */
    end = str_val + digits;

    if (value < 0) 
    {
        /* allocate negative character */
        *str_val = '-';
        value = value * -1;
    }

    *end = '\0';
    end--;
    while (value != 0)
    {
        *end = '0'+(value % radix);
        if ( (*end -'0') > 9 )
        {
            *end = *end - '0' - 10 + 'A';
        }
        value /= radix;    
        end--;
    }
 }

