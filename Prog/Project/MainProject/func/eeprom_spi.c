#include "hw_stuff.h"
#include "eeprom_spi.h"

/** Important NOTE:
 *  Code for 16MHz clock, using FM25V20A with 2Mbit memory
 *  SPI on 8MHz
 **/

    #define CMD_READ        0x03
    #define CMD_WRITE       0x02
    #define CMD_WREN        0x06        // write enable
    #define CMD_WRDI        0x04        // write disable
    #define CMD_RDSR        0x05        // read status register
    #define CMD_DPD         0xB9        // deep power down
    #define CMD_ID          0x9F        // read device ID

    #define EE_SF_WREN      0x02

    #define EE_MAX_SIZE     (256*1024)

    #define EE_MAX_READ_WO_DMA  4      // nr. of bytes which worth be read without DMA
    #define EE_MAX_WRITE_WO_DMA  8     // nr. of bytes which worth be written without DMA

    enum EeepromStatus
    {
        eest_lowpower   = 0,
        eest_enable_rd,
        eest_enable_wr,
        eest_read_in_progr,
        eest_write_in_progr,
        eest_pwrup_in_progr,
        eest_pwrup_in_progr_w
    } ee_status;

    uint32 ee_erase = 0;


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
        SPI_PORT_EE->DR  = CMD_RDSR;
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


    static uint32 internal_check_ID(void)
    {
        volatile uint32 result;
        HW_Chip_EEProm_Enable();
        // write read status command
        SPI_PORT_EE->DR  = CMD_ID;
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );
        while ( SPI_PORT_EE->SR & SPI_I2S_FLAG_BSY );    // need this - timing for receiving result
        // read the response
        result = SPI_I2S_ReceiveData( SPI_PORT_EE );     // need this - timing for receiving result 
        SPI_PORT_EE->DR  = 0x00;
        while ( SPI_PORT_EE->SR & SPI_I2S_FLAG_BSY );
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_RXNE) == 0 );
        result = SPI_I2S_ReceiveData( SPI_PORT_EE );
        HW_Chip_EEProm_Disable();

        if ( result == 0x7F )
            return 1;
        return 0;
    }


    uint32 eeprom_init()
    {
        volatile uint32 status;
        // Deselect the EEprom: Chip Select high
        HW_Chip_EEProm_Disable();
        HW_SPI_interface_init( SPI_PORT_EE,/*SPI_BaudRatePrescaler_2*/ SPI_BaudRatePrescaler_4);   // set up SPI interface
        HW_DMA_Init( DMACH_EE );

        internal_set_write_enable( false );                             // clear the write enable
        ee_status = eest_enable_rd;
        ee_erase = 0;
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
            HW_Chip_EEProm_Enable();                                // device is woken up by CS -\_
            if ( write )
                ee_status = eest_pwrup_in_progr_w;
            else
                ee_status = eest_pwrup_in_progr;
        }
        else
        if ( write )
        {
            // set write enable
            if ( ee_status != eest_enable_wr )
            {
                uint32 result;
                internal_set_write_enable( true );
                result = internal_get_status();
                if ( (result & EE_SF_WREN) == 0 )
                    return (uint32)-1;
            }
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

        HW_DMA_Uninit( DMACH_EE );
        HW_DMA_Init( DMACH_EE );

        ee_status = eest_enable_rd;
        ee_erase = 0;
        return 0;
    }

    uint32 eeprom_deepsleep()
    {
        if (ee_status == eest_lowpower)
            return 0;
        eeprom_disable();
        internal_send_command( CMD_DPD );  
        ee_status = eest_lowpower;
        return 0;
    }

    uint32 eeprom_read( uint32 address, uint32 count, uint8 *buff, bool async )
    {
        int i;
        volatile uint32 result;

        if ( (ee_status != eest_enable_rd) && (ee_status != eest_enable_wr) )
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


    uint32 eeprom_write( uint32 address, const uint8 *buff, uint32 count, bool async )
    {
        uint32 i;

        if ( ee_status != eest_enable_wr )
            return 0;

        if ( address >= EE_MAX_SIZE )
            return 0;
        if ( (address + count) > EE_MAX_SIZE )
            count = EE_MAX_SIZE - address;
        if ( count == 0 )
            return 0;

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
        while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );        // wait last address byte write

        if ( count < EE_MAX_WRITE_WO_DMA )
        {
            // write the data directly if not worth configuring DMA
            for (i=0; i<count; i++)
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
            HW_DMA_EE_Enable( DMAREQ_TX );                  // enable TX requests for SPI port
            HW_DMA_Send( DMACH_EE, buff, count );           // set up buffer for data to be received

            if ( async == false )
            {
                // not async - wait here for data to be transmitted entirely
                while ( DMA_EE_TX_Channel->CNDTR );                     // wait for data amount to be sent
                while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );    // wait TX empty
                while ( SPI_PORT_EE->SR & SPI_I2S_FLAG_BSY );           // wait last bit to shift out
                HW_DMA_EE_Disable( DMAREQ_TX );                         // disable DMA request for TX
                HW_Chip_EEProm_Disable();                               // CS _/-
            }
            else
            {
                // async operation
                ee_status = eest_write_in_progr;
            }
        }
        return count;
    }

    const uint8 ee_eb = 0xff;

    uint32 eeprom_erase(void)
    {
        if ( ee_status != eest_enable_wr )
            return 1;

        HW_DMA_Uninit( DMACH_EE );
        HW_DMA_Init( DMACH_EE | 0x100 );
        eeprom_write( 0x00, &ee_eb, 0x8000, true );
        ee_erase = EE_MAX_SIZE / 0x8000;
 
        return 0;
    }


    bool eeprom_is_operation_finished( void )
    {
        if ( ee_status < eest_read_in_progr )           // for speed optimization
            return true;

        if ( ee_status == eest_read_in_progr )          // if it is in read in progress state
        {
            if ( DMA_EE_RX_Channel->CNDTR == 0)         // check if DMA finished it's job
            {
                HW_Chip_EEProm_Disable();
                HW_SPI_Set_Rx_mode_only(SPI_PORT_EE, false);    // stop the unidirectional RX mode (stops the clock generation also)  
                HW_DMA_EE_Disable( DMAREQ_RX );                 // disable DMA request for RX on SPI
                ee_status = eest_enable_rd;                 
            }
            else
                return false;
        }
        else if ( ee_status == eest_write_in_progr )    // write in progress
        {
            if ( DMA_EE_TX_Channel->CNDTR == 0 )        // if DMA finished it's job
            {
                while ( (SPI_PORT_EE->SR & SPI_I2S_FLAG_TXE) == 0 );    // wait TX empty
                while ( SPI_PORT_EE->SR & SPI_I2S_FLAG_BSY );           // wait last bit to shift out
                HW_DMA_EE_Disable( DMAREQ_TX );                         // disable DMA request for TX
                HW_Chip_EEProm_Disable();                               // CS _/-  -> write operation started

                if ( ee_erase )
                {
                    ee_erase--;
                    if ( ee_erase )
                    {
                        ee_status = eest_enable_wr;
                        eeprom_write( ee_erase * 0x8000, &ee_eb, 0x8000, true );
                        return false;
                    }
                    else
                    {
                        HW_DMA_Uninit( DMACH_EE );
                        HW_DMA_Init( DMACH_EE );
                        ee_status = eest_enable_wr;                     // mark as finished
                        return true;
                    }
                    return false;
                }
                ee_status = eest_enable_wr;                             // mark as finished
                return true;
            }
            else
                return false;                               // signal busy since DMA didn't finished it's job
        }
        else                                                // ee_status >= eest_pwrup_in_progr
        {
            if ( internal_check_ID() )                      // if device responds with ID then everything is fine
            {
                if ( ee_status == eest_pwrup_in_progr_w )
                {
                    uint32 result;
                    internal_set_write_enable( true );
                    result = internal_get_status();
                    if ( (result & EE_SF_WREN) == 0 )
                        ee_status = eest_enable_rd;         // indicates failure
                    else
                        ee_status = eest_enable_wr;
                }
                else
                    ee_status = eest_enable_rd; 
                return true;
            }
            else 
                return false;
        }
        
        return true;
    }


