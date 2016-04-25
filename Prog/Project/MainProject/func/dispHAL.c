/**************************************************************
 *
 *      Display Hardvare Abstraction Layer
 * 
 *      Display module driver for Crystalfontz CFAL12864N-A-B1
 *
 *      Display module is operated in serial mode with
 *      double buffering mode - meaning there is an internal graphic
 *      memory provided by the upper layer graphic library which is
 *      filled with graphic data, then fed to the display when application
 *      signals the completion of drawing. 
 *      Using SPI DMA output, only write access, no read.
 *
 *
 *      Timings:    
 *          100ms - dc/dc convertor startup -> rest of operations
 *          50ms - after display on command
 *
 *
 *      Interface:
 *          - DispHAL_Init() should be called first to init the library. graphic memory pointer should be provided
 *            (this is usually called by the graphic library at init)
 *         
 *        Display Operations done by the application: 
 *          - DispHAL_Display_On() powers up and initialize the display module.
 *          - DispHAL_Display_Off() powers down the display
 *          - DispHAL_SetContrast() set the contrast of the display
 *          - DispHAL_UpdateScreen() trigger a display refresh with the new content from the graphic memory ( sort of pageflip )
 * 
 *          All these operations are asynchronous and autonomous and are driven by ISR, the application should not wait for command 
 *          completion for neither of these operations, just call them and forget them.
 *
 *        There are two essential routines to be called periodically to make the whole stuff work:
 *          - DispHAL_ISR_Poll() should be called on a 0.5ms timer interval by a timer ISR - this is the main workflow routine of 
 *            of the whole display operations, and command timings.
 *          - DispHAL_App_Poll() should be called by the application periodically to run the queue of operations called by the application.
 *            Timing is not important here. This routine retunrs also the busy state of the display (if operations are running in background)
 * 
 *        Using the same SPI interface by the application for other stuff:
 *          Busy status can be asked with DispHAL_DisplayBusy(). This is important when the application wants to use the SPI bus for other things.
 *          When SPI is needed for the application, it should wait till DispHAL_DisplayBusy() returns false and it should call DispHAL_ReleaseSPI().
 *          When a new display operation is done by the application, the SPI bus is reaquired automatically. So application should take care to
 *          finish the other SPI operations on this interface.
 * 
 *      How it works:
 *          When a display operation is called by the application, if display is not busy - it will start immediately to process it, otherwise it will
 *          mark it and start to execute when the previous operation is finished. This is done in the application loop routine DispHAL_App_Poll().
 *          Display operation routines return immediatelly, operations are async. and no result or busy status need to be checked between them.
 *          
 *          Operation execution is triggered by display operation routine or by the loop routine (if there are queued ops. ), they set up states for the state
 *          machine implemented in the DispHAL_ISR_Poll() and DispHAL_ISR_DMA_Complete(). Commands / data is sent to SPI through DMA, finishing each chunk will
 *          trigger the DMA ISR which will set up the next chunk or the next command sequence. DispHAL_ISR_Poll() is responsible for the timings between each
 *          command from a sequence, it works in cooperation with the DMA ISR.
 *          DispHAL_App_Poll() will detect if ISR sequences are finished so operation is complete and looks after other marked operations to be done and
 *          starts them up. 
 *          There is no operation queue. DispHAL_Display_On(), DispHAL_SetContrast(), DispHAL_UpdateScreen() are marking flags, that they need to be executed
 *          DispHAL_Display_Off() will cancel any marked operation and powers off the display module.
 *          As told abowe, SPI bus is acquired and configured at any display operation routine automatically, so any pending operation on it will be 
 *          interrupted. 
 */


#include <string.h>
#include <stdlib.h>

#include "hw_stuff.h"
#include "dispHAL.h"
#include "dispHAL_internal.h"

static struct SDispHALStruct hal;
uint8 *gmem;

uint8 gflip[660];               // special feature for Hygro project - greyscale flip buffer for the 0:16 - 109:63 area ( 110pix wide, 48pix high)

#define CSPEC_VPON      0x01
#define CSPEC_VPOFF     0x02
#define CSPEC_RESET     0x03
#define CSPEC_DLY50     0x04
#define CSPEC_UMEM      0x05                        // this should be the last command in a command sequence

