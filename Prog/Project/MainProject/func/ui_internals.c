/************************************************
 *
 *      UI drawing, setup and helper routines
 *
 *
 ***********************************************/


#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "hw_stuff.h"
#include "events_ui.h"
#include "ui_internals.h"
#include "ui.h"
#include "core.h"
#include "graphic_lib.h"
#include "ui_graphics.h"
#include "utilities.h"

#ifndef ON_QT_PLATFORM
  #include "stdlib_extension.h"
  #include "dispHAL.h"
#endif


extern struct SUIstatus ui;
extern struct SCore core;



////////////////////////////////////////////////////
//
//   UI helper routines
//
////////////////////////////////////////////////////

const char c_menu_main[]            = { "combine shutter setup" };
const char c_menu_setup[]           = { "display power_save reset" };

// local general purpose buffers for graph and tendency
int grf_up_lim;
int grf_dn_lim;
uint32 grf_decpts;
uint8 grf_values[256];   // 128 pixels max - doubled for min/max registration


static int uist_internal_setup_menu( void *handle, const char *list )
{
    const char *ptr = list;
    const char *sptr= list;
    int res;
    int len;
    char temp[16];

    while ( 1 )
    {
        ptr++;
        if ( (*ptr == 0x00) || (*ptr == ' ') )
        {
            len = ptr - sptr;
            strncpy( temp, sptr, len );
            temp[len]   = 0;
            res = uiel_dropdown_menu_add_item( (struct Suiel_dropdown_menu *)handle, temp );
            sptr = ptr + 1;
            if ( res || (*ptr == 0x00) )
                return res;
        }
    }
}


uint8 uist_internal_get_pixel_value( int in_val, int minval, int maxval )
{
    return (uint8)( 1 + ((in_val - minval) * 40) / (maxval - minval) );
}


void uist_internal_tendencyval2pixels( struct STendencyBuffer *tend )
{
    int i,j,nr;
    int up_lim;
    int dn_lim;
    int temp;

    // if no tendency graph or only one value - fill with uniform values
    if ( tend->c < 2 )
    {
        uint8 pix_val;

        if (tend->c == 0)
        {
            pix_val = 20;
            grf_dn_lim = 0;
            grf_up_lim = 0;
            grf_decpts = 0;
        }
        else
        {
            temp = core_utils_temperature2unit( tend->value[0], ui.p.mgThermo.unitT );      // x100 units

            if ( temp < 0 )
            {
                grf_up_lim = (temp / 100);
                grf_dn_lim = (temp / 100) - 1;
            }
            else
            {
                grf_up_lim = (temp / 100) + 1;
                grf_dn_lim = (temp / 100);
            }
            pix_val = uist_internal_get_pixel_value( tend->value[0], 
                                                     core_utils_unit2temperature( grf_dn_lim*100, ui.p.mgThermo.unitT ),
                                                     core_utils_unit2temperature( grf_up_lim*100, ui.p.mgThermo.unitT ) );
        }

        for( i=0; i<STORAGE_TENDENCY; i++)
            grf_values[i] = pix_val;

        return;
    }

    // else - find out the min/max values
    dn_lim = tend->value[0];
    up_lim = tend->value[0];
    for ( i=0; i<tend->c; i++ )
    {
        if ( dn_lim > tend->value[i] )
            dn_lim = tend->value[i];
        if ( up_lim < tend->value[i] )
            up_lim = tend->value[i];
    }

    // normalize them to integer
    temp = core_utils_temperature2unit( dn_lim, ui.p.mgThermo.unitT );
    if (temp < 0)
        grf_dn_lim = temp / 100 - 1;
    else
        grf_dn_lim = temp / 100;
    temp = core_utils_temperature2unit( up_lim, ui.p.mgThermo.unitT );
    if (temp < 0)
        grf_up_lim = temp / 100;
    else
        grf_up_lim = temp / 100 + 1;

    dn_lim = core_utils_unit2temperature( grf_dn_lim*100, ui.p.mgThermo.unitT );
    up_lim = core_utils_unit2temperature( grf_up_lim*100, ui.p.mgThermo.unitT );
    grf_decpts = 0;

    // calculate the pixel values
    if ( (tend->c < STORAGE_TENDENCY) || (tend->w == 0) )       // tendency buffer is not filled or is filled exactly with it's size
        i = 0;
    else
        i = tend->w;
    j = 0;
    nr = 0;
    if ( tend->c < STORAGE_TENDENCY )
    {
        uint8 fillval;
        fillval = uist_internal_get_pixel_value( tend->value[0], dn_lim, up_lim );
        for ( j=0; j<( STORAGE_TENDENCY - tend->c); j++ )           // first fill the
            grf_values[j] = fillval;
    }
    while ( nr < tend->c )
    {
        grf_values[j] = uist_internal_get_pixel_value( tend->value[i], dn_lim, up_lim );
        j++;
        i++;
        nr++;
        if ( i == STORAGE_TENDENCY )
            i = 0;
    }
}



