#ifndef DISPHAL_H
#define DISPHAL_H

#ifdef __cplusplus
    extern "C" {
#endif

    #include "graphic_lib.h"
      
      
    // Routines for graphic library
    // 
    // init the display hardware subsystem 
    uint32 DispHAL_Init(uint8 *Gmem);

    // update look up table (not used for 1 bit screens )   - not used for the current HAL implementation - but needed for compatibility with graphic library
    uint32 DispHAL_UpdateLUT( LUTelem *LUT );

    // routine for graphic library to tell the HAL about the window it modified - not used for the current HAL implementation - but needed for compatibility with graphic library
    void DispHAL_NeedUpdate(  uint32 X1, uint32 Y1, uint32 X2, uint32 Y2  ); 



    // Routines for application level (system specific)
    // 

    // --- Special feature for hygro project grayscale ---  no generic implementation because of memory constraints
    // Send the current graphic content to the grayscale flip buffer 
    void DispHal_ToFlipBuffer( void );

    // clear the flip buffer, disabling the grayscale displaying
    void DispHal_ClearFlipBuffer( void );


    // Called by application to tell when it finished drawing and display can be updated
    void DispHAL_UpdateScreen( void );

    // Turn on the display module.
    int  DispHAL_Display_On( void );
    
    // Turn off the display module.
    int  DispHAL_Display_Off( void );

    // sets up contrast value, cval = 0 - 63
    void DispHAL_SetContrast( int cval );

    // checks if there is an operation in progress. Typical usecases: when SPI bus needs to be used for other things or want to go to power down mode.
    bool DispHAL_DisplayBusy( void );

    // release SPI. Other modules can use SPI only if this routine returns with true.
    // call fails if display is allready busy transferring data or command
    bool DispHAL_ReleaseSPI( void );

    // ISR routine - should be called in fixed periods from a timer ISR - timing is important (minimum timing: 250us)
    void DispHAL_ISR_Poll(void);

    // Application polling routine. Should be called periodically. Returns true for display busy
    uint32 DispHAL_App_Poll(void);


#ifdef __cplusplus
    }
#endif




#endif // DISPHAL_H
