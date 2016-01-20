/*************************************************************************************
    nagyizee's grahpic library
    V 0.1

    API for simple graphic rendering for embedded, low memory devices

    Output format for graphic device in local ram  (used by display_HAL):
        16bit  RGB555       this will be displayed directly, no LUT conversion in HAL. Displaying mode depends on display module ( direct drive or buffered ) and it is handled in display_HAL
                Input color formats:
                    ARGB1555    for vectorials and bitmaps           - color is masked from 555 or trasparency is applied if 1000 is set
                    RGB555  for bitmaps
                    LUT4    for bitmaps          - note that lut[15] for LUT4 and lut[255] for LUT8 is always transparent
                    LUT8    for bitmaps
                note: for LUT bitmaps the pixels are converted in RGB by the bitmap streaming routine
        8bit LUT        buffered mode only - display_HAL will handle the data generation according to LUT and refresh
                Input color formats:
                    LUT8    for vectorials and bitmaps           - note that lut[255] for LUT8 is always transparent
        4bit LUT        buffered mode only - display_HAL will handle the data generation according to LUT and refresh
                Input color formats:
                    LUT4    for vectorials and bitmaps           - note that lut[15] for LUT4 is always transparent
        1bit BW     buffered mode only - display_HAL will handle the data refresh
                Input color formats:
                    LUT4    for bitmaps and vectorial            - accepts colors: 0 - black, 1 - white,  rest are transparent
                    BW  for bitmaps          - b/w pixel map, horizontally packed
                    Note that in this mode the LUT update has no sense - routine retuns immediately

    File structure:
        - LUT macros

        - Graphic library configuration
    
        - API structures and constants
        - API routines.
        These are used by the high level application
    
        - HAL routine defines
        These are external definitions - project implementation should provide the memory and
        the display-specific implementation of these routines

    NOTE: this library is not thread safe - use it in a single thread only

**************************************************************************************/

#ifndef _GRAPHIC_LIB_H_
#define _GRAPHIC_LIB_H_

