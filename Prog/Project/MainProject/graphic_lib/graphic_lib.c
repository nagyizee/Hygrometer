/*************************************************************************************
    nagyizee's grahpic library implementation
    V 0.1
**************************************************************************************/

#ifdef ARM_PLATFORM
#elif QT_PLATFORM
#elif ON_QT_PLATFORM
#else
//    #include <ansi_c.h>
#endif

#include <math.h>
#include <string.h>
#include "graphic_lib.h"


#ifdef USE_GLIB_ON_EWARM
int abs( int val )
{
    if ( val < 0 )
        return -val;
    else
        return val;
}

#endif


struct SBitmapDrawing
{
    int16  X_poz;
    int16  Y_poz;
    uint16  X_dim;
    uint16  Y_dim;

    int16  X_crt;
    int16  Y_crt;

    bool    go_simple;          // optimized run - picture is inside of display frame, input format corresponds to output format and no upside-down

    uint32  pixel_format;
    bool    upside_down;
};


struct STextRendering
{
    const uint8 *pFont;     // pointer to the font data
    int16 monospace;        // width of the '&' character, if 0 then it will use variable space
    int16 color;            // text color, -1 transparent
    int16 bkgnd;            // text background, -1 transparent

    int16 Xpoz;             // text cursor in graphic coordinates
    int16 Ypoz;             //
};

struct SDrawWindow
{
    bool use_window;
    uint32 X1;
    uint32 Y1;
    uint32 X2;
    uint32 Y2;
};


struct S_GraphicLibVariables
{
    LUTelem  *LUT;

    uint32  line_width;
    int32   color;

    struct SBitmapDrawing               bmp_draw;
    struct STextRendering               text;
    struct SDrawWindow                  draw_window;

} G_var;


#ifdef CONF_GRAPHIC_INT_MEM
    uint8   g_mem[ GDISP_MAX_MEMORY ];
#else
    uint8   *g_mem;
#endif


