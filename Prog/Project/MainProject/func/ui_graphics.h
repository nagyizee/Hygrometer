#ifndef UI_GRAPHICS_H
#define UI_GRAPHICS_H

#ifdef __cplusplus
    extern "C" {
#endif


    #define BMP_MAIN_SELECTOR   0
    #define BMP_GRF_SELECTED    1
    #define BMP_GAUGE_HI        2
    #define BMP_GAUGE_LO        3
    #define BMP_QSW_THP_RATE    4
    #define BMP_ICO_OP_NONE     5
    #define BMP_ICO_OP_MONI     6
    #define BMP_ICO_OP_REG      7
    #define BMP_ICO_OP_REGMONI  8
    #define BMP_ICO_REGST_START 9
    #define BMP_ICO_REGST_STOP  10
    #define BMP_ICO_REGST_H     11
    #define BMP_ICO_REGST_HP    12
    #define BMP_ICO_REGST_P     13
    #define BMP_ICO_REGST_T     14
    #define BMP_ICO_REGST_TH    15
    #define BMP_ICO_REGST_THP   16
    #define BMP_ICO_REGST_TP    17
    #define BMP_ICO_REGST_TASK1 18
    #define BMP_ICO_REGST_TASK2 19
    #define BMP_ICO_REGST_TASK3 20
    #define BMP_ICO_REGST_TASK4 21
    #define BMP_ICO_WAIT        22

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

    void uigrf_rounded_rect( int x1, int y1, int x2, int y2, int color, bool fill, int bgnd ); 

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