static void uist_mainwindow_statusbar( uint32 opmode, int rdrw )
{
    uint32 clock;
    int x;

    clock = core_get_clock_counter();

    if ( rdrw & RDRW_BATTERY)
        uigrf_draw_battery(120, 0, 50);

    if ( (rdrw & RDRW_STATUSBAR) == RDRW_STATUSBAR )
    {
        // redraw everything
        Graphic_SetColor(0);
        Graphic_FillRectangle( 0, 0, 118, 15, 0);
        Graphic_SetColor(0);

        Graphic_SetColor(1);
        Graphic_Line(13,15,127,15);
        // draw the page selection
        for ( x=0; x<3; x++ )
        {
            if ( x == ui.main_mode )
                Graphic_Rectangle( 1+x*4, 14, 3+x*4, 15 );
            else
                Graphic_PutPixel( 2+x*4, 15, 1 );
        }

        switch ( opmode )
        {
            case UImm_gauge_thermo:     uigrf_text( 2, 4, uitxt_smallbold, "Thermo:" ); break;
            case UImm_gauge_hygro:      uigrf_text( 2, 4, uitxt_smallbold, "Hygro:" );  break;
            case UImm_gauge_pressure:   uigrf_text( 2, 4, uitxt_smallbold, "Baro:" );   break;
        }

        // draw time
        {
            timestruct tm;
            datestruct dt;
            utils_convert_counter_2_hms( clock, &tm.hour, &tm.minute, NULL );
            utils_convert_counter_2_ymd( clock, &dt.year, &dt.mounth, &dt.day );
            uigrf_puttime( 92, 0, uitxt_small, 1, tm, true, false );
            uigrf_putdate( 93, 9, uitxt_micro, 1, dt, false, true );
        }
    }

    // clock blink
    if ( clock & 0x01 )
    {
        Graphic_PutPixel( 104, 2, 1 );
        Graphic_PutPixel( 104, 5, 1 );
    }
    else
    {
        Graphic_PutPixel( 104, 2, 0 );
        Graphic_PutPixel( 104, 5, 0 );
    }
}

static void uist_internal_disp_with_focus( int i )
{
    ui_element_display( ui.ui_elems[i], (ui.focus - 1) == i );
}

static void uist_internal_disp_all_with_focus()
{
    int i;
    for ( i=0; i<ui.ui_elem_nr; i++ )
        ui_element_display( ui.ui_elems[i], (ui.focus - 1) == i );
}

void internal_drawthermo_minmaxval( int x, int y, int value )
{
    if ( value == NUM100_MAX )
        uigrf_text( x, y, (enum Etextstyle)(uitxt_small | uitxt_MONO), " HI" );
    else if ( value == NUM100_MIN )
        uigrf_text( x, y, (enum Etextstyle)(uitxt_small | uitxt_MONO), " --" );
    else
        uigrf_putfixpoint( x, y, uitxt_small, value / 10, 3, 1, 0x00, false );
}


////////////////////////////////////////////////////
//
//   UI status dependent redraw routines
//
////////////////////////////////////////////////////


