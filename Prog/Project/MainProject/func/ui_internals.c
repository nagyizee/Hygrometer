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



static void uist_mainwindow_statusbar( int rdrw )
{
    uint32 clock;
    int x;

    if ( (ui.m_state == UI_STATE_SETWINDOW) && (ui.m_setstate == UI_SET_QuickSwitch) )  // skip status bar drawing for quick switch panel
        return;

    clock = core_get_clock_counter();

    if ( rdrw & RDRW_BATTERY)
        uigrf_draw_battery(120, 0, core.measure.battery );

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

        switch ( ui.m_state )
        {
            case UI_STATE_MAIN_GAUGE:
                switch ( ui.main_mode )
                {
                    case UImm_gauge_thermo:     uigrf_text( 2, 4, uitxt_smallbold, "Thermo:" ); break;
                    case UImm_gauge_hygro:      uigrf_text( 2, 4, uitxt_smallbold, "Hygro:" );  break;
                    case UImm_gauge_pressure:   uigrf_text( 2, 4, uitxt_smallbold, "Baro:" );   break;
                }
                break;
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

void internal_gauge_put_hi_lo( bool high )
{
    Graphic_SetColor(0);
    Graphic_FillRectangle(13, 16, 47, 37, 0 );
    Graphic_FillRectangle(47, 30, 63, 37, 0 );
    
    if ( high )
        uibm_put_bitmap( 21, 16, BMP_GAUGE_HI );
    else
        uibm_put_bitmap( 21, 16, BMP_GAUGE_LO );
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
            internal_gauge_put_hi_lo( true );
        else if ( temp == NUM100_MIN )
            internal_gauge_put_hi_lo( false );
        else
            uigrf_putvalue_impact( 13, 16, temp, 3, 2, true );

        // tendency meter
        uigrf_putfixpoint( 81, 59, uitxt_micro, -2253, 3, 2, 0x00, false );

        core.measure.dirty.b.upd_temp = 0;
    }
    if ( redraw_all & RDRW_UI_CONTENT )
    {
        int x, y, i;
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

        for ( i=0; i<3; i++ )
        {
            internal_drawthermo_minmaxval( x+i*27, y+8,  core_utils_temperature2unit( core.nv.op.sens_rd.minmax[CORE_MMP_TEMP].max[GET_MM_SET_SELECTOR( mms, i )],(enum ETemperatureUnits)unit ) );
            internal_drawthermo_minmaxval( x+i*27, y+16, core_utils_temperature2unit( core.nv.op.sens_rd.minmax[CORE_MMP_TEMP].min[GET_MM_SET_SELECTOR( mms, i )],(enum ETemperatureUnits)unit ) );
        }

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
                uist_internal_tendencyval2pixels( &core.nv.op.sens_rd.tendency[CORE_MMP_TEMP] );

            uigrf_put_graph_small( 77, 16, grf_values, STORAGE_TENDENCY,
                                   core.nv.op.sens_rd.tendency[CORE_MMP_TEMP].w,
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
                internal_gauge_put_hi_lo( true );
            else if ( hygro == NUM100_MIN )
                internal_gauge_put_hi_lo( false );
            else
                uigrf_putvalue_impact( 13, 16, hygro, 3, 2, true );
        }
        else
        {
            if ( ui.p.mgHygro.unitH == hu_rh )
                hygro = core.measure.measured.rh;
            else
                hygro = core.measure.measured.absh;
            uigrf_putvalue_impact( 13, 16, hygro, 3, 2, false );
        }

        // tendency meter
        uigrf_putfixpoint( 81, 59, uitxt_micro, -2253, 3, 2, 0x00, false );
        core.measure.dirty.b.upd_hygro = 0;
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
                    internal_drawthermo_minmaxval( x+i*27, y+8, core.nv.op.sens_rd.minmax[CORE_MMP_RH].max[GET_MM_SET_SELECTOR( mms, i )] );
                    internal_drawthermo_minmaxval( x+i*27, y+16, core.nv.op.sens_rd.minmax[CORE_MMP_RH].min[GET_MM_SET_SELECTOR( mms, i )] );
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
                    internal_drawthermo_minmaxval( x+i*27, y+8, core.nv.op.sens_rd.minmax[CORE_MMP_ABSH].max[GET_MM_SET_SELECTOR( mms, i )] );
                    internal_drawthermo_minmaxval( x+i*27, y+16, core.nv.op.sens_rd.minmax[CORE_MMP_ABSH].min[GET_MM_SET_SELECTOR( mms, i )] );
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
                        uist_internal_tendencyval2pixels( &core.nv.op.sens_rd.tendency[CORE_MMP_RH] );
                    else
                        uist_internal_tendencyval2pixels( &core.nv.op.sens_rd.tendency[CORE_MMP_ABSH] );
                }
                uigrf_put_graph_small( 77, 16, grf_values, STORAGE_TENDENCY,
                                       core.nv.op.sens_rd.tendency[CORE_MMP_RH].w,                  // should be the same for RH and abs hum.
                                       grf_up_lim, grf_dn_lim, 3, grf_decpts );
            }
            core.measure.dirty.b.upd_th_tendency = 0;
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

    int x,y;

    if ( redraw_all & RDRW_UI_DYNAMIC )
    {
        int press;

        press = core.measure.measured.pressure >> 2;
        uigrf_putvalue_impact( 7, 16, press, 4, 2, false );

        // tendency meter
        x = 77;
        y = 16;
        uigrf_putfixpoint( x+4, y+43, uitxt_micro, 1231, 4, 2, 0x00, false );
        uigrf_text( x+32, y+43, uitxt_micro, "U/MIN" );
        core.measure.dirty.b.upd_pressure = 0;
    }

    if ( redraw_all & RDRW_UI_CONTENT )
    {
    // altimetric setup
        x = 0;
        y = 40;
    
        Graphic_SetColor( 1 );
        Graphic_FillRectangle( x, y, x + 41, y + 6, 1 );
        uigrf_text_inv( x+3, y+1, uitxt_micro,  "REFERENCE" );
    
        uigrf_text( x, y+9, uitxt_micro,  "MSL:" );
    //    uigrf_text( x+16, y+9, uitxt_micro, "1013.25" );
        Graphic_SetColor( -1 );
        Graphic_FillRectangle( x, y+8, x + 41, y + 14, -1 );
        Graphic_PutPixel(x, y, 0);
        Graphic_PutPixel(x+41, y, 1);
        uigrf_text( x, y+16, uitxt_micro,  "ALT:" );
//      uigrf_text( x+16, y+17, uitxt_micro,  "26495 F" );
    
    
        // min/max set display
        x = 42;
        y = 41;
        Graphic_SetColor( 1 );
        Graphic_Rectangle( x, y+1, x, y+22);
        Graphic_FillRectangle( x+1, y, x + 34, y + 6, 1 );
        Graphic_PutPixel(x+34, y, 0);
    //    uigrf_text_inv( x+10, y+1, uitxt_micro,  "SET1" );
        uigrf_text( x+8, y+9, uitxt_micro, "1002.5" );
        uigrf_text( x+8, y+16, uitxt_micro, " 996.4" );
    
        // tendency graph
        {
            if ( core.measure.dirty.b.upd_press_tendency )
                uist_internal_tendencyval2pixels( &core.nv.op.sens_rd.tendency[CORE_MMP_PRESS] );

            uigrf_put_graph_small( 77, 16, grf_values, STORAGE_TENDENCY,
                                   core.nv.op.sens_rd.tendency[CORE_MMP_PRESS].w,
                                   grf_up_lim, grf_dn_lim, 3, grf_decpts );

        }


        core.measure.dirty.b.upd_press_tendency = 0;
        core.measure.dirty.b.upd_press_minmax = 0;
        uist_internal_disp_all_with_focus();
    }
}