#ifdef __cplusplus
    extern "C" {
#endif

#ifdef USE_GLIB_ON_EWARM
    #include "stm32f10x.h"
#endif
    #include "typedefs.h"           // should be provided by the project implementation
    #include "graphic_lib_user.h"   // user application should provide this

    // RGB 1555 macros - get R/G/B/A from LUT with RGB1555.  RGBA will be in 5 bit formats
    #define GLUT_GET_ALPHA(a)   ( a >> 15 )
    #define GLUT_GET_R5(a)      ( (a >> 10) & 0x1f )
    #define GLUT_GET_G5(a)      ( (a >> 5) & 0x1f )
    #define GLUT_GET_B5(a)      ( a & 0x1f )

    // RGB 888 macros - get R/G/B/A from LUT with RGB1555. RGB will be in 8 bit
    #define GLUT_GET_R8(a)      ( (a >> 7) & 0xf8 )
    #define GLUT_GET_G8(a)      ( (a >> 2) & 0xf8 )
    #define GLUT_GET_B8(a)      ( (a << 3) & 0xf8 )

    // make an RGB1555 color from 5 bit R/G/B/A
    #define GLUT_COMPOSE_FROM_RGB1555(alpha,r,g,b)  ( (uint16)( (alpha ? 0x8000 : 0x0000) | (r<<10) | (g<<5) | (b) ) )
    // make an RGB1555 color from 8 bit R/G/B/A
    #define GLUT_COMPOSE_FROM_RGB1888(alpha,r,g,b)  ( (uint16)( GLUT_COMPOSE_FROM_RGB1555( alpha, (r>>3), (g>>3), (b>>3) ) ) )

    // define some basic ARGB1555 colors
    #define GCOLOR_BLACK            0
    #define GCOLOR_GRAY_DARK        GLUT_COMPOSE_FROM_RGB1888(0, 0x40, 0x40, 0x40)
    #define GCOLOR_GRAY             GLUT_COMPOSE_FROM_RGB1888(0, 0x80, 0x80, 0x80)
    #define GCOLOR_GRAY_BIGHT       GLUT_COMPOSE_FROM_RGB1888(0, 0xC0, 0xC0, 0xC0)
    #define GCOLOR_WHITE            GLUT_COMPOSE_FROM_RGB1888(0, 0xFF, 0xFF, 0xFF)
    #define GCOLOR_RED_DARK         GLUT_COMPOSE_FROM_RGB1888(0, 0x80, 0x00, 0x00)
    #define GCOLOR_GREEN_DARK       GLUT_COMPOSE_FROM_RGB1888(0, 0x00, 0x80, 0x00)
    #define GCOLOR_BLUE_DARK        GLUT_COMPOSE_FROM_RGB1888(0, 0x00, 0x00, 0x80)
    #define GCOLOR_YELOW_DARK       GLUT_COMPOSE_FROM_RGB1888(0, 0x80, 0x80, 0x00)
    #define GCOLOR_CYAN_DARK        GLUT_COMPOSE_FROM_RGB1888(0, 0x00, 0x80, 0x80)
    #define GCOLOR_PURPLE_DARK      GLUT_COMPOSE_FROM_RGB1888(0, 0x80, 0x00, 0x80)
    #define GCOLOR_ORANGE           GLUT_COMPOSE_FROM_RGB1888(0, 0xff, 0x80, 0x00)
    #define GCOLOR_RED              GLUT_COMPOSE_FROM_RGB1888(0, 0xff, 0x00, 0x00)
    #define GCOLOR_GREEN            GLUT_COMPOSE_FROM_RGB1888(0, 0x00, 0xff, 0x00)
    #define GCOLOR_BLUE             GLUT_COMPOSE_FROM_RGB1888(0, 0x00, 0x00, 0xff)
    #define GCOLOR_YELOW            GLUT_COMPOSE_FROM_RGB1888(0, 0xff, 0xff, 0x00)
    #define GCOLOR_CYAN             GLUT_COMPOSE_FROM_RGB1888(0, 0x00, 0xff, 0xff)
    #define GCOLOR_PURPLE           GLUT_COMPOSE_FROM_RGB1888(0, 0xff, 0x00, 0xff)
    #define GCOLOR_TRANSPARENT      GLUT_COMPOSE_FROM_RGB1888(1, 0xff, 0x00, 0xff)

    enum EGraphicTextCursorType
    {
        graphicTextCursor_Hide              = 0,
        graphicTextCursor_UnderLine,
        graphicTextCursor_UnderLineBlink,
        graphicTextCursor_Block,
        graphicTextCursor_BlockBlink,
    };


    typedef int32 g_result;
    typedef uint16 LUTelem;

    #define GRESULT_OK               0
    #define GRESULT_PARAM_ERROR      -1
    #define GRESULT_FATAL_ERROR      -2
    #define GRESULT_NOT_IMPLEMENTED  -3
    #define GRESULT_NOT_PERMITTED    -4

    /**************************************************
    Configuration from user setup
    **************************************************/

    #define GDISP_WIDTH               CONF_DISP_WIDTH
    #define GDISP_HEIGHT              CONF_DISP_HEIGHT
    #define GDISP_PIXEL_FORMAT        CONF_DISP_PIXEL_FORMAT   // depends on representation of pixels in memory, isn't related to display hardware

    #define GASSERTION_LEVEL          CONF_GASSERTION_LEVEL    // runtime checks for implementation and usage failures, see valid values at GRAPHIC_ASSERTION_XXX defines

    #define GRAPHIC_1BIT_BLACK        CONF_GRAPHIC_1BIT_BLACK  // depends on LCD module - the bit value for black or white
    #define GRAPHIC_1BIT_WHITE        CONF_GRAPHIC_1BIT_WHITE
    /**************************************************/


    // Do not edit the following calculated values
    #if (GDISP_PIXEL_FORMAT == gpixformat_4bitLUT)
        #define GDISP_MAX_MEM_W       ( GDISP_WIDTH / 2 + ( GDISP_WIDTH & 0x01) )
        #define GDISP_MAX_MEM_H       ( GDISP_HEIGHT )
        #define GDISP_MAX_MEMORY      ( GDISP_MAX_MEM_W * GDISP_MAX_MEM_H )
    #elif (GDISP_PIXEL_FORMAT == gpixformat_1bit)
        #define GDISP_MAX_MEM_W       ( GDISP_WIDTH )
        #define GDISP_MAX_MEM_H       ( GDISP_HEIGHT / 8 + ( GDISP_HEIGHT % 8 != 0)  )
        #define GDISP_MAX_MEMORY      ( GDISP_MAX_MEM_W * GDISP_MAX_MEM_H )
    #elif (GDISP_PIXEL_FORMAT == gpixformat_8bitLUT)
        #define GDISP_MAX_MEM_W       ( GDISP_WIDTH )
        #define GDISP_MAX_MEM_H       ( GDISP_HEIGHT )
        #define GDISP_MAX_MEMORY      ( GDISP_MAX_MEM_W * GDISP_MAX_MEM_H )
    #elif (GDISP_PIXEL_FORMAT == gpixformat_16bitARGB)
        #define GDISP_MAX_MEM_W       ( GDISP_WIDTH * 2 )
        #define GDISP_MAX_MEM_H       ( GDISP_HEIGHT  )
        #define GDISP_MAX_MEMORY      ( GDISP_MAX_MEM_W * GDISP_MAX_MEM_H )
    #else
        #error "Need to add for the new format"
    #endif


    /**************************************************
    API routines
    **************************************************/

    // Init the graphic library. 
    // It will set up the grahpic memory pointer and the LookUpTable pointer provided by the application/project implementation
    // Will do the display module initialization at low level also 
    // params:
    //      memory - graphic memory array. Application must assure that this memory exists all the time, and it doesn't alter it's content directly
    //             - if CONF_GRAPHIC_INT_MEM is defined then this parameter can be NULL, memory is allocated inside of the library
    //      LUT - look up table for the 4 or 8 bit pixel formats. can be NULL for 1bit or 16bit formats
    // return:
    //      result
    g_result Graphics_Init( uint8 *memory, LUTelem *LUT );


    // Change the look-up table for the following operations
    // params:
    //      LUT - look up table for the 4 or 8 bit pixel formats.
    // return:
    //      result
    g_result Graphics_UpdateLUT( LUTelem *LUT );

    // Clear the display memory
    // params:
    //      color - depends on pixel format. see GRAPHIC_DISPLAY_PIXEL_FORMAT definition
    void     Graphics_ClearScreen( uint32 color );

    // Set the drawing window
    // !NOTE! it doesn't apply to PutPixel
    // defines a drawing window where graphics is generated
    // params:
    //      X1,Y1,X2,Y2 - window size xy1 - the upper corner, xy2 - the lower corner
    // return:
    //      result
    g_result Graphic_Window( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2 );

    // Shift a graphic window horizontally.
    // pixels from X1 -> X1+amount will be overwritten
    // pixels on X2-amount -> X2 will remain untouched, the application need to clear it up or reuse it
    // params:
    //      X1,Y1,X2,Y2 - window size xy1 - the upper corner, xy2 - the lower corner
    //      amount - pixels to be shifted.
    // return:
    //      result
    g_result Graphic_ShiftH( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2, uint32 amount );


    // ------------ Graphical primitives ---------------

    // params:
    //      X_poz, Y_poz - pixel coordinates
    //      color - color - depends on pixel format. see GRAPHIC_DISPLAY_PIXEL_FORMAT definition
    //      fillColor - color of the filled surface - depends on pixel format. see GRAPHIC_DISPLAY_PIXEL_FORMAT definition
    //      X1,Y1, X2,Y2 - line / rectangle coordinates
    //      radius - circle radius
    //      width - line width.  0 - no line (used for filled objects only, others will fail), 1 - normal pixel line (optimized run), 2 - 10 - use NxN dimension sprites for rendering 
    //      
    // return:
    //      result:

    // put a pixel with a specified color to a specified coordinate
    g_result Graphic_PutPixel( uint32 X_poz, uint32 Y_poz, int32 color );

    // gets a pixel's color  (depends on pixel format)
    uint32   Graphic_GetPixel( uint32 X_poz, uint32 Y_poz );

    // set line width
    // applies currently to non anti-aliassed line and rectangle
    g_result Graphic_SetWidth( uint32 width );

    // set color for outlines (line, rectangle, circle)
    g_result Graphic_SetColor( int32 color );

    // draws a simple line
    g_result Graphic_Line( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2 );

    // draws an antialiassed line
    // works only in gpixformat_16bitARGB, and currently with line width 1 pixel
    g_result Graphic_AALine( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2 );

    // draws a rectangle 
    g_result Graphic_Rectangle( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2 );

    // draws a filled rectangle, if no need for outline, set the width to 0
    g_result Graphic_FillRectangle( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2, int32 fillColor );

    // draws a circle
    g_result Graphic_Circle( uint32 X1, uint32 Y1, uint32 radius );

    // draws a filled circle, if no need for outline, set the width to 0
    g_result Graphic_FillCircle( uint32 X1, uint32 Y1, uint32 radius, int32 fillColor );

    // draws a Bezier Spline between coordinates X[0] Y[0] - X[4] Y[4] with control points X[1] Y[1] and X[2] Y[2]
    g_result Graphic_DrawSpline( int32 *Xarray, int32 *Yarray );


    // ------------ draw bitmap ---------------
    //
    // About the input_format parameter:
    //      - if gpixformat_NA      - then the library's defined native color format is used for rendering pixels
    //      - gpixformat_1bit       - working for 16bit/8bit/4bit library defined color modes, uses bitmap LUT with 2 locations
    //      - gpixformat_4bitLUT    - working for 16bit/8bit library defined color modes, uses bitmap LUT with 16 locations
    //      - gpixformat_8bitLUT    - working for 16bit library defined color mode, uses bitmap LUT with 255 location
    //  bitmap LUT is 8bit per entry for color modes 8bit and 4bit library defined color format
    //             is LUTelem per entry for color mode 16bit

    // Set up the LUT pointer for use at drawing witmap with different color format - see abowe
    // LUT array represented by LUT pointer needs to be available till bitmap drawing is finished
    g_result Bitmap_UseLUT( void *LUT );

    // Start drawing a bitmap stream. Data is provided by Bitmap_DrawStream() till the compete image is drawn
    // or Bitmap_StopDrawing() is given.
    // pixel format is given in GRAPHIC_DISPLAY_PIXEL_FORMAT define
    // For 1bit and 4 bit formats if there are no multiple of 8 or 2 pixels then the last part of a scanline on X, data must be set to 0 bits to complete the byte
    // params:
    //      X_poz, Y_poz - the relative start coordinate of the picture - can be negative for off screen displaying
    //      X_dim, Y_dim - picture size, can be bigger than the actual display, picture will be cropped
    //      upside_down  - picture will be displayed from bottom-up - usefull for BMP streaming from file
    //      input_format - see description abowe
    // return:
    //      result
    g_result Bitmap_StartDrawing( int32 X_poz, int32 Y_poz, uint32 X_dim, uint32 Y_dim, bool upside_down, uint32 input_format );

    // Data stream for the picture
    // params:
    //      data - data buffer.
    //      size - buffer size - NOTE!!! buffer content must be aligned to pixel format, for ex. 16bit per pixel needs buffers with complete pixels ( %2 = 0 )
    // return:
    //      result:     1 - displaying is completed, no need for BackGround_StopDrawing()
    g_result Bitmap_DrawStream( const uint8 *data, uint32 size );

    // Stop data streaming
    // return:
    //      result
    g_result Bitmap_StopDrawing( void );

    // returns the needed memory for copying a picture area
    uint32   Bitmap_GetAreaMemorySize ( uint32 X_dim, uint32 Y_dim );

    // Copies a picture area from the graphic memory
    // params:
    //      X_poz, Y_poz - upper corner coordinates
    //      X_dim, Y_dim - area dimensions          - Note: Coordinate + Dimension should be inside of the drawing area
    //      bitmap_buffer 
    //      buffer_size  - size of the given bitmap buffer. It stops if buffer_size is reached
    // return:
    //      result
    g_result Bitmap_CopyArea( uint32 X_poz, uint32 Y_poz, uint32 X_dim, uint32 Y_dim, uint8 *bitmap_buffer, uint8 buffer_size );

    // draws a bitmap
    // params:
    //      bitmap_buffer - buffer with bitmap data
    //      X_poz, Y_poz  - upper corner coordinates
    //      X_dim, Y_dim  - bitmap dimensions without extrusion (original bitmap size)
    //      input_format  - see description abowe
    //      extrudeH, extrudeV   - bitmap extrusion from the middle portion with given pixel number - usefull to generate buttons and stuff
    // return:
    //      result
    g_result Bitmap_Draw( uint8 *bitmap_buffer, uint32 X_poz, uint32 Y_poz, uint32 X_dim, uint32 Y_dim, uint32 input_format, uint32 extrudeH, uint32 extrudeV );



    // ------------ Draw text with different font type ---------------
    // Set up text font and colors. Font should be provided by application and this need to assure that the font structure exists till the text drawing
    // using this font is finished
    // params:
    //      font - font data buffer. Should hold a valid header, and needs to have a valid length
    //             if =null it will use the default small font in case if CONF_GRAPHIC_INT_SSFONT is defined
    //      monospace - if true then the characters will use the same display space always (it will take the width of character '&')
    //      color - text color
    //      background - background color, -1 is transparent
    // return:
    //      result:
    g_result Gtext_SetFontAndColors(const uint8 *font, bool monospace, int32 color, int32 background );

    // set the graphic coordinates of a text
    // params:
    //      X_poz, Y_poz - text position (currently need to fit inside of screen)
    // return:
    //      result:
    g_result Gtext_SetCoordinates( int32 X_poz, int32 Y_poz );


    // Put a character or a text to the specified coordinates
    // X pointer will advance to the next character position
    // params:
    //      text - null terminated text string
    // return:
    //      result:
    g_result Gtext_PutChar( uint8 chr );
    g_result Gtext_PutText( const char *text );

    // Get the width or height in pixels of a character from the selected font
    // params:
    //      chr - character to examine
    // return:
    //      result: <0 if error (font doesn't support that character).
    //              otherwise returns the size in pixels (including white space underneath if font has such a thing
    g_result Gtext_GetCharacterWidth( uint8 chr );
    g_result Gtext_GetCharacterHeight( void );


    /**************************************************
    HAL routines

    external, display and system specific implementation is needed
    **************************************************/

    // Init the display hardware, return 0 in case of success
    // Gmem is the graphic memory used by the graphic_lib
    extern uint32 DispHAL_Init(uint8 *Gmem);

    // For buffered displaying when library output format is LUT4 or LUT8
    // LUT is the pointer to an application reserved LUT for the current displaying session
    extern uint32 DispHAL_UpdateLUT( LUTelem *LUT );

    // Register need for update for a rectangular zone in pixels from coordinates Poz_X, Poz_Y, of size Width x Height
    // depends on hardware implementation, this can update imediately the requested zone, or in a timed way if data sending takes time.
    extern void   DispHAL_NeedUpdate(  uint32 X1, uint32 Y1, uint32 X2, uint32 Y2  );

#ifdef __cplusplus
    }
#endif


#endif