// Predefined command list for startup
#define CMDLIST_SIZE_STARTUP        30              // size of startup sequence command list (included the special commands)
const uint8 cmdlist_startup[] = {   0xAE,               // Display off
                                    0xA1,               // Segment mapping (0xA0 normal, 0xA1 reversed)
                                    0xDA, 0x12,         // set default hw cfg for common pads
                                    0xC8,               // Common output scan direction (0xC0 = (0 - COM[N-1]), 0xC8 = (COM[N-1] - 0)
                                    0xA8, 0x3F,         // Set multiplex ratio - set for 64
                                    0xDB, 0x23,         // set  VCOMH 
                                    0xD9, 0x22,         // set precharge period
                                    0xD5, 0x60,         // Set Display clock divide ratio/oscillator frequency, Set for -5%
                                    0xD3, 0x00,         // no display offset
                                    0x40,               // Set display start line (0x40 = start at line 0)
                                    0x81, 0x80,         // Contrast set
                                    0xFF, CSPEC_VPON,   // -- special command for turning on Vpanel and making 100ms pause after it
                                    0xAD, 0x8A,         // DC-DC voltage converter OFF, 0x8A = OFF, 0x8B = ON
                                    0xAF,               // Display on
                                    0xFF, CSPEC_DLY50,  // -- special command to pause the operations for 50ms
                                    0x40, 0x02, 0x10,   // set disp. start line, column addressed to 0
                                    0xFF, CSPEC_UMEM    // update display memory from gmem
                                };

// Predefined command list for shutdown
#define CMDLIST_SIZE_SHUTDOWN       5               // size of startup sequence command list (included the special commands)
const uint8 cmdlist_shutdown[] = {  0xAE,               // Display off
                                    0xFF, CSPEC_VPOFF,  // -- special command for turning off Vpanel and making 100ms pause after it
                                    0xFF, CSPEC_RESET   // -- special command to assert RESET signal
                                };

// Command list for other application specific operation
#define CMDLIST_SIZE_CUSTOM     8
uint8   cmdlist_custom[CMDLIST_SIZE_CUSTOM];


static void disp_spi_take_over( void )
{
    if ( hal.status.spi_owner )
        return;                     // return if spi is allready under control
    hal.status.spi_owner = 1;

    // init SPI port
    HW_SPI_interface_init( SPI_PORT_DISP, SPI_BaudRatePrescaler_4);     // gives 4MHz

    // init the DMA for TX
    HW_DMA_Uninit( DMACH_DISP );            // for refresh
    HW_DMA_Init( DMACH_DISP );              // init the DMA channel - just need to set address and start
    HW_DMA_Disp_Enable(DMAREQ_TX);  // enable the SPI port's dma request
}


static void disp_spi_release( void )
{
    if ( hal.status.spi_owner == 0 )
        return;

    // uninit dma
    HW_DMA_Disp_Disable(DMAREQ_TX); // disable the SPI port's dma request
    HW_DMA_Uninit(DMACH_DISP);      // uninit the DMA channel
    hal.status.spi_owner = 0;
}



/********************************************************
*
*       ISR routines
*
********************************************************/

#define GREY_OFF            0x00
#define GREY_FIELD_MAIN     0x01            // main graphic content - display is updated
#define GREY_FIELD_SEC      0x02            // secondary graphic content from gflip buffer - only region is updated 

#define GREY_LIM_TOFLIP     19              // 20ms - main content - 20ms 
#define GREY_LIM_TOMAIN     28              // 30ms - grey content - 10ms  => ~30fps


///   18 - 27:  -- small line (brighter), fast scan
///   18 - 28:  -- large line (brighter), slow scan
///   19 - 28:  -- small line (brighter), slow scan - most optimal till now
///   20 - 29:  -- dirty line (br/dark), medium scan

volatile bool   isr_op_finished = false;
volatile bool   isr_busy = false;
volatile uint32 isr_grey_on = GREY_OFF;     // 0 - not active, 1,2 - main image, 3 - grey image
volatile uint32 isr_grey_ctr = 0;

// !!!! NOTE !!!! this routine is used inside ISR, make sure to protect by interrupt disable if it is called from application !!!!
static void disp_isr_internal_setup_display_page_command( void )
{
    hal.send.cmd_ptr = cmdlist_custom;
    hal.send.cmd_idx = 0;
    hal.send.cmd_len = 5;

    if ( isr_grey_on != GREY_FIELD_SEC )
        cmdlist_custom[0] = (uint8)(0xB0 + hal.send.gmem_line_start);       // page address
    else
        cmdlist_custom[0] = (uint8)(0xB0 + hal.send.gmem_line_start + 2);   // page address for the flip buffer (shifted down by 16 pixels)
    
    cmdlist_custom[1] = 0x02;                                       // coloumn address low
    cmdlist_custom[2] = 0x10;                                       // coloumn address hi
    cmdlist_custom[3] = 0xFF;                                       // display page line special command
    cmdlist_custom[4] = CSPEC_UMEM;
}