static inline void uist_draw_setwindow_quickswitch( int redraw_type )
{
    if ( redraw_type & RDRW_UI_CONTENT_ALL )
    {
        uigrf_text( 0, 3, uitxt_smallbold,  "Monitor" );
        uigrf_text( 61, 3, uitxt_smallbold,  "Register" );
        Graphic_SetColor(1);
        Graphic_Line(0,15,127,15);
        uibm_put_bitmap( 0, 16, BMP_QSW_THP_RATE );

    }

    if ( redraw_type & RDRW_UI_CONTENT )
    {


        uist_internal_disp_all_with_focus();
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
    uiel_control_list_set_callback( &ui.p.mgThermo.units, UIClist_Vchange, 0, ui_call_maingauge_thermo_unit_vchange );
    uiel_control_list_set_callback( &ui.p.mgThermo.units, UIClist_EscLong, 0, ui_call_maingauge_thermo_unit_toDefault );
    uiel_control_list_set_callback( &ui.p.mgThermo.units, UIClist_Esc, 0, ui_call_maingauge_esc_pressed );
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
        uiel_control_list_set_callback( &ui.p.mgThermo.minmaxset[i], UIClist_Vchange, i, ui_call_maingauge_thermo_minmax_vchange );
        uiel_control_list_set_callback( &ui.p.mgThermo.minmaxset[i], UIClist_EscLong, i, ui_call_maingauge_thermo_minmax_toDefault );
        uiel_control_list_set_callback( &ui.p.mgThermo.minmaxset[i], UIClist_Esc, i, ui_call_maingauge_esc_pressed );
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
    uiel_control_list_set_callback( &ui.p.mgHygro.units, UIClist_Vchange, 0, ui_call_maingauge_hygro_unit_vchange );
    uiel_control_list_set_callback( &ui.p.mgHygro.units, UIClist_EscLong, 0, ui_call_maingauge_hygro_unit_toDefault );
    uiel_control_list_set_callback( &ui.p.mgHygro.units, UIClist_Esc, 0, ui_call_maingauge_esc_pressed );
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
        uiel_control_list_set_index( &ui.p.mgHygro.minmaxset[i], GET_MM_SET_SELECTOR(core.nv.setup.show_mm_hygro, i) );
        uiel_control_list_set_callback( &ui.p.mgHygro.minmaxset[i], UIClist_Vchange, i, ui_call_maingauge_hygro_minmax_vchange );
        uiel_control_list_set_callback( &ui.p.mgHygro.minmaxset[i], UIClist_EscLong, i, ui_call_maingauge_hygro_minmax_toDefault );
        uiel_control_list_set_callback( &ui.p.mgHygro.minmaxset[i], UIClist_Esc, i, ui_call_maingauge_esc_pressed );
        ui.ui_elems[i+1] = &ui.p.mgHygro.minmaxset[i];
    }

    ui.ui_elem_nr = 4;
}


