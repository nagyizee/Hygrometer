#ifndef EEPROM_SPI_H
#define EEPROM_SPI_H

#ifdef __cplusplus
    extern "C" {
#endif

#include "stm32f10x.h"
#include "typedefs.h"
#include "hw_stuff.h"


    // init the eeprom module
    uint32 eeprom_init();

    // gets eeprom dimension
    uint32 eeprom_get_size();

    // enable eeprom in read only mode or in read/write mode
    uint32 eeprom_enable( bool write );

    // disable eeprom module
    uint32 eeprom_disable();

    // read count quantity of data from address in buff, returns the nr. of successfull read bytes
    uint32 eeprom_read( uint32 address, int count, uint8 *buff );

    // write count quantity of data to address from buff, returns the nr. of successfull written bytes
    uint32 eeprom_write( uint32 address, uint8 *buff, int count );

#ifdef __cplusplus
    }
#endif


#endif // EEPROM_SPI_H