#ifdef CONF_GRAPHIC_INT_SSFONT
// SystemFont
// Static Buffer in data space for font 'SystemFontData'
const uint8 SystemFontData[] = {0x25, 0x08, 0x05, 0x84, 0x10, 0x42, 0x00, 0x01, 0x4A, 0x01, 0x00, 0x00, 0x00, 0x4A, 0x7D, 0xF5,
                              0x95, 0x02, 0xC4, 0x17, 0x47, 0x1F, 0x01, 0x73, 0x22, 0x22, 0x72, 0x06, 0x26, 0x15, 0x51, 0x93,
                              0x05, 0x86, 0x08, 0x00, 0x00, 0x00, 0x88, 0x08, 0x21, 0x08, 0x02, 0x82, 0x20, 0x84, 0x88, 0x00,
                              0x80, 0x54, 0x57, 0x09, 0x00, 0x80, 0x90, 0x4F, 0x08, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x11, 0x00,
                              0x80, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8C, 0x01, 0x10, 0x22, 0x22, 0x42, 0x00, 0x2E, 0xE6,
                              0x3A, 0xA3, 0x03, 0xC4, 0x10, 0x42, 0x88, 0x03, 0x2E, 0x42, 0x44, 0xC4, 0x07, 0x2E, 0x42, 0x06,
                              0xA3, 0x03, 0x88, 0xA9, 0xF4, 0x11, 0x02, 0x3F, 0x3C, 0x08, 0xA3, 0x03, 0x2E, 0x86, 0x17, 0xA3,
                              0x03, 0x1F, 0x22, 0x44, 0x08, 0x01, 0x2E, 0x46, 0x17, 0xA3, 0x03, 0x2E, 0x46, 0x0F, 0xA3, 0x03,
                              0xC0, 0x18, 0x60, 0x0C, 0x00, 0xC0, 0x18, 0x60, 0x88, 0x00, 0x88, 0x88, 0x20, 0x08, 0x02, 0x00,
                              0x7C, 0xF0, 0x01, 0x00, 0x41, 0x10, 0x44, 0x44, 0x00, 0x2E, 0x42, 0x44, 0x00, 0x01, 0x2E, 0x42,
                              0x5B, 0xAB, 0x03, 0x44, 0xA9, 0xF8, 0x63, 0x04, 0x2F, 0xC6, 0x17, 0xE3, 0x03, 0x2E, 0x86, 0x10,
                              0xA2, 0x03, 0x27, 0xC5, 0x18, 0xD3, 0x01, 0x3F, 0x84, 0x13, 0xC2, 0x07, 0x3F, 0x84, 0x13, 0x42,
                              0x00, 0x2E, 0x86, 0x1E, 0xA3, 0x03, 0x31, 0xC6, 0x1F, 0x63, 0x04, 0x8E, 0x10, 0x42, 0x88, 0x03,
                              0x1C, 0x21, 0x84, 0x92, 0x01, 0x31, 0x95, 0x51, 0x52, 0x04, 0x21, 0x84, 0x10, 0xC2, 0x07, 0x71,
                              0xD7, 0x18, 0x63, 0x04, 0x31, 0xE6, 0x3A, 0x63, 0x04, 0x2E, 0xC6, 0x18, 0xA3, 0x03, 0x2F, 0xC6,
                              0x17, 0x42, 0x00, 0x2E, 0xC6, 0x58, 0xB3, 0x07, 0x2F, 0xC6, 0x97, 0x62, 0x04, 0x2E, 0x06, 0x07,
                              0xA3, 0x03, 0x9F, 0x10, 0x42, 0x88, 0x03, 0x31, 0xC6, 0x18, 0xA3, 0x03, 0x31, 0xC6, 0xA8, 0x14,
                              0x01, 0x31, 0xC6, 0x58, 0xAB, 0x02, 0x31, 0x2A, 0xA2, 0x62, 0x04, 0x31, 0x46, 0x45, 0x08, 0x01,
                              0x1F, 0x22, 0x22, 0xC2, 0x07, 0x4E, 0x08, 0x21, 0x84, 0x03, 0x21, 0x08, 0x82, 0x20, 0x04, 0x0E,
                              0x21, 0x84, 0x90, 0x03, 0x44, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x07, 0x82, 0x20,
                              0x00, 0x00, 0x00, 0x00, 0x38, 0xE8, 0xA3, 0x07, 0x21, 0x84, 0x36, 0xE3, 0x03, 0x00, 0xF8, 0x10,
                              0xA2, 0x03, 0x10, 0x42, 0x9B, 0xA3, 0x07, 0x00, 0xB8, 0xF8, 0x83, 0x07, 0x44, 0x09, 0x71, 0x84,
                              0x00, 0x00, 0xF8, 0xE8, 0xA1, 0x03, 0x21, 0xB4, 0x19, 0x63, 0x04, 0x04, 0x18, 0x42, 0x88, 0x03,
                              0x08, 0x30, 0x84, 0x92, 0x01, 0x21, 0xA4, 0x32, 0x4A, 0x02, 0x86, 0x10, 0x42, 0x88, 0x03, 0x00,
                              0xAC, 0x5A, 0x6B, 0x05, 0x00, 0xBC, 0x18, 0x63, 0x04, 0x00, 0xB8, 0x18, 0xA3, 0x03, 0x00, 0xBC,
                              0xF8, 0x42, 0x00, 0x00, 0xF8, 0xE8, 0x21, 0x04, 0x00, 0xB4, 0x19, 0x42, 0x00, 0x00, 0xB8, 0xE0,
                              0xA0, 0x03, 0x84, 0x38, 0x42, 0x08, 0x02, 0x00, 0xC4, 0x18, 0xB3, 0x05, 0x00, 0xC4, 0x18, 0x15,
                              0x01, 0x00, 0xC4, 0x58, 0xAB, 0x02, 0x00, 0x44, 0x45, 0x54, 0x04, 0x00, 0xC4, 0xE8, 0xA0, 0x03,
                              0x00, 0x7C, 0x44, 0xC4, 0x07, 0x10, 0x21, 0x86, 0x10, 0x04, 0x84, 0x10, 0x42, 0x08, 0x21, 0x41,
                              0x08, 0x23, 0x44, 0x00, 0xA2, 0x22, 0x00, 0x00, 0x00, 0x40, 0x01, 0x12, 0x1D, 0x00, 0x2E, 0x86,
                              0x50, 0x1D, 0x01, 0x0A, 0xC4, 0x18, 0xB3, 0x05, 0x0C, 0xB8, 0xF8, 0x83, 0x07, 0x2E, 0x3A, 0xE8,
                              0xA3, 0x07, 0x0A, 0x38, 0xE8, 0xA3, 0x07, 0x06, 0x38, 0xE8, 0xA3, 0x07, 0x04, 0x38, 0xE8, 0xA3,
                              0x07, 0x00, 0xF8, 0x50, 0x3C, 0x01, 0x2E, 0xBA, 0xF8, 0x83, 0x07, 0x0A, 0xB8, 0xF8, 0x83, 0x07,
                              0x06, 0xB8, 0xF8, 0x83, 0x07, 0x0A, 0x18, 0x42, 0x88, 0x03, 0x26, 0x19, 0x42, 0x88, 0x03, 0x06,
                              0x18, 0x42, 0x88, 0x03, 0x0A, 0xB8, 0x18, 0x7F, 0x04, 0x04, 0xB8, 0x18, 0x7F, 0x04, 0x0C, 0xFC,
                              0x70, 0xC2, 0x07, 0x00, 0x3C, 0xEA, 0x8A, 0x07, 0x00, 0xF8, 0xF2, 0x4A, 0x07, 0x2E, 0xBA, 0x18,
                              0xA3, 0x03, 0x0A, 0xB8, 0x18, 0xA3, 0x03, 0x06, 0xB8, 0x18, 0xA3, 0x03, 0x44, 0xC5, 0x18, 0xB3,
                              0x05, 0x06, 0xC4, 0x18, 0xB3, 0x05, 0x0A, 0xC4, 0xE8, 0xA1, 0x03, 0x0A, 0xB8, 0x18, 0xA3, 0x03,
                              0x0A, 0xC4, 0x18, 0xA3, 0x03, 0xC4, 0x97, 0x52, 0x3C, 0x01, 0x4C, 0x8A, 0x27, 0xC4, 0x07, 0x51,
                              0x91, 0x4F, 0x3E, 0x01, 0xA3, 0x94, 0xD5, 0x53, 0x06, 0x98, 0x10, 0x47, 0xC8, 0x00, 0x0C, 0x38,
                              0xE8, 0xA3, 0x07, 0x0C, 0x18, 0x42, 0x88, 0x03, 0x0C, 0xB8, 0x18, 0xA3, 0x03, 0x0C, 0xC4, 0x18,
                              0xB3, 0x05, 0x0E, 0xB4, 0x19, 0x63, 0x04, 0x0E, 0xC4, 0x59, 0x73, 0x04, 0xC0, 0x41, 0x1F, 0xFD,
                              0x07, 0x00, 0xB8, 0x18, 0xDD, 0x07, 0x04, 0x10, 0x22, 0xA2, 0x03, 0x00, 0x00, 0xE0, 0x05, 0x00,
                              0x00, 0x00, 0xE0, 0x21, 0x00, 0x31, 0x26, 0x2E, 0x53, 0x06, 0x31, 0x26, 0xA2, 0x72, 0x04, 0x04,
                              0x10, 0x42, 0x08, 0x01, 0x00, 0xC8, 0x24, 0x01, 0x00, 0x00, 0x24, 0x99, 0x00, 0x00, 0xA0, 0x82,
                              0x0A, 0x2A, 0xA8, 0x32, 0x11, 0x99, 0x88, 0x4C, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0x84, 0x10, 0x42,
                              0x08, 0x21, 0x84, 0x90, 0x43, 0x08, 0x21, 0x84, 0x1C, 0x72, 0x08, 0x21, 0x4A, 0xA9, 0xA5, 0x94,
                              0x52, 0x00, 0x80, 0xA7, 0x94, 0x52, 0x00, 0x1C, 0x72, 0x08, 0x21, 0x4A, 0x2D, 0xB4, 0x94, 0x52,
                              0x4A, 0x29, 0xA5, 0x94, 0x52, 0x00, 0x3C, 0xB4, 0x94, 0x52, 0x4A, 0x2D, 0xF4, 0x00, 0x00, 0x4A,
                              0xA9, 0x07, 0x00, 0x00, 0x84, 0x1C, 0x72, 0x00, 0x00, 0x00, 0x80, 0x43, 0x08, 0x21, 0x84, 0x10,
                              0x0E, 0x00, 0x00, 0x84, 0x90, 0x0F, 0x00, 0x00, 0x00, 0x80, 0x4F, 0x08, 0x21, 0x84, 0x10, 0x4E,
                              0x08, 0x21, 0x00, 0x80, 0x0F, 0x00, 0x00, 0x84, 0x90, 0x4F, 0x08, 0x21, 0x84, 0x70, 0xC2, 0x09,
                              0x21, 0x4A, 0x29, 0xAD, 0x94, 0x52, 0x4A, 0x69, 0xE1, 0x01, 0x00, 0x00, 0x78, 0xA1, 0x95, 0x52,
                              0x4A, 0x6D, 0xF0, 0x01, 0x00, 0x00, 0x7C, 0xB0, 0x95, 0x52, 0x4A, 0x69, 0xA1, 0x95, 0x52, 0x00,
                              0x7C, 0xF0, 0x01, 0x00, 0x4A, 0x6D, 0xB0, 0x95, 0x52, 0x84, 0x7C, 0xF0, 0x01, 0x00, 0x4A, 0xA9,
                              0x0F, 0x00, 0x00, 0x00, 0x7C, 0xF0, 0x09, 0x21, 0x00, 0x80, 0xAF, 0x94, 0x52, 0x4A, 0x29, 0x0F,
                              0x00, 0x00, 0x84, 0x70, 0xC2, 0x01, 0x00, 0x00, 0x70, 0xC2, 0x09, 0x21, 0x00, 0x00, 0xAF, 0x94,
                              0x52, 0x4A, 0xA9, 0xAD, 0x94, 0x52, 0x84, 0x7C, 0xF0, 0x09, 0x21, 0x08, 0x21, 0x07, 0x00, 0x00,
                              0x00, 0x00, 0x4E, 0x08, 0x21, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xF0, 0xFF, 0xFF, 0xE7,
                              0x9C, 0x73, 0xCE, 0x39, 0x9C, 0x73, 0xCE, 0x39, 0xE7, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x40, 0xB6,
                              0xD2, 0x24, 0x00, 0x2E, 0xBE, 0x18, 0x5F, 0x00, 0x6D, 0x86, 0x10, 0x42, 0x00, 0xE0, 0x2B, 0xA5,
                              0x94, 0x04, 0x3F, 0x0A, 0x22, 0xE2, 0x07, 0xC0, 0xB7, 0x18, 0xA3, 0x03, 0x20, 0xC6, 0x38, 0x5B,
                              0x00, 0x20, 0x1B, 0x21, 0x84, 0x00, 0x8E, 0xB8, 0xE8, 0x88, 0x03, 0x4C, 0x4A, 0x26, 0x25, 0x03,
                              0x2E, 0xC6, 0xA8, 0xD4, 0x06, 0x3E, 0x18, 0x15, 0xA3, 0x03, 0x00, 0xA8, 0x5A, 0x15, 0x00, 0xC4,
                              0xD5, 0xEA, 0x08, 0x01, 0x2E, 0x84, 0x17, 0x82, 0x03, 0x2E, 0xC6, 0x18, 0x63, 0x04, 0x1F, 0x80,
                              0x0F, 0xC0, 0x07, 0xC4, 0x11, 0x00, 0x1C, 0x00, 0xC1, 0x60, 0x13, 0xC0, 0x07, 0x90, 0x0D, 0x06,
                              0xC1, 0x07, 0x88, 0x12, 0x42, 0x08, 0x21, 0x84, 0x10, 0x42, 0x8A, 0x00, 0x80, 0x80, 0x0F, 0x08,
                              0x00, 0x40, 0x54, 0x24, 0x2A, 0x00, 0x00, 0x30, 0x29, 0x19, 0x00, 0x00, 0x30, 0xEF, 0x19, 0x00,
                              0x00, 0x00, 0xC6, 0x00, 0x00, 0x9C, 0x10, 0x52, 0x0C, 0x01, 0x46, 0x29, 0x05, 0x00, 0x00, 0x06,
                              0x11, 0x07, 0x00, 0x00, 0xCE, 0x39, 0x07, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00};