static inline void uist_setview_mainwindowgauge_pressure( void )
{
    // unit selector
    uiel_control_list_init( &ui.p.mgPress.units, 52, 16, 23, uitxt_small, 1, false );
    uiel_control_list_add_item( &ui.p.mgPress.units, "hPa", 0 );
    uiel_control_list_add_item( &ui.p.mgPress.units, "Hgmm", 1 );
    uiel_control_list_set_index( &ui.p.mgPress.units, core.nv.setup.show_unit_press );
    ui.ui_elems[0] = &ui.p.mgPress.units;
    // reference setup
    uiel_control_edit_init( &ui.p.mgPress.msl_press, 14, 47, uitxt_micro, uiedit_numeric, 6, 2 );
    uiel_control_edit_set_num( &ui.p.mgPress.msl_press, core.nv.op.params.press_msl + 50000 );
    ui.ui_elems[1] = &ui.p.mgPress.msl_press;

    uiel_control_edit_init( &ui.p.mgPress.crt_altitude, 14, 55, uitxt_micro, uiedit_numeric, 5, 0 );
    uiel_control_edit_set_num( &ui.p.mgPress.crt_altitude, core.nv.op.params.press_alt );
    ui.ui_elems[2] = &ui.p.mgPress.crt_altitude;

    // min/max set
    uiel_control_list_init( &ui.p.mgPress.minmaxset, 52, 40, 16, uitxt_micro, 0, true );
    uiel_control_list_add_item( &ui.p.mgPress.minmaxset, "SET1", mms_set1 );
    uiel_control_list_add_item( &ui.p.mgPress.minmaxset, "SET2", mms_set2 );
    uiel_control_list_add_item( &ui.p.mgPress.minmaxset, "DAY", mms_day_crt );
    uiel_control_list_add_item( &ui.p.mgPress.minmaxset, "DAY-", mms_day_bfr );
    uiel_control_list_add_item( &ui.p.mgPress.minmaxset, "WEEK", mms_week_crt );
    uiel_control_list_add_item( &ui.p.mgPress.minmaxset, "WK-", mms_week_bfr );
    uiel_control_list_set_index( &ui.p.mgPress.minmaxset, GET_MM_SET_SELECTOR(core.nv.setup.show_mm_press, 0) );
    ui.ui_elems[3] = &ui.p.mgPress.minmaxset;

    ui.ui_elem_nr = 4;
}


const char upd_rates[][6]               = { "5 sec",  "10sec",  "30sec",  "1 min",  "2 min",  "5 min",  "10min",  "30min",  "60min" };
const enum EUpdateTimings upd_timings[] = { ut_5sec,  ut_10sec, ut_30sec, ut_1min,  ut_2min,  ut_5min,  ut_10min, ut_30min, ut_60min };