// !!!! NOTE !!!! it is used by ISR only, never call it from application !!!!
static inline void disp_isr_internal_run_update_gmem( void )
{
    HW_Chip_Disp_Enable();           // chip select
    HW_Chip_Disp_BusData();          // assert the data signal

    if ( isr_grey_on != GREY_FIELD_SEC )
        HW_DMA_Send( DMACH_DISP, gmem + hal.send.gmem_line_start * GDISP_MAX_MEM_W, GDISP_MAX_MEM_W );
    else
        HW_DMA_Send( DMACH_DISP, gflip + hal.send.gmem_line_start * 110, 110 );     // special case for Hygro project

    // create the command list for the next group (if existent)
    if ( hal.send.gmem_line_start < hal.send.gmem_line_end )
    {
        hal.send.gmem_line_start++;
        disp_isr_internal_setup_display_page_command();
    }
}



// !!!! NOTE !!!! this routine is used inside ISR, make sure to protect by interrupt disable if it is called from application !!!!
static bool disp_isr_internal_run_cmd_sequence( void )
{
    // it will send display commands till end, or till a special command
    int to_send = 0;

    // -- if all the commands are sent - finalize the operation
    if ( hal.send.cmd_idx == hal.send.cmd_len )
    {
        HW_Chip_Disp_Disable();
        return true;            // notify the finishing of operations
    }

    // -- if special command
    if ( hal.send.cmd_ptr[0] == 0xFF )
    {
        hal.send.cmd_idx += 2;
        switch ( hal.send.cmd_ptr[1] )
        {
            case CSPEC_DLY50:
                hal.send.tmr = 20;     // 5 ms timeout
                break;
            case CSPEC_RESET:           // assert reset signal
                HW_Chip_Disp_Reset();
                hal.send.tmr = 1;       // 250us timeout
                break;
            case CSPEC_VPOFF:
                HW_PWR_Disp_Off();
                hal.send.tmr = 200;    // 50ms timeout. This is needed for capacitors to be discharged
                break;
            case CSPEC_VPON:
                HW_PWR_Disp_On();
                hal.send.tmr = 20;    // 5ms timeout. Chargepump normally starts at 450us (measurd)
                break;
            case CSPEC_UMEM:
                disp_isr_internal_run_update_gmem();
                return false;           // do not break the PTR
        }
        hal.send.cmd_ptr += 2;
        return false;
    }

    // -- otherwise it is normal display command
    HW_Chip_Disp_Enable();           // chip select
    HW_Chip_Disp_BusCommand();       // assert the command signal

    // find the end of the contiguous display command stream
    while ( (to_send + hal.send.cmd_idx) < hal.send.cmd_len )
    {
        if ( *(hal.send.cmd_ptr + to_send) == 0xFF )
            break;
        to_send++;
    }

    HW_DMA_Send( DMACH_DISP, hal.send.cmd_ptr, to_send );
    hal.send.cmd_ptr += to_send;
    hal.send.cmd_idx += to_send;

    return false;
}

#define ISS_WAIT_TIMEOUT    0x01
#define ISS_DMA_TRANF       0x02

// Frame update rate = 100Hz    (10ms)

void DispHAL_ISR_Poll(void)
{
    if ( hal.send.tmr )
    {
        hal.send.tmr--;
        if ( hal.send.tmr == 0 )
        {
            // there was a timeout in the command sequence - proceed with the next command in sequence
            if ( disp_isr_internal_run_cmd_sequence() )
            {
                // command sequence finished
                isr_op_finished = true;
                isr_busy = false;
           }
        }
    }

    if ( isr_grey_on )  // greyscaling is on - special case for Hygro project
    {
        isr_grey_ctr++;
        if ( isr_busy == false )
        {
            bool upd_disp = false;
            if ( isr_grey_on == GREY_FIELD_MAIN )       // main field active
            {
                if ( isr_grey_ctr >= GREY_LIM_TOFLIP )  // reached the flip point
                {
                    isr_grey_on = GREY_FIELD_SEC;
                    // launch partial display update with the grey data
                    upd_disp = true;
                }
            }
            else
            {
                if ( isr_grey_ctr >= GREY_LIM_TOMAIN )  // reached point where main display content should be displayed
                {
                    isr_grey_on = GREY_FIELD_MAIN;
                    isr_grey_ctr = isr_grey_ctr % GREY_LIM_TOMAIN;
                    // launch complete display redraw
                    upd_disp = true;
                }
            }

            if ( upd_disp )
            {
                hal.send.gmem_line_start    = 0;
                if ( isr_grey_on == GREY_FIELD_SEC )
                    hal.send.gmem_line_end      = 5;        // display the flip buffer - it has only 6 lines
                else
                    hal.send.gmem_line_end      = 7;        // display all the graphic memory
                disp_isr_internal_setup_display_page_command();
                disp_isr_internal_run_cmd_sequence( );  // run the command sequence
                isr_busy = true;
            }

        }
    }
}


