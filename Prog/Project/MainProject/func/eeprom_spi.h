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

    // enable eeprom in read only mode or in read/write mode. Can not be used when read or write operation is in progress
    uint32 eeprom_enable( bool write );

    // disable eeprom module - it interrupts any ongoing writing or reading operation
    uint32 eeprom_disable();

    // enter the device in deepsleep mode. - it interrupts any ongoing writing or reading operation
    uint32 eeprom_deepsleep();

    // read count quantity of data from address in buff, returns the nr. of successfull read bytes
    uint32 eeprom_read( uint32 address, int count, uint8 *buff, bool async );

    // write count quantity of data to address from buff, returns the nr. of successfull written bytes
    // function returns as soon as data is sent - check with eeprom_is_operation_finished() if write operation is finished
    uint32 eeprom_write( uint32 address, uint8 *buff, int count, bool async );

    // erase eeprom - first it must enabled in write mode
    uint32 eeprom_erase(void);

    // check if an async read or write is finished or write is completed (sync/async - doesn't matter)
    // this also polls write operation if data is spread over more pages
    bool eeprom_is_operation_finished( void );

#ifdef __cplusplus
    }
#endif


#endif // EEPROM_SPI_H