static inline void uist_setview_setwindow_quickswitch( void )
{   
    int i, j;
    // monitoring on/off switch
    uiel_control_checkbox_init( &ui.p.swQuickSw.monitor, 40, 2 );
    uiel_control_checkbox_set( &ui.p.swQuickSw.monitor, core.nv.op.op_flags.b.op_monitoring );
    uiel_control_checkbox_set_callback( &ui.p.swQuickSw.monitor, UICcb_Esc, 0, ui_call_setwindow_quickswitch_esc_pressed );
    ui.ui_elems[0] = &ui.p.swQuickSw.monitor;

    // registering on/off switch
    uiel_control_checkbox_init( &ui.p.swQuickSw.reg, 104, 2 );
    uiel_control_checkbox_set( &ui.p.swQuickSw.reg, core.nv.op.op_flags.b.op_registering );
    uiel_control_checkbox_set_callback( &ui.p.swQuickSw.reg, UICcb_Esc, 0, ui_call_setwindow_quickswitch_esc_pressed );
    ui.ui_elems[1] = &ui.p.swQuickSw.reg;

    // monitoring rate setup
    for ( i=0; i<3; i++ )
    {
        uiel_control_list_init( &ui.p.swQuickSw.m_rates[i], 12, 16 + i*11, 25, uitxt_small, 1, false );
        for (j=0; j< (sizeof(upd_timings) / sizeof(enum EUpdateTimings)); j++ )
            uiel_control_list_add_item( &ui.p.swQuickSw.m_rates[i], upd_rates[j], upd_timings[j] );
        switch (i)
        {
            case 0: uiel_control_list_set_index(&ui.p.swQuickSw.m_rates[i], (enum EUpdateTimings)core.nv.setup.tim_tend_temp ); break;
            case 1: uiel_control_list_set_index(&ui.p.swQuickSw.m_rates[i], (enum EUpdateTimings)core.nv.setup.tim_tend_hygro ); break;
            case 2: uiel_control_list_set_index(&ui.p.swQuickSw.m_rates[i], (enum EUpdateTimings)core.nv.setup.tim_tend_press ); break;
        }
        uiel_control_list_set_callback ( &ui.p.swQuickSw.m_rates[i], UIClist_Esc, i+1, ui_call_setwindow_quickswitch_esc_pressed );
        ui.ui_elems[2+i] = &ui.p.swQuickSw.m_rates[i];
    }

    uiel_control_pushbutton_init( &ui.p.swQuickSw.resetMM, 0, 51, 37, 12 );
    uiel_control_pushbutton_set_content( &ui.p.swQuickSw.resetMM, uicnt_text, 0, "RESET MM", uitxt_micro );
    uiel_control_pushbutton_set_callback( &ui.p.swQuickSw.resetMM, UICpb_Esc, 0, ui_call_setwindow_quickswitch_esc_pressed );
    uiel_control_pushbutton_set_callback( &ui.p.swQuickSw.resetMM, UICpb_OK, 0, ui_call_setwindow_quickswitch_reset_minmax );
    ui.ui_elems[5] = &ui.p.swQuickSw.resetMM;

    ui.ui_elem_nr = 6;
}




static void local_drawwindow_common_op( int redraw_type )
{
    // if all the content should be redrawn
    if ( redraw_type == RDRW_ALL )
        Graphics_ClearScreen(0);

    // if status bar should be redrawn
    if ( redraw_type & RDRW_STATUSBAR )
    {
        uist_mainwindow_statusbar( redraw_type );
    }

}


////////////////////////////////////////////////////
//
//   UI Setups and redraw selectors
//
////////////////////////////////////////////////////

void uist_drawview_modeselect( int redraw_type )
{
    local_drawwindow_common_op( redraw_type );
    
    if ( redraw_type & RDRW_UI_CONTENT_ALL )
    {
        uibm_put_bitmap( 0, 16, BMP_MAIN_SELECTOR );
    }
}


void uist_drawview_mainwindow( int redraw_type )
{
    local_drawwindow_common_op( redraw_type );

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


void uist_drawview_setwindow( int redraw_type )
{
    local_drawwindow_common_op( redraw_type );

    // if UI content should be redrawn
    if ( redraw_type & (~RDRW_STATUSBAR) )                          // if there are other things to be redrawn from the UI
    {
        switch ( ui.m_setstate )
        {
            case UI_SET_QuickSwitch:    uist_draw_setwindow_quickswitch(redraw_type); break;
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


void uist_drawview_popup(int redraw_type)
{
    if ( redraw_type & RDRW_UI_CONTENT )
    {
        int i;

        uigrf_rounded_rect(10, 16, 117, 60, 1, true, 0);

        if ( ui.popup.params.line1[0] != 0x00 )
            uigrf_text( ui.popup.params.x1 + 10, ui.popup.params.y1 + 16, (enum Etextstyle)ui.popup.params.style1, ui.popup.params.line1 );
        if ( ui.popup.params.line2[0] != 0x00 )
            uigrf_text( ui.popup.params.x2 + 10, ui.popup.params.y2 + 16, (enum Etextstyle)ui.popup.params.style2, ui.popup.params.line2 );


        ui_element_display( &ui.popup.pb1, (ui.popup.focus == 0) );
        if ( ui.popup.elems == 2 )
            ui_element_display( &ui.popup.pb2, (ui.popup.focus == 1) );
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

void uist_setupview_setwindow( bool reset )
{
    ui.focus = 1;   // default focus on first element

    switch ( ui.m_setstate )
    {
        case UI_SET_QuickSwitch:    uist_setview_setwindow_quickswitch(); break;
    }
}


void uist_setupview_debuginputs( void )
{

}