static inline void uist_draw_gauge_thermo( int redraw_all )
{
    if ( redraw_all & RDRW_UI_DYNAMIC )
    {
        // convert temperature from base unit to the selected one
        int temp;
        temp = core_utils_temperature2unit( core.measure.measured.temperature, ui.p.mgThermo.unitT );

        // temperature display
        if ( temp == NUM100_MAX )
            uigrf_text( 13, 17, (enum Etextstyle)(uitxt_smallbold | uitxt_MONO), "HI" );
        else if ( temp == NUM100_MIN )
            uigrf_text( 13, 17, (enum Etextstyle)(uitxt_smallbold | uitxt_MONO), "LO" );
        else
            uigrf_putvalue_impact( 13, 17, temp, 3, 2, true );

        // tendency meter
        uigrf_putfixpoint( 81, 59, uitxt_micro, -2253, 3, 2, 0x00, false );

        core.measure.dirty.b.upd_temp = 0;
    }
    if ( redraw_all & RDRW_UI_CONTENT )
    {
        int x, y;
        uint32 mms;
        int unit;
        // min/max set display
        x = 0;
        y = 41;

        mms = core.nv.setup.show_mm_temp;
        unit = ui.p.mgThermo.unitT;

        Graphic_SetColor( 0 );
        Graphic_FillRectangle( x, y+7, x + 75, y + 22, 0 );
        Graphic_SetColor( 1 );
        Graphic_FillRectangle( x, y, x + 75, y + 6, 1 );

        internal_drawthermo_minmaxval( x, y+8, core_utils_temperature2unit( core.nv.op.sens_rd.minmax.temp_max[GET_MM_SET_SELECTOR( mms, 0 )], (enum ETemperatureUnits)unit ) );
        internal_drawthermo_minmaxval( x, y+16, core_utils_temperature2unit( core.nv.op.sens_rd.minmax.temp_min[GET_MM_SET_SELECTOR( mms, 0 )], (enum ETemperatureUnits)unit ) );
        internal_drawthermo_minmaxval( x+27, y+8, core_utils_temperature2unit( core.nv.op.sens_rd.minmax.temp_max[GET_MM_SET_SELECTOR( mms, 1 )], (enum ETemperatureUnits)unit ) );
        internal_drawthermo_minmaxval( x+27, y+16, core_utils_temperature2unit( core.nv.op.sens_rd.minmax.temp_min[GET_MM_SET_SELECTOR( mms, 1 )], (enum ETemperatureUnits)unit ) );
        internal_drawthermo_minmaxval( x+54, y+8, core_utils_temperature2unit( core.nv.op.sens_rd.minmax.temp_max[GET_MM_SET_SELECTOR( mms, 2 )], (enum ETemperatureUnits)unit ) );
        internal_drawthermo_minmaxval( x+54, y+16, core_utils_temperature2unit( core.nv.op.sens_rd.minmax.temp_min[GET_MM_SET_SELECTOR( mms, 2 )], (enum ETemperatureUnits)unit ) );

        // for tendency meter
        switch ( unit )
        {
            case tu_C: uigrf_text( 104, 59, uitxt_micro, "*C/MIN" ); break;
            case tu_F: uigrf_text( 104, 59, uitxt_micro, "*F/MIN" ); break;
            case tu_K: uigrf_text( 104, 59, uitxt_micro, "*K/MIN" ); break;
        }

        // tendency graph
        {
            if ( core.measure.dirty.b.upd_th_tendency )
                uist_internal_tendencyval2pixels( &core.nv.op.sens_rd.tendency.temp );

            uigrf_put_graph_small( 77, 16, grf_values, STORAGE_TENDENCY,
                                   core.nv.op.sens_rd.tendency.temp.w,
                                   grf_up_lim, grf_dn_lim, 3, grf_decpts );

        }

        core.measure.dirty.b.upd_temp_minmax = 0;
        core.measure.dirty.b.upd_th_tendency = 0;

        uist_internal_disp_all_with_focus();
    }
}


