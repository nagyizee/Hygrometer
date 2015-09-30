#ifndef GRAPHIC_LIB_USER_H
#define GRAPHIC_LIB_USER_H

#ifdef __cplusplus
    extern "C" {
#endif


#define gpixformat_NA           0        // used only as a routine parameter if no color format is given
#define gpixformat_1bit         1        // black/white - 0 black, 1 white. pixels are in columns of 8
#define gpixformat_4bitLUT      2        // 16 color, 0 is considered black, 15 is transparent
#define gpixformat_8bitLUT      3        // 256 color, 0 is considered black, 255 is transparent
#define gpixformat_16bitARGB    4        // 16bit ARGB:  1bit Alpha, 5bit R, 5bit G, 5bit B

#define GASSERTION_FULL              0x03    // full behavior check and error reporting
#define GASSERTION_CHECK_PARAMS      0x01    // check for API input params only
#define GASSERTION_NONE              0x00    // no parameter check   - can speed up execution but hide bugs


/**************************************************
Graphic library configuration - edit here
**************************************************/

#define CONF_DISP_WIDTH               128
#define CONF_DISP_HEIGHT              64
#define CONF_DISP_PIXEL_FORMAT        gpixformat_1bit       // depends on representation of pixels in memory, isn't related to display hardware

#define CONF_GASSERTION_LEVEL         GASSERTION_FULL       // runtime checks for implementation and usage failures, see valid values at GRAPHIC_ASSERTION_XXX defines

#define CONF_GRAPHIC_1BIT_BLACK       0       // depends on LCD module - the bit value for black or white
#define CONF_GRAPHIC_1BIT_WHITE       1

#define CONF_GRAPHIC_INT_MEM                  // if defined then graphic library will use an internal static memory buffer for speed access, 
                                              // otherwise memory buffer needs to be given externally
                                             
// #define CONF_GRAPHIC_INT_SSFONT            // define it to activate internal system font from the library ( ~1kB flash space )   
/**************************************************/

#ifdef __cplusplus
    }
#endif




#endif // GRAPHIC_LIB_USER_H