/* Performance measurements:

        Wake up from deep sleep: 375us 
        

        CS -\___/- for CMD_RDIP: 2.3us

        Write small amount of data:
            operation (execution of routine for 6bytes in the same page) - 29.3us
            set up for write + command + address (first CS-\_ -> first data byte): 9.3us
            send 6 data bytes (byte0 first clock -> byte5 first clock): 7.7us
            last data clock -> CS_/-: 1.2us 

        Write data with DMA - 200bytes in the same page:
            operation 233us
            set up for write + command + address (first CS-\_ -> first data byte): 11.76us
            send 6 data bytes (byte0 first clock -> byte5 first clock): 5.72us
            last data clock -> CS_/-: 1.5us 

            8bytes ~OK for nonDMA

        Read small amount of data:
            operation (execution of routine for 6bytes in the same page) - 24.1us
            command + address (first CS-\_ -> first data byte): 6.8us
            receive 6 data bytes (byte0 first clock -> byte5 first clock): 13.6us  -> 2.72us/byte
            last data clock -> SC_/-: 1.5us

        Read with DMA 200bytes:
            operation (execution of routine for 6bytes in the same page) - 221us
            command + address (first CS-\_ -> first data byte): 12.2us
            receive 6 data bytes (byte0 first clock -> byte5 first clock): 5us     -> 1us/byte
            last data clock -> SC_/-: N/A

            - 4 byte should use DMA



        Benchmarks:
            Synch write:
                6bytes:     write op    22.6us
                            poll op     2.3us

                200bytes:   write op    216us
                            poll op     2.3us

                500bytes:   write op    562us
                            poll op     2.3us


            Async write:
                6bytes:     write op    22.6us
                            poll op     2.3us

                200bytes:   write op    16.4us
                            poll op     212us

                500bytes:   write op    16.4us
                            poll op     581us


            Wake up from deep sleep:    372us (polled)
                
            Synch read:
                6bytes:     read op     27.8us
                            poll op     2.3us

                200bytes:   read op     220us
                            poll op     2.3us

                500bytes:   read op     567us
                            poll op     2.3us

            Async read:
                6bytes:     read op     16.1us
                            poll op     12us

                200bytes:   read op     16.1us
                            poll op     205us

                500bytes:   read op     16.1us
                            poll op     552us


            Chip erase:     266ms   
    
*/
    






