#ifndef EEPROM_SPI_H
#define EEPROM_SPI_H

#ifdef __cplusplus
    extern "C" {
#endif

#include "stm32f10x.h"
#include "typedefs.h"
#include "hw_stuff.h"


    // init the eeprom module. eeprom is in read-ready standby state
    uint32 eeprom_init();

    // gets eeprom dimension
    uint32 eeprom_get_size();

    // enable eeprom in read only mode or in read/write mode. Can not be used when read or write operation is in progress
    // Can be asyncronous if eeprom was in deep sleep - check with eeprom_is_operation_finished()
    uint32 eeprom_enable( bool write );

    // disable eeprom module - it interrupts any ongoing writing or reading operation, eeprom enters in standby mode
    uint32 eeprom_disable();

    // enter the device in deepsleep mode. - it interrupts any ongoing writing or reading operation
    // an enable operation will be required
    uint32 eeprom_deepsleep();

    // read count quantity of data from address in buff, returns the nr. of successfull read bytes
    // eeprom must be in enabled state ( read or write )
    uint32 eeprom_read( uint32 address, uint32 count, uint8 *buff, bool async );

    // write count quantity of data to address from buff, returns the nr. of successfull written bytes
    // eeprom must be in write enabled state
    uint32 eeprom_write( uint32 address, const uint8 *buff, uint32 count, bool async );

    // erase eeprom - first it must enabled in write mode
    // erase operation is asyncronous - check with eeprom_is_operation_finished()
    uint32 eeprom_erase(void);

    // check if an async read or write is finished, or eeprom_enable() and eeprom_erase() finished the operation
    bool eeprom_is_operation_finished( void );

#ifdef __cplusplus
    }
#endif


#endif // EEPROM_SPI_H