static inline void uist_draw_gauge_hygro( int redraw_all )
{
    if ( redraw_all & RDRW_UI_DYNAMIC )
    {
        // convert temperature from base unit to the selected one
        int hygro;

        if ( ui.p.mgHygro.unitH == hu_dew )
        {
            hygro = core_utils_temperature2unit( core.measure.measured.dewpoint, (enum ETemperatureUnits)core.nv.setup.show_unit_temp );
            // temperature display
            if ( hygro == NUM100_MAX )
                uigrf_text( 13, 17, (enum Etextstyle)(uitxt_smallbold | uitxt_MONO), "HI" );
            else if ( hygro == NUM100_MIN )
                uigrf_text( 13, 17, (enum Etextstyle)(uitxt_smallbold | uitxt_MONO), "LO" );
            else
                uigrf_putvalue_impact( 13, 17, hygro, 3, 2, true );
        }
        else
        {
            if ( ui.p.mgHygro.unitH == hu_rh )
                hygro = core.measure.measured.rh;
            else
                hygro = core.measure.measured.absh;
            uigrf_putvalue_impact( 13, 17, hygro, 3, 2, false );
        }

        // tendency meter
        uigrf_putfixpoint( 81, 59, uitxt_micro, -2253, 3, 2, 0x00, false );
        core.measure.dirty.b.upd_hum = 0;
    }
    if ( redraw_all & RDRW_UI_CONTENT )
    {
        int x, y, i;
        uint32 mms;
        // min/max set display
        x = 0;
        y = 41;

        mms = core.nv.setup.show_mm_hygro;

        Graphic_SetColor( 0 );
        Graphic_FillRectangle( x, y+7, x + 75, y + 22, 0 );
        Graphic_SetColor( 1 );
        Graphic_FillRectangle( x, y, x + 75, y + 6, 1 );

        switch ( ui.p.mgHygro.unitH )
        {
            case hu_rh:
                for ( i=0; i<3; i++ )
                {
                    internal_drawthermo_minmaxval( x+i*27, y+8, core.nv.op.sens_rd.minmax.rh_max[GET_MM_SET_SELECTOR( mms, i )] );
                    internal_drawthermo_minmaxval( x+i*27, y+16, core.nv.op.sens_rd.minmax.rh_min[GET_MM_SET_SELECTOR( mms, i )] );
                }
                break;
            case hu_dew:
                for ( i=0; i<3; i++ )
                {
                    internal_drawthermo_minmaxval( x+i*27, y+8, NUM100_MIN );       // no min/max for dew point
                    internal_drawthermo_minmaxval( x+i*27, y+16, NUM100_MIN );
                }
                break;
            case hu_abs:
                for ( i=0; i<3; i++ )
                {
                    internal_drawthermo_minmaxval( x+i*27, y+8, core.nv.op.sens_rd.minmax.absh_max[GET_MM_SET_SELECTOR( mms, i )] );
                    internal_drawthermo_minmaxval( x+i*27, y+16, core.nv.op.sens_rd.minmax.absh_min[GET_MM_SET_SELECTOR( mms, i )] );
                }
                break;
        }

        // for tendency meter
        switch ( ui.p.mgHygro.unitH )
        {
            case hu_rh:  uigrf_text( 104, 59, uitxt_micro, "% RH" ); break;
            case hu_dew: uigrf_text( 104, 59, uitxt_micro, "DEW*" ); break;
            case hu_abs: uigrf_text( 104, 59, uitxt_micro, "G/M3" ); break;
        }

        // tendency graph
        {
            if ( ui.p.mgHygro.unitH == hu_dew )
            {
                // display empty graph since we don't measure this
                uigrf_put_graph_small( 77, 16, grf_values, 0, 0, grf_up_lim, grf_dn_lim, 3, grf_decpts );
            }
            else
            {
                if ( core.measure.dirty.b.upd_th_tendency )
                {
                    if ( ui.p.mgHygro.unitH == hu_rh )
                        uist_internal_tendencyval2pixels( &core.nv.op.sens_rd.tendency.RH );
                    else
                        uist_internal_tendencyval2pixels( &core.nv.op.sens_rd.tendency.abshum );
                }
                uigrf_put_graph_small( 77, 16, grf_values, STORAGE_TENDENCY,
                                       core.nv.op.sens_rd.tendency.RH.w,                  // should be the same for RH and abs hum.
                                       grf_up_lim, grf_dn_lim, 3, grf_decpts );
                core.measure.dirty.b.upd_th_tendency = 0;
            }
        }
        core.measure.dirty.b.upd_hum_minmax = 0;
        core.measure.dirty.b.upd_abshum_minmax = 0;
        uist_internal_disp_all_with_focus();
    }
}