// this is called directly from the ISR - define it in stm32f10x_it.c also
void DispHAL_ISR_DMA_Complete(void)
{
    // clear the interrupt request
    HW_LED_On();
    DMA1->IFCR |= DMA_DISP_IRQ_FLAGS;

    while ( SPI_PORT_DISP->SR & SPI_I2S_FLAG_BSY );         // wait the data to be transmitted for sure
    HW_Chip_Disp_Disable();                                 // unselect display
    DMA_DISP_TX_Channel->CCR     &= (uint16_t)(~DMA_IT_TC); // disable IRQ. It will be reenabled with a new data packet

    if ( disp_isr_internal_run_cmd_sequence() )
    {
        // command sequence finished
        isr_op_finished = true;
        isr_busy = false;
    }
}



/********************************************************
*
*       helper routines
*
********************************************************/

static int disp_internal_launch_onoff_sequence( bool on )
{
    __disable_interrupt();
    if ( isr_busy )
    {
        __enable_interrupt();
        return 1;
    }
    hal.send.cmd_idx = 0;
    if ( on )
    {
        hal.send.cmd_ptr = cmdlist_startup;
        hal.send.cmd_len = CMDLIST_SIZE_STARTUP;
        hal.send.gmem_line_start    = 0;
        hal.send.gmem_line_end      = 7;        // display all the graphic memory
    }
    else
    {
        hal.send.cmd_ptr = cmdlist_shutdown;
        hal.send.cmd_len = CMDLIST_SIZE_SHUTDOWN;
    }
    isr_busy = true;
    disp_isr_internal_run_cmd_sequence( );      // run the command sequence
    __enable_interrupt();
    hal.status.busy = 1;
    return 0;
}


static int disp_internal_update_gmem( void )
{
    disp_spi_take_over();   // take the spdif control

    __disable_interrupt();
    if ( isr_busy )
    {
        __enable_interrupt();
        return 1;
    }
    hal.send.gmem_line_start    = 0;
    hal.send.gmem_line_end      = 7;        // display all the graphic memory
    disp_isr_internal_setup_display_page_command();
    disp_isr_internal_run_cmd_sequence( );  // run the command sequence
    isr_busy = true;
    __enable_interrupt();
    hal.status.busy = 1;
    return 0;
}


static int disp_internal_update_contrast( void )
{
    disp_spi_take_over();   // take the spdif control

    __disable_interrupt();
    if ( isr_busy )
    {
        __enable_interrupt();
        return 1;
    }
    hal.send.cmd_ptr = cmdlist_custom;
    hal.send.cmd_idx = 0;
    hal.send.cmd_len = 2;

    cmdlist_custom[0] = 0x81;               // contrast setup
    cmdlist_custom[1] = (uint8)hal.contrast; 
    disp_isr_internal_run_cmd_sequence( );  // run the command sequence
    isr_busy = true;
    __enable_interrupt();
    hal.status.busy = 1;
    return 0;
}

/********************************************************
*
*       API routines
*
********************************************************/


uint32 DispHAL_Init(uint8 *Gmem)
{
    memset( (void*)(&hal), 0, sizeof(hal) );

    HW_PWR_Disp_Off();
    HW_Chip_Disp_Disable();
    HW_Chip_Disp_Reset();

    gmem = Gmem;
    hal.contrast = 0x80;

    return 0;
}


uint32 DispHAL_UpdateLUT( LUTelem *LUT )
{
    // nothing to do here
    return 0;
}


void DispHAL_NeedUpdate(  uint32 X1, uint32 Y1, uint32 X2, uint32 Y2  )
{

}


void DispHAL_UpdateScreen( void )
{
    if ( isr_grey_on )              // display is automatically updated on time base at timer ISR if greyscale is active
        return;

    hal.status.gmem_dirty = 1;      // mark memory update request
    if ( (hal.status.disp_initted == 0) || ( hal.status.busy ) )     // return if memory update requested and display is busy or not initted
        return;

    if (disp_internal_update_gmem())
        return;                     // if update gmem procedure failed (ISR is busy) - leave the gmem_dirty flag
    hal.status.gmem_dirty = 0;
}

