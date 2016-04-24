#ifndef DISPHAL_INTERNAL_H
#define DISPHAL_INTERNAL_H

#ifdef __cplusplus
    extern "C" {
#endif

        
struct SDispHALstate
{
    uint32  busy:1;                 // generic busy flag, it is 1 when display operation in progress. set by API call, cleared by ISR

    // initialization phase
    uint32  disp_initted:1;         // display is initialized
    uint32  disp_iniprog:1;         // display init is in progress
    uint32  disp_uniniprog:1;       // display uninit in progress
    uint32  disp_updating:1;        // if display update is in progress

    // display memory and comm. related
    uint32  gmem_dirty:1;           // update request for display memory was made  
    uint32  cntr_dirty:1;           // update request for contrast was made
    uint32  spi_owner:1;            // if spi port is used by display

    // greyscale flip buffer
    uint32  gray_on:1;              // if grayscale displaying is on
};


struct SDispSend
{
    const uint8 *cmd_ptr;           // command pointer
    int cmd_idx;                    // index of the next command in the sequence
    int cmd_len;                    // length of the command list
    int gmem_line_start;            // start page of the graphic memory (8 pixel packages vertically)
    int gmem_line_end;              // end page of the graphic memory
    int tmr;                        // if non-zero then timeout is counted before executing the next step
};



struct SDispHALStruct
{
    struct SDispHALstate status;
    struct SDispSend     send;
    int                  contrast;  // contrast value ( 0x00 - 0xFE )

};




#ifdef __cplusplus
    }
#endif




#endif // DISPHAL_INTERNAL_H














