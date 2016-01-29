#include "hw_stuff.h"
#include "eeprom_spi.h"

/** Important NOTE:
 *  Code for 16MHz clock, using 25AA1024 with 256byte pages
 *  SPI on 8MHz
 **/

    #define CMD_READ        0x03
    #define CMD_WRITE       0x02
    #define CMD_WREN        0x06        // write enable
    #define CMD_WRDI        0x04        // write disable
    #define CMD_RDSR        0x05        // read status register
    #define CMD_CE          0xC7        // chip erase
    #define CMD_RDIP        0xAB        // release from dpd and read ID
    #define CMD_DPD         0xB9        // deep power down

    #define EE_SF_BUSY      0x01
    #define EE_SF_WREN      0x02

    #define EE_MAX_SIZE     (128*1024)

    #define EE_MAX_READ_WO_DMA  10      // nr. of bytes which worth be read without DMA
    #define EE_MAX_WRITE_WO_DMA  10     // nr. of bytes which worth be written without DMA

    enum EeepromStatus
    {
        eest_lowpower   = 0,
        eest_enable_rd,
        eest_enable_wr,
        eest_read_in_progr,
        eest_write_in_progr,
    } ee_status;

    uint32 wr_to_go = 0;
    uint32 wr_addr = 0;
    uint8 *wr_buff = 0;
    bool   wr_dma_used = false;


    static void internal_send_command( uint8 command )
    {
        // timings: with 8.3MHz clock
        // CS --|______|--  enable time: 2.3us
        // CS\_ -> first clock: 0.5us
        // 8xCLK: 1us
        // last CLK -> CS /-: 0.8us

        volatile uint32 dummy;
        dummy = SPI_I2S_ReceiveData( SPI_PORT_EE );     // clean RXNE flag
        HW_Chip_EEProm_Enable();
        SPI_PORT_EE->DR  = command;         // write enable command
        while( (SPI_PORT_EE->SR & SPI_I2S_FLAG_RXNE) == 0 );
        HW_Chip_EEProm_Disable();
    }


    static void internal_set_write_enable( bool status )
    {
        if ( status )
            internal_send_command( CMD_WREN );          // write enable command
        else
            internal_send_command( CMD_WRDI );          // write disable command
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
    static inline void internal_write_chunk( uint32 address, uint8 *buff, int len, bool async ) 
    {
        uint32 i;

        internal_set_write_enable(true);

        HW_Chip_EEProm_Enable();
        // send the command with the high address bit
        SPI_PORT_EE->DR  = CMD_WRITE;
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );
        // sent the 24bit address with MSB first
        SPI_PORT_EE->DR  = (address >> 16) & 0x01;
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );
        SPI_PORT_EE->DR  = (address >> 8) & 0xff;
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );
        SPI_PORT_EE->DR  = address & 0xff;
        wr_dma_used = false;
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );

        if ( len < EE_MAX_WRITE_WO_DMA )
        {
            // write the data directly if not worth configuring DMA
            for (i=0; i<len; i++)
            {
                SPI_PORT_EE->DR  = *buff;
                buff++;
                while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );
            }
            // wait till data is sent out
            while ( SPI_PORT_EE->SR & SPI_I2S_FLAG_BSY );
            HW_Chip_EEProm_Disable();                       // initiate write cycle by disabling CS
        }
        else
        {
            // write using DMA transfer
            HW_DMA_EE_Enable( DMAREQ_TX );              // enable TX requests for SPI port
            HW_DMA_Send( DMACH_EE, buff, len );         // set up buffer for data to be received

            if ( async == false )
            {
                // not async - wait here for data to be transmitted entirely
                while ( DMA_EE_RX_Channel->CNDTR );                     // wait for data amount to be sent
                while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );    // wait TX empty
                while ( SPI_PORT_EE->SR & SPI_I2S_FLAG_BSY );           // wait last bit to shift out
                HW_DMA_EE_Disable( DMAREQ_RX );                         // disable DMA request for TX
                HW_Chip_EEProm_Disable();                               // CS _/-
            }
            else
            {
                // async operation
                wr_dma_used = true;
            }
        }
    }

    static void internal_write_operation( bool async )
    {
        int len;

        len = 0x100 - (wr_addr & 0xff);     // space in the current page
        if ( len > wr_to_go )
            len = wr_to_go;                 // copy the remaining data only

        internal_write_chunk( wr_addr, wr_buff, len, async );
        wr_buff = ((uint8*)wr_buff + len);
        wr_addr += len;
        wr_to_go -= len;
    }


    uint32 eeprom_init()
    {
        volatile uint32 status;
        // Deselect the EEprom: Chip Select high
        HW_Chip_EEProm_Disable();
        HW_SPI_interface_init( SPI_PORT_EE, SPI_BaudRatePrescaler_2);   // set up SPI interface
        HW_DMA_Init( DMACH_EE );
        internal_set_write_enable( false );                             // clear the write enable
        internal_send_command( CMD_DPD );  
        ee_status = eest_lowpower;
        return 0;
    }

    uint32 eeprom_get_size()
    {
        return EE_MAX_SIZE;
    }

    uint32 eeprom_enable( bool write )
    {
        if ( ee_status >= eest_read_in_progr )
            return 1;

        if ( ee_status == eest_lowpower )
        {
            // take it out from deep sleep
            internal_send_command( CMD_RDIP );                      // wake up the device
            HW_Delay( 100 );
        }
        if ( write && (ee_status != eest_enable_wr) )
        {
            // set write enable
            uint32 result;
            internal_set_write_enable( true );
            result = internal_get_status();
            if ( (result & EE_SF_WREN) == 0 )
                return (uint32)-1;
            
            ee_status = eest_enable_wr;
        }
        else
        {
            ee_status = eest_enable_rd;
        }
        return 0;
    }

    uint32 eeprom_disable()
    {
        HW_Chip_EEProm_Disable();
        HW_SPI_Set_Rx_mode_only(SPI_PORT_EE, false);
        internal_set_write_enable( false );
        ee_status = eest_enable_rd;
        return 0;
    }

    uint32 eeprom_deepsleep()
    {
        eeprom_disable();
        internal_send_command( CMD_DPD );  
        ee_status = eest_lowpower;
        return 0;
    }

    uint32 eeprom_read( uint32 address, int count, uint8 *buff, bool async )
    {
        int i;
        volatile uint32 result;

        if ( (ee_status != eest_enable_rd) || (ee_status != eest_enable_wr) )
            return 0;

        if ( address >= EE_MAX_SIZE )
            return 0;
        if ( (address + count) > EE_MAX_SIZE )
            count = EE_MAX_SIZE - address;
        if ( count == 0 )
            return 0;

        HW_Chip_EEProm_Enable();
        // send the read command
        SPI_PORT_EE->DR  = CMD_READ;
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );
        // sent the 24bit address with MSB first
        SPI_PORT_EE->DR  = (address >> 16) & 0x01;
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );
        SPI_PORT_EE->DR  = (address >> 8) & 0xff;
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );
        SPI_PORT_EE->DR  = address & 0xff;
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );
        while ( SPI_PORT_EE->SR & SPI_I2S_FLAG_BSY );    // wait till address is sent completely
        result = SPI_I2S_ReceiveData( SPI_PORT_EE );     // need this otherwise it will not receive the first data byte

        // read the amount of data
        if ( count < EE_MAX_READ_WO_DMA )
        {
            // in case if it doesn't worth to receive through DMA - we ignore the async and proceed with reception here locally
            for (i=0; i<count; i++)
            {
                SPI_PORT_EE->DR  = 0;                                    // push dummy data to generate clock
                while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_RXNE) == 0 );    // and wait for data to be received
                *buff = SPI_PORT_EE->DR;
                buff++;
            }
            async = false;
            HW_Chip_EEProm_Disable();
        }
        else
        {
            // to much data to read - easier with DMA - set it up and start it up
            HW_DMA_EE_Enable( DMAREQ_RX );              // enable RX requests for SPI port
            HW_DMA_Receive( DMACH_EE, buff, count );    // set up buffer for data to be received
            HW_SPI_Set_Rx_mode_only(SPI_PORT_EE, true);              // start unidirectional RX (will generate clock for receiving data)

            if ( async == false )
            {
                // we need to wait here for data to be received entirely
                while ( DMA_EE_RX_Channel->CNDTR );     // wait for data amount to be received
                HW_Chip_EEProm_Disable();
                HW_SPI_Set_Rx_mode_only(SPI_PORT_EE, false);         // stop the unidirectional RX mode (stops the clock generation also)  
                HW_DMA_EE_Disable( DMAREQ_RX ); 
            }
            else
            {
                // async operation
                ee_status = eest_read_in_progr;
            }
        }
        return count;
    }


    uint32 eeprom_write( uint32 address, uint8 *buff, int count, bool async )
    {
        if ( (ee_status != eest_enable_wr) && (ee_status != eest_enable_rd) )
            return 0;
        if ( address >= EE_MAX_SIZE )
            return 0;
        if ( (address + count) > EE_MAX_SIZE )
            count = EE_MAX_SIZE - address;
        if ( count == 0 )
            return 0;

        wr_to_go = count;
        wr_buff = buff;
        wr_addr = address;
        do
        {
            internal_write_operation( async );
            if ( wr_to_go && (async == false) )     // if we have more chunks to be sent in non-async mode then need to wait write completion
            {
                uint32 eest;
                do
                {
                    eest = internal_get_status();
                } while ( eest & EE_SF_BUSY );
            }
            if ( async )                            // for async operation we don't care about to_go here - just exit
                break;

        } while ( wr_to_go != 0 );

        ee_status = eest_write_in_progr;
        return count;
    }


    uint32 eeprom_erase(void)
    {
        if ( ee_status != eest_enable_wr )
            return 1;

        internal_set_write_enable( true );
        internal_send_command( CMD_WRDI );
        wr_dma_used = false;
        wr_to_go = 0;
        ee_status = eest_write_in_progr;
    }


    bool eeprom_is_operation_finished( void )
    {
        if ( ee_status == eest_read_in_progr )          // if it is in read in progress state
        {
            if ( DMA_EE_RX_Channel->CNDTR == 0)         // check if DMA finished it's job
            {
                HW_Chip_EEProm_Disable();
                HW_SPI_Set_Rx_mode_only(SPI_PORT_EE, false);             // stop the unidirectional RX mode (stops the clock generation also)  
                HW_DMA_EE_Disable( DMAREQ_RX );             // disable DMA request for RX on SPI
                ee_status = eest_enable_rd;                 
            }
            else
                return false;
        }
        else if ( ee_status == eest_write_in_progr )    // write in progress
        {
            if ( (wr_dma_used == false) || (DMA_EE_RX_Channel->CNDTR == 0) )    // if DMA was not used or it finished it's job
            {
                uint32 eest = 0;
                if ( wr_dma_used )
                {
                    while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );    // wait TX empty
                    while ( SPI_PORT_EE->SR & SPI_I2S_FLAG_BSY );           // wait last bit to shift out
                    HW_DMA_EE_Disable( DMAREQ_RX );                         // disable DMA request for TX
                    HW_Chip_EEProm_Disable();                               // CS _/-  -> write operation started
                    wr_dma_used = false;                                    // set to false since we disabled the DMA
                    return false;
                }
                eest = internal_get_status();
                if ( (eest & EE_SF_BUSY) == 0 )               // check if the write operation is finished
                {
                    if ( wr_to_go )                         // if it was an async sending and there are still some data chunks to be sent
                    {
                        internal_write_operation( true );   // send them as async
                        return false;                       // signal busy
                    }
                    ee_status = eest_enable_rd;             // mark as finished
                    return true;                            // signal finished
                }
                return false;                               // signal busy
            }
            else
                return false;                               // signal busy since DMA didn't finished it's job
        }
        return true;                                    // by default signal finished (no write/read pending)
    }