static inline void uist_draw_gauge_pressure( int redraw_all )
{
    int press = ( 1013.25 * 100 );       // testing purpose
    int press_int = press / 100;
    int press_fract = press % 100;

    // pressure display
    int x = 7;
    int y = 17;
    uigrf_putnr(x, y, (enum Etextstyle)(uitxt_large_num | uitxt_MONO), press_int, 4, ' ', false );
    uigrf_putnr(x+48, y+14, (enum Etextstyle)(uitxt_smallbold | uitxt_MONO), press_fract, 2, '0', false );
    Graphic_SetColor( 1 );
    Graphic_Rectangle( x+44, y+19, x+45, y+20 );
    uigrf_text( x+48, y+2, uitxt_small,  "hPa" );

    // altimetric setup
    x = 0;
    y = 41;

    Graphic_SetColor( 1 );
    Graphic_FillRectangle( x, y, x + 41, y + 6, 1 );
    uigrf_text_inv( x+3, y+1, uitxt_micro,  "REFERENCE" );

    uigrf_text( x, y+9, uitxt_micro,  "SLP:" );
    uigrf_text( x+16, y+9, uitxt_micro, "1013.25" );
    Graphic_SetColor( -1 );
    Graphic_FillRectangle( x, y+8, x + 41, y + 14, -1 );
    Graphic_PutPixel(x, y, 0);
    Graphic_PutPixel(x+41, y, 1);
    uigrf_text( x, y+17, uitxt_micro,  "ALT:" );
    uigrf_text( x+16, y+17, uitxt_micro,  "26495 F" );


    // min/max set display
    x = 42;
    y = 41;
    Graphic_SetColor( 1 );
    Graphic_Rectangle( x, y+1, x, y+22);
    Graphic_FillRectangle( x+1, y, x + 34, y + 6, 1 );
    Graphic_PutPixel(x+34, y, 0);
    uigrf_text_inv( x+10, y+1, uitxt_micro,  "SET1" );
    uigrf_text( x+8, y+9, uitxt_micro, "1002.5" );
    uigrf_text( x+8, y+17, uitxt_micro, " 996.4" );



    // tendency meter
    x = 77;
    y = 16;
    uigrf_putfixpoint( x+4, y+43, uitxt_micro, 1231, 4, 2, 0x00, false );
    uigrf_text( x+32, y+43, uitxt_micro, "U/MIN" );

    // tendency graph
    uigrf_putfixpoint( x+40, y, uitxt_micro, 29, 2, 1, 0x00, false );
    uigrf_putfixpoint( x+40, y+36, uitxt_micro, 01, 2, 1, 0x00, false );
    Graphic_Rectangle( x+40, y+6, x+41, y+34 );
    Graphic_Rectangle( x, y, x, y+40 );

    {
        uint8 values[39] = {4, 3, 3, 2, 0, 1, 1, 5, 6, 9,
                            12,14,15,30,33,39,40,40,38,34,
                            30,32,33,33,35,36,32,28,24,23,
                            22,22,21,18,19,25,26,27,26 };
        int i, j;

        Graphic_SetColor( 1 );

        for (j=0; j<=5; j++)
        {
            for (i=0; i<=6; i++)
                Graphic_PutPixel(x+i*6, y+j*8, 1);
        }

        for (i=0; i<38; i++)
        {
            Graphic_Line( x+1+i, y+40-values[i], x+2+i, y+40-values[i+1] );
        }

    }


}




////////////////////////////////////////////////////
//
//   UI status dependent window setups
//
////////////////////////////////////////////////////