#endif








    void SetSinglePixelInMemory( uint32 X_poz, uint32 Y_poz, int32 color );
    int32 GetSinglePixelFromMemory( uint32 X_poz, uint32 Y_poz );
    void PutFillRect( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2, int32 color );
    void MakeLine( int32 X1, int32 Y1, int32 X2, int32 Y2, bool window );
    void MakeRect( int32 X1, int32 Y1, int32 X2, int32 Y2 );



    static inline bool check_need_windowed( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2 )
    {
        if (  ((X1 <= X2) && ( (X1 < G_var.draw_window.X1) || (X2 > G_var.draw_window.X2) )) ||
              ((X1 > X2) && ( (X2 < G_var.draw_window.X1) || (X2 < G_var.draw_window.X2)  )) ||
              ((Y1 <= Y2) && ( (Y1 < G_var.draw_window.Y1) || (Y2 > G_var.draw_window.Y2) )) ||
              ((Y1 > Y2) && ( (Y2 < G_var.draw_window.Y1) || (Y2 < G_var.draw_window.Y2) ))     )
            return true;
        return false;
    }


    #define MAX_VERTICAL_PIXEL_PACK         (GDISP_MAX_MEM_H + 2)

    static inline g_result internal_bitmap_draw_1bp( const uint8 *buffer, uint32 size )
    {
        uint8 b;
        uint8 g;
        uint16 i;
        uint8 k;
        bool neg = false;
        uint16 address;

        if (  G_var.bmp_draw.X_dim == 0 )
            return GRESULT_NOT_PERMITTED;

        for ( i=0; i<size; i++ )
        {
            for (k=0; k<8; k++)
            {
                if ( G_var.bmp_draw.go_simple ||
                    ((G_var.bmp_draw.X_crt >= G_var.draw_window.X1) &&
                     (G_var.bmp_draw.X_crt <= G_var.draw_window.X2) &&
                     (G_var.bmp_draw.Y_crt >= G_var.draw_window.Y1) &&
                     (G_var.bmp_draw.Y_crt <= G_var.draw_window.Y2)) )
                {
                    address = GDISP_WIDTH * (G_var.bmp_draw.Y_crt >> 3) + G_var.bmp_draw.X_crt;
                    g = (1 << (G_var.bmp_draw.Y_crt & 0x07) );
                }
                else
                {
                    address = 0xFFFF;
                }

                if ( address != 0xFFFF )
                {
                    b = (1<<k);
                    if ( ((neg == false) && (buffer[i] & b)) || ((neg == true) && !(buffer[i] & b)) )   // black pixel
                        g_mem[ address ] |= g;
                    else                                // white pixel
                        g_mem[ address ] &= ~g;
                }

                G_var.bmp_draw.Y_crt++;
                if ( G_var.bmp_draw.Y_crt == (G_var.bmp_draw.Y_poz + G_var.bmp_draw.Y_dim) )
                {
                    k = 8;  // skip to the next byte
                    G_var.bmp_draw.Y_crt = G_var.bmp_draw.Y_poz;
                    G_var.bmp_draw.X_crt++;
                    if ( G_var.bmp_draw.X_crt == (G_var.bmp_draw.X_poz + G_var.bmp_draw.X_dim) )
                        goto _end_graphics;
                }

            }
        }

        return 0;
    _end_graphics:
        G_var.bmp_draw.X_dim = 0;
        return 0;
    }//END: internal_bitmap_draw_1bp


    static g_result internal_rectangle_draw_1bp( uint32 x1, uint32 y1, uint32 x2, uint32 y2, int color, bool fill )
    {
        register uint32 x, y;
        uint32 h1;      // height in bytes - beginning
        uint32 h2;      // height in bytes - ending
        uint32 p1;      // position in byte - beginning
        uint32 p2;      // position in byte - ending
        bool monobyte = false;  // if rectangle fits in one byte
        uint8 b1;       // bitmask for beginning
        uint8 b2;       // bitmask for ending
        uint8 l1;       // bitmask for vertical/fill beginning
        uint8 l2;       // bitmask for vertical/fill ending
        register uint8 b;

        // calculate byte parameters
        h1 = (y1 >> 3);     // height in 8 pack
        h2 = (y2 >> 3);
        p1 = y1 & 0x07;     // bit position
        p2 = y2 & 0x07;

        b1 = (1 << p1);         // bit for upper margin

        l1  = ~( (uint8)((uint8)(1 << p1) - 1) );       //  1<<3:  00001000->  -1:  00000111->  ~: 11111000  ||  1<<0: 00000001-> -1: 00000000-> ~: 11111111  ||  1<<7: 10000000-> -1: 01111111-> ~: 10000000
        l2  = (uint8)((uint8)(1 << p2) - 1) | (1<<p2);  //  1<<3:  00001000->  -1:  00000111->  |: 00001111  ||  1<<0: 00000001-> -1: 00000000-> |: 00000001  ||  1<<7: 10000000-> -1: 01111111-> |: 11111111
        if (h1 == h2)
        {
            monobyte = true;
            b1 |= (1<<p2);
            l1 &= l2;       //    11111100 & 00111111 = 00111100
        }
        else
        {
            b2 = (1<<p2);       // bit for lower margin
        }

        // scan on X axis
        for ( x = x1; x <= x2; x++ )
        {
            for ( y = h1; y<=h2; y++ )
            {
                if ( monobyte ) // drawing fits in a single byte in height
                {
                    if ( (x == x1) || (x == x2) || fill )   // vertical lines or fill
                        b = l1;
                    else                                    // outline
                        b = b1;
                }
                else            // drawing uses more bytes in height
                {
                    if ( (x == x1) || (x == x2) || fill )   // vertical lines or fill
                    {
                        if ( y == h1 )
                            b   = l1;
                        else if ( y == h2 )
                            b   = l2;
                        else
                            b   = 0xff;
                    }
                    else                                    // outline
                    {
                        if ( y == h1 )
                            b   = b1;
                        else if ( y == h2 )
                            b   = b2;
                        else
                            b   = 0x00;
                    }
                }

                switch ( color )
                {
                    case 0:     // black
                        g_mem[ y * GDISP_WIDTH + x ] &= ~b;
                        break;
                    case 1:     // white
                        g_mem[ y * GDISP_WIDTH + x ] |= b;
                        break;
                    case -1:    // invert
                        g_mem[ y * GDISP_WIDTH + x ] ^= b;
                        break;
                }

            }//end: for y
        }//end: for x
        return 0;
    }//END: internal_rectangle_draw_1bp





    g_result Graphics_Init( uint8 *memory, LUTelem *LUT )
    {
    #ifndef CONF_GRAPHIC_INT_MEM
        if ( (memory == NULL) || 
             ( ( (GDISP_PIXEL_FORMAT == gpixformat_4bitLUT) ||
                 (GDISP_PIXEL_FORMAT == gpixformat_8bitLUT)   ) &&
               (LUT == NULL) ) )
        {
            return GRESULT_PARAM_ERROR;
        }
        g_mem = memory;
    #endif

        memset( &G_var, 0, sizeof(struct S_GraphicLibVariables) );

        G_var.LUT   = LUT;

        G_var.draw_window.X1    = 0;
        G_var.draw_window.X2    = GDISP_WIDTH - 1;
        G_var.draw_window.Y1    = 0;
        G_var.draw_window.Y2    = GDISP_HEIGHT - 1;

        G_var.line_width        = 1;

        memset( g_mem, 0, GDISP_MAX_MEMORY );

    #ifdef CONF_GRAPHIC_INT_SSFONT
        G_var.text.pFont = SystemFontData;
    #else
        G_var.text.pFont = NULL;
    #endif

        if ( DispHAL_Init( g_mem ) == 0 )
        {
            if ( LUT )
                DispHAL_UpdateLUT( LUT );
            return GRESULT_OK;
        }
        else
            return GRESULT_FATAL_ERROR;

    }//END: Graphics_Init



    g_result Graphics_UpdateLUT( LUTelem *LUT )
    {
        if ( LUT == NULL )
            return GRESULT_NOT_PERMITTED;

        G_var.LUT   = LUT;
        DispHAL_UpdateLUT( LUT );
        return GRESULT_OK;
    }


    void Graphics_ClearScreen( uint32 color )
    {
    #if (GDISP_PIXEL_FORMAT == gpixformat_4bitLUT)
        {
            uint32 msize = GDISP_MAX_MEMORY;
            uint8 clr    = (uint8)( (color & 0x0F) | ( (color & 0x0F<<4)) );
            memset( g_mem, clr, msize );
        }
    #elif (GDISP_PIXEL_FORMAT == gpixformat_1bit)
        if ( color == 0 )
            memset(g_mem, 0, GDISP_MAX_MEMORY );
        else
            memset(g_mem, 0xff, GDISP_MAX_MEMORY );
    #elif (GDISP_PIXEL_FORMAT == gpixformat_8bitLUT)
        memset(g_mem, (uint8)color, GDISP_MAX_MEMORY );
    #else
        {
            uint8* G_mem = g_mem;
            uint32 msize = GDISP_MAX_MEMORY / 2;
            while ( msize-- )
            {
                (*(uint16*)G_mem) = (uint16)color;
                 G_mem+=2;
            }
        }
    #endif

        DispHAL_NeedUpdate( 0, 0, GDISP_WIDTH-1, GDISP_HEIGHT-1 );

    }//END: Graphics_ClearScreen



    g_result Graphic_Window( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2 )
    {
        if ( (X1 >= GDISP_WIDTH) || (Y1 >= GDISP_HEIGHT) ||
             (X2 >= GDISP_WIDTH) || (Y2 >= GDISP_HEIGHT) ||
             (X2 <= X1) || (Y2 <= Y1) )
            return GRESULT_PARAM_ERROR;

        G_var.draw_window.X1    = X1;
        G_var.draw_window.X2    = X2;
        G_var.draw_window.Y1    = Y1;
        G_var.draw_window.Y2    = Y2;
        return GRESULT_OK;
    }//END: Graphic_Rectangle


    g_result Graphic_ShiftH( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2, uint32 amount )
    {
        if ( (X1 >= GDISP_WIDTH) || (Y1 >= GDISP_HEIGHT) ||
             (X2 >= GDISP_WIDTH) || (Y2 >= GDISP_HEIGHT) ||
             (amount == 0) || (X2 <= X1) || (Y2 <= Y1) ||
             (amount >= (X2 - X1)) )
            return GRESULT_PARAM_ERROR;
    #if (GDISP_PIXEL_FORMAT == gpixformat_1bit)
        {

            // Primitive implementation - only Y*8 ranges
            // TODO: make for all pixel purpose
            int x;
            int y;
            int final;
            final = X2 - amount;
            for ( x=X1; x<=final; x++ )
            {
                for ( y = (Y1>>3); y <= (Y2>>3); y++ )
                {
                    g_mem[ y * GDISP_MAX_MEM_W + x ] = g_mem[ y * GDISP_MAX_MEM_W + x + amount ]; 
                }
            }
        }
    #else
        return GRESULT_NOT_IMPLEMENTED;
    #endif 

        return GRESULT_OK;
    }


    g_result Graphic_PutPixel( uint32 X_poz, uint32 Y_poz, int32 color )
    {
    #if ( GASSERTION_LEVEL & GASSERTION_CHECK_PARAMS )
        if ( (X_poz>= GDISP_WIDTH) || (Y_poz>= GDISP_HEIGHT) )
            return GRESULT_PARAM_ERROR;
    #endif

        SetSinglePixelInMemory( X_poz, Y_poz, color );

        DispHAL_NeedUpdate( X_poz, Y_poz, X_poz, Y_poz );
        return GRESULT_OK;
    }//END: Graphic_PutPixel



    uint32 Graphic_GetPixel( uint32 X_poz, uint32 Y_poz )
    {
    #if ( GASSERTION_LEVEL & GASSERTION_CHECK_PARAMS )
        if ( (X_poz>= GDISP_WIDTH) || (Y_poz>= GDISP_HEIGHT) )
            return GRESULT_PARAM_ERROR;
    #endif

        return GetSinglePixelFromMemory( X_poz, Y_poz );
    }//END: Graphic_GetPixel


    g_result Graphic_SetWidth( uint32 width )
    {
        if ( width >= 10) 
            return GRESULT_PARAM_ERROR;

        G_var.line_width    = width;
        return GRESULT_OK;
    }//END: Graphic_SetWidth


    g_result Graphic_SetColor( int32 color )
    {
        G_var.color         = color;
        return GRESULT_OK;
    }//END: Graphic_SetWidth


    g_result Graphic_Line( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2 )
    {
        bool windowed = false;
    #if ( GASSERTION_LEVEL & GASSERTION_CHECK_PARAMS )
        if (G_var.line_width == 0)
            return GRESULT_PARAM_ERROR;
    #endif

        windowed = check_need_windowed( X1, Y1, X2, Y2 );

        MakeLine( X1, Y1, X2, Y2, windowed );
        
        DispHAL_NeedUpdate( X1, Y1, X2, Y2 );

        return GRESULT_OK;
    }//END: Graphic_Line


    g_result Graphic_AALine( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2 )
    {
    #if ( GASSERTION_LEVEL & GASSERTION_CHECK_PARAMS )
        if ( (G_var.line_width == 0) || (GDISP_PIXEL_FORMAT != gpixformat_16bitARGB) )
            return GRESULT_PARAM_ERROR;
    #endif

        // TODO: implementation for anti-aliassed line

        return GRESULT_NOT_IMPLEMENTED;
    }//END: Graphic_AALine




    // draws a rectangle
    g_result Graphic_Rectangle( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2 )
    {
    #if ( GASSERTION_LEVEL & GASSERTION_CHECK_PARAMS )
        if (G_var.line_width == 0)
            return GRESULT_PARAM_ERROR;
        if ( (X1>= GDISP_WIDTH) || (Y1>= GDISP_HEIGHT) || (X2>= GDISP_WIDTH) || (Y2>= GDISP_HEIGHT) )
            return GRESULT_PARAM_ERROR;
        if ( (X1 > X2) || (Y1 > Y2) )
            return GRESULT_PARAM_ERROR;
    #endif

#if (GDISP_PIXEL_FORMAT == gpixformat_1bit)
        // optimized routine for 1 bit displays
        internal_rectangle_draw_1bp( X1, Y1, X2, Y2, G_var.color, false );
#else
        MakeRect( X1, Y1, X2, Y2 );
#endif
        DispHAL_NeedUpdate( X1, Y1, X2, Y2 );

        return GRESULT_OK;
    }//END: Graphic_Rectangle


    // draws a filled rectangle
    g_result Graphic_FillRectangle( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2, int32 fillColor )
    {
    #if ( GASSERTION_LEVEL & GASSERTION_CHECK_PARAMS )
        if ( (X1>= GDISP_WIDTH) || (Y1>= GDISP_HEIGHT) || (X2>= GDISP_WIDTH) || (Y2>= GDISP_HEIGHT) )
            return GRESULT_PARAM_ERROR;
        if ( (X1 > X2) || (Y1 > Y2) )
            return GRESULT_PARAM_ERROR;
    #endif

#if (GDISP_PIXEL_FORMAT == gpixformat_1bit)
        // optimized routine for 1 bit displays
        internal_rectangle_draw_1bp( X1, Y1, X2, Y2, fillColor, true );
        if ( (G_var.line_width) && (fillColor != G_var.color) )
            internal_rectangle_draw_1bp( X1, Y1, X2, Y2, G_var.color, false );
#else
        if ( G_var.line_width )
        {
            MakeRect( X1, Y1, X2, Y2 );
            if ( ((X2 - X1) > 1) && ((Y2-Y1) > 1 ) )
            {
                PutFillRect(X1+1, Y1+1, X2-1, Y2-1, fillColor );
            }
        }
        else
        {
            PutFillRect(X1, Y1, X2, Y2, fillColor );
        }
#endif

        DispHAL_NeedUpdate( X1, Y1, X2, Y2 );

        return GRESULT_OK;
    }//END: Graphic_FillRectangle




    #define FIXPOINT    100000

    static int64 i_pow( int32 val, uint32 power )
    {
        int64 ival = FIXPOINT;
        if ( power == 0 )
            return  FIXPOINT;
        do 
        {
            ival = (ival * val) / FIXPOINT;
            power--;
        } while (power);
        return ival;
    }


    g_result Graphic_DrawSpline( int32 *Xarray, int32 *Yarray )
    {
        // cubic Spline implementation on fast integer
        int32 Xstart = Xarray[0];
        int32 Ystart = Yarray[0];

        int32 xl1 = Xarray[0];
        int32 yl1 = Yarray[0];
        int32 xl2 = 0;
        int32 yl2 = 0;

        uint32 t;
        
        for (t = 0; t < FIXPOINT; t += (FIXPOINT / 40) )
        {
            int32 xt;
            int32 yt;
            
            int64 elem1;
            int64 elem2;
            int64 elem3;
            int64 elem4;
            
            elem1 = i_pow (FIXPOINT - t, 3) * Xarray[0];
            elem2 = i_pow (FIXPOINT - t, 2) * 3 * t * Xarray[1];
            elem3 = i_pow (t, 2) * (FIXPOINT - t) * 3 * Xarray[2];
            elem4 = i_pow (t, 3) * Xarray[3];
            
            xt = (int32)((elem1 + elem4 + ( elem2 + elem3 ) / FIXPOINT) / FIXPOINT);
            
            elem1 = i_pow (FIXPOINT - t, 3) * Yarray[0];
            elem2 = i_pow (FIXPOINT - t, 2) * 3 * t * Yarray[1];
            elem3 = i_pow (t, 2) * (FIXPOINT - t) * 3 * Yarray[2];
            elem4 = i_pow (t, 3) * Yarray[3];
             
            yt = (int32)((elem1 + elem4 + ( elem2 + elem3 ) / FIXPOINT) / FIXPOINT);

            
            // line drawing
            if ( (Xstart != xt) || (Ystart != yt) ) 
            {
                MakeLine( Xstart, Ystart, xt, yt, true );  

                if ( xl2 < xt )
                    xl2 = xt;
                if ( yl2 < yt )
                    yl2 = yt;
                if ( xl1 > xt )
                    xl1 = xt;
                if ( yl1 > yt )
                    yl1 = yt;
            }
            
            Xstart = xt;
            Ystart = yt;
        }

        if ( xl1 < 0 )
            xl1 = 0;
        if ( xl2 < 0 )
            xl2 = 0;
        if ( yl1 < 0 )
            yl1 = 0;
        if ( yl2 < 0 )
            yl2 = 0;

        DispHAL_NeedUpdate( xl1, yl1, xl2, yl2 );

        return GRESULT_OK;
    }
    


    //////////////////////////////////////////////////////////////////
    //
    // ------------ Draw bitmaps ---------------
    //
    //////////////////////////////////////////////////////////////////


    g_result Bitmap_StartDrawing( int32 X_poz, int32 Y_poz, uint32 X_dim, uint32 Y_dim, bool upside_down, uint32 input_format )
    {
        memset( &G_var.bmp_draw, 0, sizeof( G_var.bmp_draw ) );
        G_var.bmp_draw.X_poz = X_poz;
        G_var.bmp_draw.Y_poz = Y_poz;
        G_var.bmp_draw.X_dim = X_dim;
        G_var.bmp_draw.Y_dim = Y_dim;
        G_var.bmp_draw.X_crt = X_poz;
        G_var.bmp_draw.Y_crt = Y_poz;
        G_var.bmp_draw.upside_down = upside_down;
        G_var.bmp_draw.pixel_format = input_format;

        if ( ( input_format == GDISP_PIXEL_FORMAT ) &&
             ( upside_down == false ) &&
             ( X_poz >= G_var.draw_window.X1 ) && ( Y_poz >= G_var.draw_window.Y1 ) &&
             ( (X_poz + X_dim) <= G_var.draw_window.X2 ) &&
             ( (Y_poz + Y_dim) <= G_var.draw_window.Y2 ) )
        {
            G_var.bmp_draw.go_simple = true;
        }



        return GRESULT_OK;
    }

    g_result Bitmap_DrawStream( const uint8 *data, uint32 size )
    {

#if (GDISP_PIXEL_FORMAT == gpixformat_1bit)
        // for 1bit format it only accepts 1bit bitmaps with Y order scanning for efficiency
        return internal_bitmap_draw_1bp( data, size );
#else
        return GRESULT_OK;
#endif
    }

    g_result Bitmap_StopDrawing( void )
    {
        DispHAL_NeedUpdate( G_var.bmp_draw.X_poz,
                            G_var.bmp_draw.Y_poz,
                            G_var.bmp_draw.X_poz + G_var.bmp_draw.X_dim,
                            G_var.bmp_draw.Y_poz + G_var.bmp_draw.Y_dim );
        G_var.bmp_draw.X_dim = 0;
        return GRESULT_OK;
    }











    //////////////////////////////////////////////////////////////////
    //
    // ------------ Draw text with different font type ---------------
    //
    //////////////////////////////////////////////////////////////////

    static bool CheckCHRinFont( uint8 chr )
    {
        if ( chr < '!' )
            return false;

        if ( (G_var.text.pFont[0] & 0xC0) == 0 )        // full ASCII
            return true;
        // else consider half ASCII
        if ( chr > '~' )
            return false;

        if ( ((G_var.text.pFont[0] & 0xC0) == 2 ) &&    // upper case only
             ((chr >= 'a') && (chr < '{' )) )
            return false;
        if ( ((G_var.text.pFont[0] & 0xC0) == 3 ) &&    // numeric only
             (chr > ':') )
            return false;

        return true;
    }
    


    g_result Gtext_SetFontAndColors( const uint8 *font, bool monospace, int32 color, int32 background )
    {
        if ( font == 0 )
        #ifdef CONF_GRAPHIC_INT_SSFONT
            G_var.text.pFont    = SystemFontData;
        #else
            return GRESULT_PARAM_ERROR;
        #endif
        else
            G_var.text.pFont    = font;

        G_var.text.monospace = 0;

        if (monospace)
        {
            G_var.text.monospace = Gtext_GetCharacterWidth('&');
        }

#if (GDISP_PIXEL_FORMAT == gpixformat_1bit)
        if ( (G_var.text.pFont[0] == 0)      ||
             ((color != 0) && (color != 1 )) ||
             ((background != 0) && (background != 1) && (background != -1)) )
            return GRESULT_PARAM_ERROR;
#elif (GDISP_PIXEL_FORMAT == gpixformat_4bitLUT)
        if ( (G_var.text.pFont[0] == 0)     ||
             ((color < 0) || (color > 15 )) ||
             ((background < -1) || (background > 15)) )
            return GRESULT_PARAM_ERROR;
#elif (GDISP_PIXEL_FORMAT == gpixformat_8bitLUT)
        if ( (G_var.text.pFont[0] == 0)     ||
             ((color < 0) || (color > 255 )) ||
             ((background < -1) || (background > 15)) )
            return GRESULT_PARAM_ERROR;
#elif (GDISP_PIXEL_FORMAT == gpixformat_16bitARGB)
        if ( (G_var.text.pFont[0] == 0)     ||
             ((color < 0) || (color > 0x7fff )) ||
             ((background < -1) || (background > 0x7fff)) )
            return GRESULT_PARAM_ERROR;
#endif

        G_var.text.color = color;
        G_var.text.bkgnd = background;

        return GRESULT_OK;
    }//END: Gtext_SetFontAndColors



    g_result Gtext_SetCoordinates( int32 X_poz, int32 Y_poz )
    {
        if ( (X_poz < 0) || (Y_poz < 0) || 
             (Y_poz >= GDISP_HEIGHT) ||
             (X_poz >= GDISP_WIDTH) )
            return GRESULT_PARAM_ERROR;

        G_var.text.Xpoz = X_poz;
        G_var.text.Ypoz = Y_poz;
        return GRESULT_OK;
    }//END: Gtext_SetCoordinates


    g_result Gtext_PutChar( uint8 chr )
    {
        uint32 next_Xpoz;
        uint32 width;
        uint32 height;
        bool printable = true;

        height = G_var.text.pFont[1];

        // check if character is in the font file and get it's width
        if ( CheckCHRinFont(chr) == false )     // if character is not in font file or it is 'space' then advance X pointer 
        {
            width = Gtext_GetCharacterWidth(' ');
            printable = false;
        }
        else
        {
            if ( G_var.text.monospace ) // trick the interface routine to give the real character width
            {
                uint32 temp = G_var.text.monospace;
                G_var.text.monospace = 0;
                width = Gtext_GetCharacterWidth(chr);
                G_var.text.monospace = temp;
            }
            else
                width = Gtext_GetCharacterWidth(chr);
        }

        if ( G_var.text.monospace )
            next_Xpoz = G_var.text.monospace + G_var.text.Xpoz;
        else
            next_Xpoz = width + G_var.text.Xpoz;

        // draw the background rectangle
        if ( next_Xpoz >= GDISP_WIDTH )
            next_Xpoz = GDISP_WIDTH -1;

        if ( G_var.text.bkgnd != -1 )       // if background is needed - fill the zone
        {
            PutFillRect( G_var.text.Xpoz, G_var.text.Ypoz, next_Xpoz, G_var.text.Ypoz + height - 1, G_var.text.bkgnd );
        }

        // draw the font pixels
        if ( printable )
        {
            const uint8 *fchar;
            int offs = 3;
            int c_size;

            if ( (G_var.text.pFont[0] & 0x1f) == 0 )
            {
                c_size = G_var.text.pFont[3];
                offs = 4;
            }
            else
                c_size = ( G_var.text.pFont[0] & 0x1f );

            if ( ((G_var.text.pFont[0] & 0x20) != 2) || ( chr < 'a' ) )
            {
                fchar = G_var.text.pFont + ( chr - '!' ) * c_size + offs;          // character nr * character data + header
            }
            else    // case for upper case only and punctuation from the end
            {
                fchar = G_var.text.pFont + ( chr - ('{' - 'a') ) * c_size + offs;
            }

            if ( (G_var.text.pFont[0] & 0x20) == 0 )    // not a fixed size, skip the width header
            {
                fchar++;
            }

            // we have the data pointer for the current character, begin to draw it
            {
                uint32 x;
                uint32 y;
                uint8 data  = *fchar;
                register uint32 Ypoz = G_var.text.Ypoz;
                uint32 datacnt = 0;

                register uint32 xmin = G_var.draw_window.X1;
                register uint32 xmax = G_var.draw_window.X2;
                register uint32 ymin = G_var.draw_window.Y1;
                register uint32 ymax = G_var.draw_window.Y2;
                register uint32 color = (uint32)G_var.text.color;

                fchar++;
                for ( y = 0; y<height; y++ )
                {
                    for ( x = 0; x<width; x++ )
                    {
                        if (( data & 0x01 ) && ((G_var.text.Xpoz + x) < GDISP_WIDTH) )
                        {
                            register uint32 xpoz;
                            // TODO - optimize for 4bit LUT - 
                            // ideea: - set up memory start address, and only increment on X run
                            xpoz = (G_var.text.Xpoz + x);
                            if ( (xpoz <= xmax) && (xpoz >= xmin) && (Ypoz <= ymax) && (Ypoz >= ymin)  )
                                SetSinglePixelInMemory( xpoz, Ypoz, color );
                        }
                        data = data >> 1;
                        datacnt++;
                        if (datacnt == 8)
                        {
                            data    = *fchar;
                            fchar++;
                            datacnt = 0;
                        }
                    }
                    Ypoz++;
                }
            }
        }

        DispHAL_NeedUpdate( G_var.text.Xpoz, G_var.text.Ypoz, next_Xpoz, G_var.text.Ypoz + height);

        // advance the X pointer
        G_var.text.Xpoz = next_Xpoz + 1;
        if ( G_var.text.Xpoz >= GDISP_WIDTH )
        {
            return 1;
        }

        return GRESULT_OK;
    }//END: Gtext_PutChar


    g_result Gtext_PutText( const char *text )
    {
        int32 res;
        do
        {
            res = Gtext_PutChar( *text );
            if ( res != 0 )
            {
                if ( res == 1 )
                    return GRESULT_OK;
                else
                    return res;
            }
            text++;
        } while ( *text != 0x00 );

        return GRESULT_OK;
    }//END: Gtext_PutText


    g_result Gtext_GetCharacterWidth( uint8 chr )
    {
        uint32 c_size;
        int offset = 3;

        if ( G_var.text.monospace )
            return G_var.text.monospace;

        if ( G_var.text.pFont[0] & 0x20 )   // fixed size font
            return G_var.text.pFont[2];

        if ( (G_var.text.pFont[0] & 0x1f) == 0 )    // if large characters
        {
            c_size = G_var.text.pFont[3];
            offset++;
        }
        else                                        // for small characters size is in header
            c_size = 0x1f & G_var.text.pFont[0];

        if ( chr == ' ' )
            return G_var.text.pFont[offset];     // for space return the width of the first character from the font

        if ( CheckCHRinFont( chr ) )
            return G_var.text.pFont[ (chr - '!') * c_size + offset ];
        else
            return GRESULT_PARAM_ERROR;
    }//END: Gtext_GetCharacterWidth


    g_result Gtext_GetCharacterHeight( void )
    {
        return G_var.text.pFont[1];
    }


    /*********************************************************************************
    //
    //
    //  HELPER ROUTINES 
    //
    //
    *********************************************************************************/



    void SetSinglePixelInMemory( uint32 X_poz, uint32 Y_poz, int32 color )
    {
    #if (GDISP_PIXEL_FORMAT == gpixformat_1bit)
        if ( color == GRAPHIC_1BIT_BLACK )
        {   // black
            g_mem[ (Y_poz>>3) * GDISP_MAX_MEM_W + X_poz ] &= ~( 1 << (Y_poz&0x07) );
        }
        else if ( color == GRAPHIC_1BIT_WHITE )
        {   // white
            g_mem[ (Y_poz>>3) * GDISP_MAX_MEM_W + X_poz ] |= ( 1 << (Y_poz&0x07) );
        }
        else
        {   // invert
            g_mem[ (Y_poz>>3) * GDISP_MAX_MEM_W + X_poz ] ^= ( 1 << (Y_poz&0x07) );
        }
    #elif (GDISP_PIXEL_FORMAT == gpixformat_4bitLUT)
        {
            uint32 address = Y_poz * GDISP_MAX_MEM_W + (X_poz>>1);
            uint8 *mempixel = g_mem + address;
            if ( color >= 0 )
            {   // normal color
                if ( X_poz & 0x01 )
                {   // second pixel from the packet
                    *mempixel = (*mempixel & 0xf0) | (uint8)( color & 0x0f );
                }
                else
                {   // first pixel from the packet
                    *mempixel = (*mempixel & 0x0f) | (uint8)( (color << 4) & 0xf0 );
                }
            }
            else
            {   // invert pixel color
                if ( X_poz & 0x01 )
                {   // second pixel from the packet
                    *mempixel = (*mempixel & 0xf0) | (uint8)( (~(*mempixel)) & 0x0f );
                }
                else
                {   // first pixel from the packet
                    *mempixel = (*mempixel & 0x0f) | (uint8)( (~(*mempixel)) & 0xf0 );
                }
            }
        }
    #elif (GDISP_PIXEL_FORMAT == gpixformat_8bitLUT)
        if ( color >= 0 )
            g_mem[ Y_poz * GDISP_MAX_MEM_W + X_poz ] = (uint8)color;
        else
            g_mem[ Y_poz * GDISP_MAX_MEM_W + X_poz ] = ~g_mem[ Y_poz * GDISP_MAX_MEM_W + X_poz ];
    #else
        {
            uint16 *mempixel = (uint16*)( g_mem + ( Y_poz * GDISP_MAX_MEM_W + (X_poz<<1) ) );
            *mempixel        = (uint16)color;
        }
    #endif
    }//END: SetSinglePixelInMemory


    int32 GetSinglePixelFromMemory( uint32 X_poz, uint32 Y_poz )
    {
    #if (GDISP_PIXEL_FORMAT == gpixformat_4bitLUT)
        {
            uint32 pixval = g_mem[ Y_poz * GDISP_MAX_MEM_W + (X_poz>>1) ];
            if ( X_poz & 0x01 )
            {   // second pixel from the packet
                return ( pixval & 0x0f );
            }
            else
            {   // first pixel from the packet
                return ( (pixval >> 4) & 0x0f );
            }
        }
    #elif (GDISP_PIXEL_FORMAT == gpixformat_1bit)
        uint32 pixval = g_mem[ (Y_poz>>3) * GDISP_MAX_MEM_W + X_poz ] & ( 1 << (Y_poz&0x07) );
        if ( pixval )
            return GRAPHIC_1BIT_WHITE;
        else
            return GRAPHIC_1BIT_BLACK;
    #elif (GDISP_PIXEL_FORMAT == gpixformat_8bitLUT)
        return (g_mem[ Y_poz * GDISP_MAX_MEM_W + X_poz ]);
    #else
        {
            uint16 *mempixel = (uint16*)( g_mem + ( Y_poz * GDISP_MAX_MEM_W + (X_poz<<1) ) );
            return *mempixel;
        }
    #endif
    }//END: GetSinglePixelFromMemory



    void MakeLine( int32 X1, int32 Y1, int32 X2, int32 Y2, bool window )
    {
        int32 dx = abs(X2-X1);
        int32 dy = abs(Y2-Y1); 
        int32 sx;
        int32 sy;
        int32 err;
        int32 e2;
        int32 color = G_var.color;


        register uint32 xmin;
        register uint32 xmax;
        register uint32 ymin;
        register uint32 ymax;

        if (window)
        {
            xmin =G_var.draw_window.X1;
            xmax =G_var.draw_window.X2;
            ymin =G_var.draw_window.Y1;
            ymax =G_var.draw_window.Y2;
        }

        if (X1 < X2) 
            sx = 1; 
        else 
            sx = -1;
        if (Y1 < Y2)
            sy = 1; 
        else 
            sy = -1;
        err = dx-dy;

        while (1)
        {
            if ( G_var.line_width == 1 )
            {
                if ( (window == false) ||
                     ( (X1 <= xmax) && (X1 >= xmin) && (Y1 <= ymax) && (Y1 >= ymin) ) )
                {
                    SetSinglePixelInMemory(X1,Y1,color);
                }
            }
            else
            {
                // TODO: TBI
            }

            if ( (X1 == X2) && (Y1 == Y2) )
                break;
            e2 = 2*err;
            if (e2 > -dy)
            {
                err = err - dy;
                X1 = X1 + sx;
            }
            if (e2 <  dx)
            {
                err = err + dx;
                Y1 = Y1 + sy;
            }
        }
    }//END: MakeLine



    static void MakeHorizontalLine( uint32 X1, uint32 X2, uint32 Y, int32 color )
    {
        uint32 lsize;
        uint32 i;

        // treate out of window cases
        if ( (Y < G_var.draw_window.Y1) || (Y > G_var.draw_window.Y2) )
            return;
        if ( (X2 < G_var.draw_window.X1) || (X1 > G_var.draw_window.X2) )
            return;
        if ( X1 < G_var.draw_window.X1 )
            X1 = G_var.draw_window.X1;
        if ( X2 > G_var.draw_window.X2 )
            X2 = G_var.draw_window.X2;

#if (GDISP_PIXEL_FORMAT == gpixformat_4bitLUT)
        //4bit version only for now
        uint8 bicolor;
        uint8 *mempixel;

        bicolor = ((color&0x0f) << 4) | (color&0x0f);
        mempixel = g_mem + Y * GDISP_MAX_MEM_W + (X1>>1);

        if ( X1 & 0x01 )    // odd pixel - complete the previous
        {
            if ( color >= 0 )
                *mempixel = (*mempixel & 0xf0) | (uint8)( color & 0x0f ); // normal color
            else
                *mempixel = (*mempixel & 0xf0) | (uint8)( (~(*mempixel)) & 0x0f );
            mempixel++;
            X1++;   // needed for lsize calculation 
        }

        lsize = (X2 - X1 + 1) >> 1;
        if ( color >= 0 )
        {
            if ( lsize )
                memset( mempixel, bicolor, lsize);
            mempixel += lsize;
        }
        else
        {
            for ( i=0; i<lsize; i++)
            {
                *mempixel = ~(*mempixel);
                mempixel++;
            }
        }

        if ((X2 & 0x01) == 0)   // even pixel - begin a new mem. poz.
        {
            if ( color >= 0 )
                *mempixel = (*mempixel & 0x0f) | (uint8)( (color << 4) & 0xf0 );
            else
                *mempixel = (*mempixel & 0x0f) | (uint8)( (~(*mempixel)) & 0xf0 );
        }
#else
        // RGB1555
        uint16 *mempixel;
        lsize       = X2 - X1;
        mempixel    = (uint16*)( g_mem + ( Y * GDISP_MAX_MEM_W + (X1<<1) ) );

        for ( i=0; i<=lsize; i++)
        {
            *mempixel   = (uint16)color;
            mempixel+= 1;
        }
#endif




    }//END: MakeHorizontalLine



    static void MakeVerticalLine( uint32 X, uint32 Y1, uint32 Y2, int32 color )
    {
        uint32 i;

        // treate out of window cases
        if ( (X < G_var.draw_window.X1) || (X > G_var.draw_window.X2) )
            return;
        if ( (Y2 < G_var.draw_window.Y1) || (Y1 > G_var.draw_window.Y2) )
            return;
        if ( Y1 < G_var.draw_window.Y1 )
            Y1 = G_var.draw_window.Y1;
        if ( Y2 > G_var.draw_window.Y2 )
            Y2 = G_var.draw_window.Y2;

#if (GDISP_PIXEL_FORMAT == gpixformat_4bitLUT)
        //4bit version only for now
        uint8 *mempixel;

        if ( X & 0x01 )    // odd pixel - complete the previous
        {
            for ( i=Y1; i<= Y2; i++  )
            {
                mempixel = g_mem + i * GDISP_MAX_MEM_W + (X>>1);
    
                if ( color >= 0 )
                    *mempixel = (*mempixel & 0xf0) | (uint8)( color & 0x0f ); // normal color
                else
                    *mempixel = (*mempixel & 0xf0) | (uint8)( (~(*mempixel)) & 0x0f );
            }
        }
        else
        {
            for ( i=Y1; i<= Y2; i++  )
            {
                mempixel = g_mem + i * GDISP_MAX_MEM_W + (X>>1);

                if ( color >= 0 )
                    *mempixel = (*mempixel & 0x0f) | (uint8)( (color << 4) & 0xf0 );
                else
                    *mempixel = (*mempixel & 0x0f) | (uint8)( (~(*mempixel)) & 0xf0 );
            }
        }
#else
        // RGB1555
        uint16 *mempixel;
        uint32 lsize;

        lsize       = Y2 - Y1;
        mempixel    = (uint16*)( g_mem + ( Y1 * GDISP_MAX_MEM_W + (X<<1) ) );

        for ( i=0; i<=lsize; i++)
        {
            *mempixel   = (uint16)color;
            mempixel+= GDISP_MAX_MEM_W / 2;
        }
#endif



    }//END: MakeVerticalLine


    void PutFillRect( uint32 X1, uint32 Y1, uint32 X2, uint32 Y2, int32 color )
    {

#if (GDISP_PIXEL_FORMAT == gpixformat_4bitLUT)
        // optimized for 4bit format
        if ( X1 & 0x01 )
        {
            MakeVerticalLine( X1, Y1, Y2, color );
            X1++;
        }
        if ( (X2 & 0x01) == 0 )
        {
            MakeVerticalLine( X2, Y1, Y2, color );
            if (X2) 
                X2--;
        }
        if ( X2 > X1 )
        {
            uint32 y;
            for (y=Y1; y<=Y2; y++ )
            {
                MakeHorizontalLine( X1, X2, y, color );
            }
        }
#elif (GDISP_PIXEL_FORMAT == gpixformat_1bit)
        internal_rectangle_draw_1bp( X1, Y1, X2, Y2, color, true );
#else
        {
            uint32 y;
            for (y=Y1; y<=Y2; y++ )
            {
                MakeHorizontalLine( X1, X2, y, color );
            }
        }
#endif

    }//END: PutFillRect


    void MakeRect( int32 X1, int32 Y1, int32 X2, int32 Y2 )
    {
        if ( (X2 < X1) || (Y2 < Y1) )
            return;
        if ( G_var.line_width == 0 )
            return;
        
        MakeHorizontalLine( X1, X2, Y1, G_var.color );
        MakeHorizontalLine( X1, X2, Y2, G_var.color );
        if ( (Y2 - Y1) > 1 ) 
        { 
            MakeVerticalLine( X1, Y1+1, Y2-1, G_var.color );
            MakeVerticalLine( X2, Y1+1, Y2-1, G_var.color );
        }

    }//END: MakeRect