void DispHal_ToFlipBuffer( void )
{
    int i;
    for (i=2; i<8; i++)
    {
        memcpy( gflip + 110 * (i-2), gmem + 128 * i, 110 );
    }

    hal.status.gmem_dirty = 0;

    if ( isr_grey_on == GREY_OFF )
    {
        isr_grey_on = GREY_FIELD_MAIN;
    }
}

void DispHal_ClearFlipBuffer( void )
{
    if ( isr_grey_on == GREY_OFF )
        return;

    isr_grey_on = GREY_OFF;
    hal.status.gmem_dirty = 1;
}

int DispHAL_Display_On( void )
{
    if ( hal.status.disp_uniniprog )                            // operation not permitted till uninit isn't finished
        return -1;
    if ( hal.status.disp_initted || hal.status.disp_iniprog )   // display is allready initted or in progress
        return 0;

    isr_grey_on = GREY_OFF;
    hal.status.gmem_dirty = 0;
    hal.status.disp_iniprog = 1;                                // initialization in progress
    hal.status.disp_updating = 1;                               // mark this because display content will be updated also

    HW_Chip_Disp_UnReset();      // de-assert the reset signal
    disp_spi_take_over();   // take the spdif control

    disp_internal_launch_onoff_sequence( true );                // no need for check since display off will disable greyscale

    return 0;
}


int DispHAL_Display_Off( void )
{
    if ( hal.status.disp_iniprog )                                          // operation not permitted till uninit isn't finished
        return -1;
    if ( (hal.status.disp_initted == 0) || (hal.status.disp_uniniprog) )    // display is allready initted or in progress
        return 0;
    hal.status.disp_uniniprog = 1;
    isr_grey_on = GREY_OFF;
    hal.status.gmem_dirty = 0;                  // clear any memory update request
    hal.status.cntr_dirty = 0;
    disp_spi_take_over();                       // take the SPI control

    if ( hal.status.busy == 0 )
    {
        // launch the sequence only after other display operations are finished. This will be launched automatically by the background code
        if ( disp_internal_launch_onoff_sequence( false ) == 0 )
            hal.status.disp_updating = 1;       // mark that we are really uninitting
    }

    return 0;
}


void DispHAL_SetContrast( int cval )
{
    if ( cval >= 0xff )
        hal.contrast = 0xfe;
    else
        hal.contrast = cval;

    if ( (hal.status.disp_initted == 0) || ( hal.status.busy ) || isr_grey_on )     // return if memory update requested and display is busy or not initted
    {
        hal.status.cntr_dirty = 1;
        return;
    }
    if ( disp_internal_update_contrast() )
        hal.status.cntr_dirty = 1;                  // if operation start failed because of ISR busy - retry later
}


bool DispHAL_DisplayBusy( void )
{
    return hal.status.busy;
}


bool DispHAL_ReleaseSPI( void )
{
    if ( hal.status.busy )
        return false;

    HW_Chip_Disp_Disable();
    disp_spi_release();
    return true;
}


uint32 DispHAL_App_Poll(void)
{
    // check 
    if ( isr_op_finished )
    {
        isr_op_finished = false;

        // in case if display was busy with an operation
        if ( hal.status.busy )
        {
            if ( hal.status.disp_iniprog )          // init sequence finished
            {
                hal.status.disp_iniprog = 0;
                hal.status.disp_initted = 1;
                hal.status.disp_updating = 0;
            }
            else if ( hal.status.disp_uniniprog )   // uninit in progress - see below
            {
                if ( hal.status.disp_updating )     // this means that the op_finished was given for uninit
                {
                    hal.status.disp_uniniprog = 0;
                    hal.status.disp_initted = 0;
                    hal.status.disp_updating = 0;
                }
                else                                // op_finished was given for a previous command, and we need to finish the uninit
                {
                    if ( disp_internal_launch_onoff_sequence( false ) == 0 )
                        hal.status.disp_updating = 1;
                    return SYSSTAT_DISP_BUSY;
                }
            }
            else if ( hal.status.disp_updating )    // display update finished
            {
                hal.status.disp_updating = 0;
            }
            hal.status.busy = 0;
        }
    }

    // check for request to update
    if ( hal.status.gmem_dirty && (hal.status.busy == 0) )  // if memory update requested and display is not busy
    {
        if ( disp_internal_update_gmem() == 0 )
            hal.status.gmem_dirty = 0;
    }
    if ( hal.status.cntr_dirty && (hal.status.busy == 0) )  // if contrast update requested and display is not busy
    {
        if ( disp_internal_update_contrast() == 0 )
            hal.status.cntr_dirty = 0;
    }

    if ( isr_grey_on || hal.status.busy )
    {
        return SYSSTAT_DISP_BUSY;
    }
    return PM_DOWN;
}