static inline void uist_setview_mainwindowgauge_thermo( void )
{
    int i;
    uiel_control_list_init( &ui.p.mgThermo.units, 52, 16, 11, uitxt_small, 1, false );
    uiel_control_list_add_item( &ui.p.mgThermo.units, "*C", tu_C );
    uiel_control_list_add_item( &ui.p.mgThermo.units, "*F", tu_F );
    uiel_control_list_add_item( &ui.p.mgThermo.units, "*K", tu_K );
    uiel_control_list_set_index( &ui.p.mgThermo.units, core.nv.setup.show_unit_temp );
    ui.ui_elems[0] = &ui.p.mgThermo.units;

    for (i=0; i<3; i++)
    {
        uiel_control_list_init( &ui.p.mgThermo.minmaxset[i], 4+i*27, 41, 16, uitxt_micro, 0, true );
        uiel_control_list_add_item( &ui.p.mgThermo.minmaxset[i], "SET1", mms_set1 );
        uiel_control_list_add_item( &ui.p.mgThermo.minmaxset[i], "SET2", mms_set2 );
        uiel_control_list_add_item( &ui.p.mgThermo.minmaxset[i], "DAY", mms_day_crt );
        uiel_control_list_add_item( &ui.p.mgThermo.minmaxset[i], "DAY-", mms_day_bfr );
        uiel_control_list_add_item( &ui.p.mgThermo.minmaxset[i], "WEEK", mms_week_crt );
        uiel_control_list_add_item( &ui.p.mgThermo.minmaxset[i], "WK-", mms_week_bfr );
        uiel_control_list_set_index( &ui.p.mgThermo.minmaxset[i], GET_MM_SET_SELECTOR(core.nv.setup.show_mm_temp, i) );
        ui.ui_elems[i+1] = &ui.p.mgThermo.minmaxset[i];
    }

    ui.ui_elem_nr = 4;

    ui.p.mgThermo.unitT = (enum ETemperatureUnits)core.nv.setup.show_unit_temp;
}


static inline void uist_setview_mainwindowgauge_hygro( void )
{
    int i;
    uiel_control_list_init( &ui.p.mgHygro.units, 52, 16, 20, uitxt_small, 1, false );
    uiel_control_list_add_item( &ui.p.mgHygro.units, "%RH", 0 );
    uiel_control_list_add_item( &ui.p.mgHygro.units, "dew", 1 );
    uiel_control_list_add_item( &ui.p.mgHygro.units, "g/m3", 2 );
    uiel_control_list_set_index( &ui.p.mgHygro.units, core.nv.setup.show_unit_hygro );
    ui.ui_elems[0] = &ui.p.mgHygro.units;

    for (i=0; i<3; i++)
    {
        uiel_control_list_init( &ui.p.mgHygro.minmaxset[i], 4+i*27, 41, 16, uitxt_micro, 0, true );
        uiel_control_list_add_item( &ui.p.mgHygro.minmaxset[i], "SET1", mms_set1 );
        uiel_control_list_add_item( &ui.p.mgHygro.minmaxset[i], "SET2", mms_set2 );
        uiel_control_list_add_item( &ui.p.mgHygro.minmaxset[i], "DAY", mms_day_crt );
        uiel_control_list_add_item( &ui.p.mgHygro.minmaxset[i], "DAY-", mms_day_bfr );
        uiel_control_list_add_item( &ui.p.mgHygro.minmaxset[i], "WEEK", mms_week_crt );
        uiel_control_list_add_item( &ui.p.mgHygro.minmaxset[i], "WK-", mms_week_bfr );
        uiel_control_list_set_index( &ui.p.mgHygro.minmaxset[i], i );
        ui.ui_elems[i+1] = &ui.p.mgHygro.minmaxset[i];
    }

    ui.ui_elem_nr = 4;
}


static inline void uist_setview_mainwindowgauge_pressure( void )
{
    ui.ui_elem_nr = 0;
}


////////////////////////////////////////////////////
//
//   UI Setups and redraw selectors
//
////////////////////////////////////////////////////

