#ifndef UI_GRAPHICS_H
#define UI_GRAPHICS_H

#ifdef __cplusplus
    extern "C" {
#endif


    #define BMP_MAIN_SELECTOR   0
    #define BMP_GRF_SELECTED    1
    #define BMP_GAUGE_HI        2
    #define BMP_GAUGE_LO        3

    enum Etextstyle
    {
        uitxt_small = 0,
        uitxt_smallbold,
        uitxt_micro,
        uitxt_large_num,
        uitxt_system,

        uitxt_MONO = 0x80       // if orred with the syles abowe then it forces the characters to use mono space (it will take the longest character)
    };


    void uibm_put_bitmap( int x, int y, int bmp );

    void uigrf_draw_battery( int x, int y, int fullness );

    void uigrf_text( int x, int y, enum Etextstyle style, char *text );
    void uigrf_text_inv( int x, int y, enum Etextstyle style, char *text );

    void uigrf_text_mono( int x, int y, enum Etextstyle style, char *text, bool specialdot );

    void grf_setup_font( enum Etextstyle style, int color, int backgnd );

    void uigrf_putnr( int x, int y, enum Etextstyle style, int nr, int digits, char fill, bool show_plus_sign );
    void uigrf_puthex( int x, int y, enum Etextstyle style, int nr, int digits, char fill );
    void uigrf_putfixpoint( int x, int y, enum Etextstyle style, int nr, int digits, int fp, char fill, bool show_plus_sign );

    
    void uigrf_puttime( int x, int y, enum Etextstyle style, int color, timestruct time, bool minute_only, bool large_border );
    void uigrf_putdate( int x, int y, enum Etextstyle style, int color, datestruct date, bool show_year, bool show_mname );

    void uigrf_putvalue_impact( int x, int y, int value, int big_digits, int small_digits, bool use_plus ); 
    void uigrf_put_graph_small( int x, int y, uint8 *array, int length, int shift, int disp_up_lim, int disp_btm_lim, int digits, int decimalp );

        /// Utilities
    int poz_2_increment( int poz );


#ifdef __cplusplus
    }
#endif


#endif // UI_GRAPHICS_H
