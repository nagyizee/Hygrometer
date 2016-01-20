#include "hw_stuff.h"
#include "eeprom_spi.h"

/** Important NOTE:
 *  Code is valid for 24MHz system frequency
 *  Using SPI on 3MHz
 **/

    #define EE_DISABLED     0
    #define EE_ENABLED      1
    #define EE_ENABLED_WR   2

    #define EE_SF_BUSY      0x01
    #define EE_SF_WREN      0x02

    #define EE_MAX_SIZE     512

    uint32 ee_status;



    static void internal_set_write_enable( bool status )
    {
        volatile uint32 dummy;
        dummy = SPI_I2S_ReceiveData( SPI_PORT_EE );   // clean RXNE flag
        HW_Chip_EEProm_Enable();
        if ( status )
            SPI_PORT_EE->DR  = 0x06;         // write enable command
        else
            SPI_PORT_EE->DR  = 0x04;         // write disable command

        while( (SPI_PORT_EE->SR & SPI_I2S_FLAG_RXNE) == 0 );
        HW_Chip_EEProm_Disable();
    }

    static uint32 internal_get_status(void)
    {
        volatile uint32 result;
        HW_Chip_EEProm_Enable();
        // write read status command
        SPI_PORT_EE->DR  = 0x05;
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );
        while ( SPI_PORT_EE->SR & SPI_I2S_FLAG_BSY );    // need this - timing for receiving result
        // read the response
        result = SPI_I2S_ReceiveData( SPI_PORT_EE );     // need this - timing for receiving result 
        SPI_PORT_EE->DR  = 0x00;
        while ( SPI_PORT_EE->SR & SPI_I2S_FLAG_BSY );
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_RXNE) == 0 );
        result = SPI_I2S_ReceiveData( SPI_PORT_EE );
        HW_Chip_EEProm_Disable();
        return result;
    }

    // using page write - assure that data fits in the page
    static uint32 internal_write_chunk( uint32 address, uint8 *buff, int len ) 
    {
        uint32 i;

        internal_set_write_enable(true);

        HW_Chip_EEProm_Enable();
        // send the command with the high address bit
        SPI_PORT_EE->DR  = 0x02 | ((address & 0x100) ? 0x08 : 0x00);
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );
        // sent the remaining address byte
        SPI_PORT_EE->DR  = address & 0xff;
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );

        // write the data
        for (i=0; i<len; i++)
        {
            SPI_PORT_EE->DR  = *buff;
            buff++;
            while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );
        }

        // wait till data is sent out
        while ( SPI_PORT_EE->SR & SPI_I2S_FLAG_BSY );
        HW_Chip_EEProm_Disable();

        do
        {
            i = internal_get_status();
        } while ( i & EE_SF_BUSY );

        return 0;
    }


    uint32 eeprom_init()
    {
        // Deselect the FLASH: Chip Select high
        HW_Chip_EEProm_Disable();

        HW_SPI_interface_init(SPI_BaudRatePrescaler_2);     // 4MHz 

        ee_status = EE_DISABLED;

        return 0;
    }

    uint32 eeprom_get_size()
    {
        return EE_MAX_SIZE;
    }

    uint32 eeprom_enable( bool write )
    {
        if ( write )
        {
            uint32 result;
            // test the eeprom
            internal_set_write_enable( true );
            result = internal_get_status();
            if ( (result & EE_SF_WREN) == 0 )
                return (uint32)-1;
            
            ee_status = EE_ENABLED_WR;
        }
        else
        {
            ee_status = EE_ENABLED;
        }
        return 0;
    }

    uint32 eeprom_disable()
    {
        HW_Chip_EEProm_Disable();
        ee_status = EE_DISABLED;
        return 0;
    }


    uint32 eeprom_read( uint32 address, int count, uint8 *buff )
    {
        int i;
        volatile uint32 result;

        if ( ee_status == EE_DISABLED )
            return 0;

        i = 0;
        if ( (address + count) > EE_MAX_SIZE )
            count = EE_MAX_SIZE - count;
        if ( count <= 0 )
            return 0;

        HW_Chip_EEProm_Enable();
        // send the command with the high address bit
        SPI_PORT_EE->DR  = 0x03 | ((address & 0x100) ? 0x08 : 0x00);
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );
        // sent the remaining address byte
        SPI_PORT_EE->DR  = address & 0xff;
        while ( SPI_PORT_EE->SR & SPI_I2S_FLAG_BSY );    // wait till address is sent completely
        result = SPI_I2S_ReceiveData( SPI_PORT_EE );     // need this otherwise it will not receive the first data byte
        // read the amount of data
        for (i=0; i<count; i++)
        {
            SPI_PORT_EE->DR  = 0;                                    // push dummy data to generate clock
            while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_RXNE) == 0 );    // and wait for data to be received
            *buff = SPI_PORT_EE->DR;
            buff++;
        }

        HW_Chip_EEProm_Disable();
        return count;
    }


    uint32 eeprom_write( uint32 address, uint8 *buff, int count )
    {
        int len;
        int to_go;
    
        if ( ee_status != EE_ENABLED_WR )
            return 0;
    
        if ( (address + count) > EE_MAX_SIZE )
            count = EE_MAX_SIZE - count;
        if (count <= 0)
            return 0;
    
        to_go = count;
        do
        {
            len = 0x10 - (address & 0x0f);    // space in the current page
            if ( len > to_go )
                len = to_go;                // copy the remaining data only
    
            if ( internal_write_chunk( address, buff, len ) )
            {
                return (count - to_go);
            }
    
            buff += len;
            address += len;
            to_go -= len;
        } while ( to_go != 0 );
    
        return count;
    }