void uist_drawview_modeselect( int redraw_type )
{
    if ( redraw_type == RDRW_ALL )
        Graphics_ClearScreen(0);
    if ( redraw_type & RDRW_STATUSBAR )
        uist_mainwindow_statusbar( ui.main_mode, redraw_type );

    if ( redraw_type & RDRW_UI_CONTENT_ALL )
    {
        uibm_put_bitmap( 0, 16, BMP_MAIN_SELECTOR );
    }
}


void uist_drawview_mainwindow( int redraw_type )
{
    // if all the content should be redrawn
    if ( redraw_type == RDRW_ALL )
        Graphics_ClearScreen(0);

    // if status bar should be redrawn
    if ( redraw_type & RDRW_STATUSBAR )
    {
        uist_mainwindow_statusbar( ui.main_mode, redraw_type );
    }

    // if UI content should be redrawn
    if ( redraw_type & (~RDRW_STATUSBAR) )                          // if there are other things to be redrawn from the UI
    {
        switch ( ui.main_mode )
        {
            case UImm_gauge_thermo:   uist_draw_gauge_thermo(redraw_type); break;
            case UImm_gauge_hygro:    uist_draw_gauge_hygro(redraw_type); break;
            case UImm_gauge_pressure: uist_draw_gauge_pressure(redraw_type); break;
            break;
        }
    }
}

                      // up  dn  l   r   ok  esc  mo
const uint32 dik_x[] = { 10, 10, 0,  20, 10, 0,  20 };
const uint32 dik_y[] = { 0,  20, 10, 10, 10, 20, 0  };

void uist_drawview_debuginputs( int redraw_type, uint32 key_bits )
{
    // if all the content should be redrawn
    int i;
    if ( redraw_type == RDRW_ALL )
    {
        int x,y;
        Graphics_ClearScreen(0);

        x = 0;
        y = 48;
        uigrf_text( x, y, uitxt_micro, "MO ES OK S1 S2 S3 S4" );
    }

    Graphic_SetColor(0);
    Graphic_FillRectangle( 0, 55, 70, 63, 0 );

    for (i=0; i<7; i++)
    {
        uint32 mask = (1<<i);

        if ( (key_bits & 0xff) & mask )     // keypress event
        {
            Graphic_SetColor(1);
            Graphic_FillRectangle( dik_x[i]+2, dik_y[i]+2+16, dik_x[i]+8, dik_y[i]+8+16, 1 );
        }
        else if ( (key_bits & 0xff00 ) & (mask << 8) ) // key released event
        {
            Graphic_SetColor(1);
            Graphic_FillRectangle( dik_x[i]+2, dik_y[i]+2+16, dik_x[i]+8, dik_y[i]+8+16, 0 );
        }
        else if ( (key_bits & 0xff0000 ) & (mask << 16) )  // key long pressed event
        {
            Graphic_SetColor(1);
            Graphic_FillRectangle( dik_x[i], dik_y[i]+16, dik_x[i]+10, dik_y[i]+10+16, 1 );
        }
        else
        {
            Graphic_SetColor(1);
            Graphic_FillRectangle( dik_x[i], dik_y[i]+16, dik_x[i]+10, dik_y[i]+10+16, 0 );
        }

        if ( key_bits & (mask<<24) )
        {
            Graphic_SetColor(1);
            Graphic_FillRectangle( 2+10*i, 57, 4+10*i, 59, 0 );
        }
    }

}


void uist_setupview_modeselect( bool reset )
{
    ui.focus = 0;
    ui.ui_elem_nr = 0;
}


void uist_setupview_mainwindow( bool reset )
{
    ui.focus = 0;

    // main windows
    switch ( ui.main_mode )
    {
        case UImm_gauge_thermo:      uist_setview_mainwindowgauge_thermo(); break;
        case UImm_gauge_hygro:       uist_setview_mainwindowgauge_hygro(); break;
        case UImm_gauge_pressure:    uist_setview_mainwindowgauge_pressure(); break;
    }
}

void uist_setupview_debuginputs( void )
{

}
