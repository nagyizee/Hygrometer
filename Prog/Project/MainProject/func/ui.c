/*
 * - User interface state machine
 *
 *  Operation modes:
 *      - long expo:        Holding the shutter for x seconds set up on the main window
 *      - sequential:       Sequential photography with given nr. of pictures in given interval between them
 *      - timer:            Timer for first photo
 *      - light trigger:    Photo taken at light change event
 *      - sound trigger:    Photo taken at sound spike event
 *
 *  Mirror lockup can be activated with a timeout for vibration attenuation. settable in second.
 *  The effect of mirror lockup for various modes:
 *      - long expo:        long expo countdown begins after mirror lockup timeout.
 *      - sequential:       lockup interval is counted in the picture interval, actual picture is taken after mirror lockup timeout
 *      - timer:            timer is downcounted, then mirror lockup timeout, then picture is taken.
 *      - l/s trigger:      mirror lockup shutter release at starting the program, picture taken in moment of trigger event
 *
 *  Trigger event can be delayed in xxx.x milliseconds for precise triggering. Triggering is IRQ controlled, no application lags are considered
 *
 *  Allowed mode combinations:
 *      - all standalone
 *      - long expo, sequential, timer - in any combination - interval is increased if needed to accomodate longexpo + mirror lockup timerouts
 *      - light trigger is mutually exclussive with sound trigger
 *      - trigger can be combined with long expo and sequential
 *      - when trigger is combined with sequential then photos are taken at interval as normal, but trigger will alter the interval.
 *        Basically trigger can release the shutter for additional photos
 *
 *
 *
 *
 **/

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
#ifndef ON_QT_PLATFORM
  #include "stdlib_extension.h"
  #include "dispHAL.h"
#endif


struct SUIstatus ui;
static struct SCore *core;


////////////////////////////////////////////////////
//
//   UI helper routines
//
////////////////////////////////////////////////////

const char c_menu_main[]            = { "combine shutter setup" };
const char c_menu_setup[]           = { "display power_save reset" };


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


static void uist_internal_statusbar( uint32 opmode, bool rdrw_all )
{
    int x;
    uigrf_draw_battery(120, 0, core->measure.battery);

    if ( opmode == 0 )
        return;

    if ( ui.m_state == UI_STATE_MAIN_WINDOW )
    {
        Graphic_Line( 25, 15, 127, 15 );
        // draw the page selection
        for ( x=0; x<6; x++ )
        {
            if ( ui.main_mode & ( 1 << x ) )
                Graphic_Rectangle( 0+x*4, 14, 2+x*4, 15 );
            else
                Graphic_PutPixel( 1+x*4, 15, 1 );
        }
    }
    else
        Graphic_Line( 0, 15, 127, 15 );

    x = 0;

    if ( opmode & OPMODE_CABLE )
    {
        uibm_put_bitmap( x, 1, BMP_ICON_CABLEREL );
        x+= 14;
    }
    if ( opmode & OPMODE_LONGEXPO )
    {
        uibm_put_bitmap( x, 1, BMP_ICON_LONGEXPO );
        x+= 14;
    }
    if ( opmode & OPMODE_SEQUENTIAL )
    {
        uibm_put_bitmap( x, 1, BMP_ICON_SEQUENTIAL );
        x+= 14;
    }
    if ( opmode & OPMODE_TIMER )
    {
        uibm_put_bitmap( x, 1, BMP_ICON_TIMER );
        x+= 14;
    }
    if ( opmode & OPMODE_LIGHT )
    {
        uibm_put_bitmap( x, 1, BMP_ICON_LIGHTNING );
        x+= 14;
    }
    if ( opmode & OPMODE_SOUND )
    {
        uibm_put_bitmap( x, 1, BMP_ICON_SOUND );
        x+= 14;
    }
    if ( core->setup.mlockup && (ui.m_state != UI_STATE_MENU) )
    {
        uibm_put_bitmap( x, 1, BMP_ICON_MLOCKUP );
        x+= 14;
    }

    if ( ui.m_state == UI_STATE_STARTED )
    {
        timestruct tm;
        uint32 i;

        if ( rdrw_all )
        {
            // first time
            Graphic_SetColor(1);
            Graphic_FillRectangle( x + 1, 0, 116, 15, 1);
            Graphic_PutPixel( x + 1, 0, 0 );
            Graphic_PutPixel( 116, 0, 0 );
            if ( x < 70 )
                uibm_put_bitmap( x+2, 1, BMP_ICON_STARTED );
        }
        if ( core->op.op_state )
        {
            // in run
            tm.hour = core->op.op_time_left / 3600;
            i = core->op.op_time_left % 3600;
            tm.minute = i / 60;
            tm.second = i % 60;
            uigrf_puttime( 76, 4, uitxt_small, 0, tm, false, false );
        }
    }
}

static void uist_internal_sbarset(int redraw)
{
    uigrf_draw_battery( 120, 0, core->measure.battery );

    if ( (redraw & RDRW_STATUSBAR) == RDRW_BATTERY )
        return;

    Graphic_Line( 0, 15, 127, 15 );

    switch ( ui.m_setstate )
    {
        case UI_SSET_PRETRIGGER:    uigrf_text( 4, 4, uitxt_smallbold, "Pretrigger:" ); break;
        case UI_SSET_SHUTTER:       uigrf_text( 4, 4, uitxt_smallbold, "Shutter:" ); break;
        case UI_SSET_COMBINE:       uigrf_text( 4, 4, uitxt_smallbold, "Combine:" ); break;
        case UI_SSET_DISPBRT:       uigrf_text( 4, 4, uitxt_smallbold, "Brightness:" ); break;
        case UI_SSET_PWRSAVE:       uigrf_text( 4, 4, uitxt_smallbold, "Power Save:" ); break;
        case UI_SSET_RESET:         uigrf_text( 4, 4, uitxt_smallbold, "Reset:" ); break;
    }
    Graphic_FillRectangle( 0, 0, 116, 15, -1);
    Graphic_PutPixel( 0, 0, 0 );
    Graphic_PutPixel( 116, 0, 0 );
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



////////////////////////////////////////////////////
//
//   UI status dependent redraw routines
//
////////////////////////////////////////////////////

static inline void uist_window_draw_cablerelease( int redraw_all )
{
    if ( redraw_all == RDRW_ALL )
    {
        if ( (ui.m_state == UI_STATE_MAIN_WINDOW) )
        {
            uigrf_text( 14, 32, uitxt_small,  "brief press" );
            uigrf_text( 56, 32, uitxt_smallbold,  "S" );
            uigrf_text( 64, 32, uitxt_small,  "for singleshot" );
            uigrf_text( 14, 43, uitxt_small,  "long press for bulb" );
        }
        Graphic_Rectangle( 10, 29, 118, 56 );
        uigrf_text( 40, 17, uitxt_small,  "Prefocus:" );

    }
    if ( (ui.m_state == UI_STATE_STARTED) && (core->op.op_state == OPSTATE_CABLEBULB) )
    {
        ui_element_display( &ui.p.cable.time, false );
    }
    ui_element_display( &ui.p.cable.prefocus, ui.focus );
}


static inline void uist_window_draw_longexpo( int redraw_all )
{
    if ( redraw_all == RDRW_ALL )
    {
        uigrf_text( 8, 30, uitxt_small,  "Long" );
        uigrf_text( 8, 40, uitxt_small,  "expo:");
    }
    ui_element_display( &ui.p.longexpo.time, ui.focus );
}

static inline void uist_window_draw_sequential( int redraw_all )
{
    if ( redraw_all == RDRW_ALL )
    {
        uigrf_text( 18, 20, uitxt_small,  "shot nr:" );
        uigrf_text( 77, 20, uitxt_small,  "interval:" );
    }
    if ( (core->op.seq_shotnr == 0) && (ui.focus == 0) )
    {
        uigrf_text( 8, 30, uitxt_small,  "infinite" );
        uigrf_text( 8, 40, uitxt_small,  "shots" );
    }
    else
    {
        uist_internal_disp_with_focus( 0 );
    }
    uist_internal_disp_with_focus( 1 );
}

static inline void uist_window_draw_timer( int redraw_all )
{
    if ( redraw_all == RDRW_ALL )
    {
        uigrf_text( 42, 20, uitxt_small,  "start timer:" );
    }
    ui_element_display( &ui.p.timer.time, ui.focus );
}


static void uist_window_draw_scale( bool centered, bool redraw, int vmax, int vmin, int thold )
{


    uigrf_draw_scale( 46, thold, vmax, vmin, centered, redraw );
}




static inline void uist_window_draw_lightsound( int redraw )
{
    if ( redraw == RDRW_ALL )
    {
        if ( ui.main_mode == OPMODE_LIGHT )
        {
            uigrf_text( 2, 19, uitxt_small,  "trig:" );
            uigrf_text( 2, 31, uitxt_small,  "cond:" );
            uibm_put_bitmap( 20, 16, BMP_ICON_TR_LIGHT );
            uibm_put_bitmap( 45, 16, BMP_ICON_TR_SHADE );
            uibm_put_bitmap( 20, 29, BMP_ICON_BRIGHT );
        }
        uigrf_text( 75, 19, uitxt_small,  "gain:" );
        uigrf_text( 75, 31, uitxt_small,  "thold:" );
        Gtext_SetCoordinates( 120, 19 );
        Gtext_PutChar('%');
        Gtext_SetCoordinates( 120, 31 );
        Gtext_PutChar('%');
    }
    if ( redraw & RDRW_UI_DYNAMIC )
    {
        int vmax;
        int vmin;
        core_analog_get_minmax( &vmax, &vmin );
        
        if ( ui.main_mode == OPMODE_LIGHT )
            uist_window_draw_scale( true, (redraw & RDRW_UI_CONTENT), vmax, vmin, core->setup.light.thold );    // for light sensor
        else
            uist_window_draw_scale( false, (redraw & RDRW_UI_CONTENT), vmax, 0xff, core->setup.sound.thold );   // for microphone
    }
    if ( redraw & RDRW_UI_CONTENT )
        uist_internal_disp_all_with_focus();
}

static inline void uist_setwindow_draw_pretrigger( int redraw_all )
{
    if ( redraw_all == RDRW_ALL )
    {
        uigrf_text( 8, 20, uitxt_small,  "set the delay bw. event" );
        uigrf_text( 8, 30, uitxt_small,  "and shutter trigger:" );
        uigrf_text( 82, 47, uitxt_small,  "ms" );
    }
    ui_element_display( &ui.p.set_pretrigger, true );
}


static inline void uist_setwindow_draw_combine( int redraw_all )
{
    int i;

    if ( redraw_all == RDRW_ALL )
    {
        const int bmp_list[] = { BMP_ICON_LONGEXPO, BMP_ICON_SEQUENTIAL, BMP_ICON_TIMER, BMP_ICON_LIGHTNING, BMP_ICON_SOUND };

        uigrf_text( 8, 17, uitxt_small, "Select the operation modes:" );

        for ( i=0; i<5; i++ )
        {
            uibm_put_bitmap( 8 + 25*i, 30, bmp_list[i] );
        }
    }

    uist_internal_disp_all_with_focus();
}


static inline void uist_setwindow_draw_shutter( int redraw_all )
{
    if ( redraw_all == RDRW_ALL )
    {
        uigrf_text( 30,  24, uitxt_small, "Mirror lockup" );
        uigrf_text( 43,  38, uitxt_small, "hold time:" );
        uigrf_text( 105, 38, uitxt_small, "sec." );
        uigrf_text( 33,  52, uitxt_small, "pulse lenght:" );
        uigrf_text( 105, 52, uitxt_small, "ms." );
    }

    uist_internal_disp_all_with_focus();
}

static inline void uist_setwindow_draw_dispbrt( int redraw_all )
{
    if ( redraw_all == RDRW_ALL )
    {
        uigrf_text( 33, 23, uitxt_small, "display on:" );
        uigrf_text( 20, 35, uitxt_small, "dimmed state:" );
        uigrf_text( 44, 47, uitxt_small, "beep:" );
    }
    uist_internal_disp_all_with_focus();
}


static inline void uist_setwindow_draw_powersave( int redraw_all )
{
    if ( redraw_all == RDRW_ALL )
    {
        uigrf_text( 39, 23, uitxt_small, "standby:" );
        uigrf_text( 30, 35, uitxt_small, "display off:" );
        uigrf_text( 31, 47, uitxt_small, "power off:" );
    }
    uist_internal_disp_all_with_focus();
}

static inline void uist_setwindow_draw_reset( int redraw_all )
{
    if ( redraw_all == RDRW_ALL )
    {
        uigrf_text( 24, 16, uitxt_small, "Are you sure to reset" );
        uigrf_text( 34, 26, uitxt_small, "all the settings?" );
        uigrf_text( 33, 40, uitxt_small, "No" );
        uigrf_text( 85, 40, uitxt_small, "Yes" );
    }
    uist_internal_disp_all_with_focus();
}

////////////////////////////////////////////////////
//
//   UI status dependent window setups
//
////////////////////////////////////////////////////

static inline void uist_window_setup_cable( void )
{
    uiel_control_checkbox_init( &ui.p.cable.prefocus, 80, 16 );
    uiel_control_time_init( &ui.p.cable.time, 26, 30, false, uitxt_large_num );
    ui.ui_elem_nr = 1;
    ui.ui_elems[0] = &ui.p.cable.prefocus;
}

static inline void uist_window_setup_longexpo( bool reset )
{
    if ( reset )
        core_reset_longexpo();
    uiel_control_time_init( &ui.p.longexpo.time, 38, 26, false, uitxt_large_num );
    uiel_control_time_set_time( &ui.p.longexpo.time, core->op.time_longexpo );
    ui.ui_elem_nr = 1;
    ui.ui_elems[0] = &ui.p.longexpo.time;
}

static inline void uist_window_setup_sequential( bool reset )
{
    if ( reset )
        core_reset_sequential();
    uiel_control_edit_init( &ui.p.interval.shot_nr, 10, 33, uitxt_large_num, uiedit_numeric, 4, 0 );
    uiel_control_edit_set_num( &ui.p.interval.shot_nr, core->op.seq_shotnr );
    uiel_control_time_init( &ui.p.interval.iv, 70, 33, true, uitxt_large_num );
    {
        timestruct time;
        time.hour = 0;
        time.minute = core->op.seq_interval / 60;
        time.second = core->op.seq_interval % 60;
        uiel_control_time_set_time( &ui.p.interval.iv, time );
    }
    ui.ui_elem_nr = 2;
    ui.ui_elems[0] = &ui.p.interval.shot_nr;
    ui.ui_elems[1] = &ui.p.interval.iv;
}

static inline void uist_window_setup_timer( bool reset )
{
    if ( reset )
        core_reset_timer();
    uiel_control_time_init( &ui.p.timer.time, 38, 33, true, uitxt_large_num );
    {
        timestruct time;
        time.hour = 0;
        time.minute = core->op.tmr_interval / 60;
        time.second = core->op.tmr_interval % 60;
        uiel_control_time_set_time( &ui.p.timer.time, time );
    }
    ui.ui_elem_nr = 1;
    ui.ui_elems[0] = &ui.p.timer.time;
}

static inline void uist_window_setup_lightsound( void )
{
    int i = 0;
    uiel_control_numeric_init( &ui.p.light.gain, 5, 100, 5, 96, 17, 3, 0x00, uitxt_smallbold );
    uiel_control_numeric_init( &ui.p.light.thold, 1, 50, 1, 96, 29, 3, 0x00, uitxt_smallbold );
    if ( ui.main_mode == OPMODE_LIGHT )
    {
        uiel_control_checkbox_init( &ui.p.light.trigH, 33, 18 );
        uiel_control_checkbox_init( &ui.p.light.trigL, 58, 18 );
        uiel_control_checkbox_init( &ui.p.light.bright, 33, 30 );
        uiel_control_checkbox_set( &ui.p.light.trigH, core->setup.light.trig_h );
        uiel_control_checkbox_set( &ui.p.light.trigL, core->setup.light.trig_l );
        uiel_control_checkbox_set( &ui.p.light.bright, core->setup.light.bright );
        uiel_control_numeric_set( &ui.p.light.gain, core->setup.light.gain );
        uiel_control_numeric_set( &ui.p.light.thold, core->setup.light.thold );
        ui.ui_elems[i++] = &ui.p.light.trigH;
        ui.ui_elems[i++] = &ui.p.light.trigL;
        ui.ui_elems[i++] = &ui.p.light.bright;
    }
    else
    {
        uiel_control_numeric_set( &ui.p.light.gain, core->setup.sound.gain );
        uiel_control_numeric_set( &ui.p.light.thold, core->setup.sound.thold );
    }
    ui.ui_elems[i++] = &ui.p.light.gain;
    ui.ui_elems[i++] = &ui.p.light.thold;
    ui.ui_elem_nr = i;
}


static inline void uist_setwindow_setup_pretrigger(void)
{
    uiel_control_edit_init( &ui.p.set_pretrigger, 47, 45, uitxt_smallbold, uiedit_numeric, 4, 1 );
    if ( ui.main_mode == OPMODE_LIGHT )
        uiel_control_edit_set_num( &ui.p.set_pretrigger, core->setup.light.iv_pretrig );
    else
        uiel_control_edit_set_num( &ui.p.set_pretrigger, core->setup.sound.iv_pretrig );

    ui.ui_elems[0]  = &ui.p.set_pretrigger;
    ui.ui_elem_nr = 1;
    ui.focus = 1;
}


static inline void uist_setwindow_setup_combine(void)
{
    int i;

    for (i=0; i<5; i++)
    {
        uiel_control_checkbox_init( &ui.p.set_combine.mode[i],  10 + 25*i, 47 );
        uiel_control_checkbox_set( &ui.p.set_combine.mode[i], core->op.opmode & ( 1 << (i+1)) );        // i+1 because we don't combine OPMODE_CABLE with anything
        ui.ui_elems[i] = &ui.p.set_combine.mode[i];
    }
    ui.ui_elem_nr = 5;
    ui.focus = 1;
}


static inline void uist_setwindow_setup_shutter(void)
{
    uiel_control_checkbox_init( &ui.p.set_shutter.mlock_en,  82, 22 );
    uiel_control_checkbox_set( &ui.p.set_shutter.mlock_en, core->setup.mlockup ? true : false );
    uiel_control_numeric_init( &ui.p.set_shutter.mlock, 0, 10, 1, 80, 36, 2, ' ', uitxt_smallbold );
    uiel_control_numeric_set( &ui.p.set_shutter.mlock, core->setup.mlockup );
    uiel_control_numeric_init( &ui.p.set_shutter.shtrp, 20, 500, 20, 80, 50, 3, ' ', uitxt_smallbold );
    uiel_control_numeric_set( &ui.p.set_shutter.shtrp, core->setup.shtr_pulse * 10 );

    ui.ui_elems[0] = &ui.p.set_shutter.mlock_en;
    ui.ui_elems[1] = &ui.p.set_shutter.mlock;
    ui.ui_elems[2] = &ui.p.set_shutter.shtrp;
    ui.ui_elem_nr = 3;
    ui.focus = 1;
}


static inline void uist_setwindow_setup_dispbrt(void)
{
    uiel_control_numeric_init( &ui.p.set_brightness.full, 0, 64, 1, 73, 21, 2, '0', uitxt_smallbold | uitxt_MONO );
    uiel_control_numeric_set( &ui.p.set_brightness.full, core->setup.disp_brt_on );
    uiel_control_numeric_init( &ui.p.set_brightness.dimmed, 0, 64, 1, 73, 33, 2, '0', uitxt_smallbold | uitxt_MONO );
    uiel_control_numeric_set( &ui.p.set_brightness.dimmed, core->setup.disp_brt_dim );
    uiel_control_checkbox_init( &ui.p.set_brightness.en_beep, 73, 45 );
    uiel_control_checkbox_set( &ui.p.set_brightness.en_beep, core->setup.beep_on );
    ui.ui_elems[0] = &ui.p.set_brightness.full;
    ui.ui_elems[1] = &ui.p.set_brightness.dimmed;
    ui.ui_elems[2] = &ui.p.set_brightness.en_beep;

    ui.ui_elem_nr = 3;
    ui.focus = 1;
}

static inline void uist_setwindow_setup_powersave(void)
{
    int i;
    int values[3];
    uiel_control_list_init( &ui.p.set_powersave.stby, 80, 21, 32, uitxt_smallbold );
    uiel_control_list_init( &ui.p.set_powersave.dispoff, 80, 33, 32, uitxt_smallbold );
    uiel_control_list_init( &ui.p.set_powersave.pwroff, 80, 45, 32, uitxt_smallbold );

    ui.ui_elems[0] = &ui.p.set_powersave.stby;
    ui.ui_elems[1] = &ui.p.set_powersave.dispoff;
    ui.ui_elems[2] = &ui.p.set_powersave.pwroff;
    values[0] = core->setup.pwr_stdby;
    values[1] = core->setup.pwr_disp_off;
    values[2] = core->setup.pwr_off;

    for (i=0; i<3; i++)
    {
        int idx = 0;
        uiel_control_list_add_item( ui.ui_elems[i], "15s", 15 );
        uiel_control_list_add_item( ui.ui_elems[i], "30s", 30 );
        uiel_control_list_add_item( ui.ui_elems[i], "1m", 60 );
        uiel_control_list_add_item( ui.ui_elems[i], "5m", 300 );
        uiel_control_list_add_item( ui.ui_elems[i], "30m", 1800 );
        uiel_control_list_add_item( ui.ui_elems[i], "1h", 3600 );
        uiel_control_list_add_item( ui.ui_elems[i], "2h", 7200 );
        uiel_control_list_add_item( ui.ui_elems[i], "never", 0 );
        switch ( values[i] )
        {
            case 15:     idx = 0; break;
            case 30:     idx = 1; break;
            case 60:     idx = 2; break;
            case 300:    idx = 3; break;
            case 1800:   idx = 4; break;
            case 3600:   idx = 5; break;
            case 7200:   idx = 6; break;
            case 0:      idx = 7; break;
        }
        uiel_control_list_set_index( ui.ui_elems[i], idx );
    }
    ui.ui_elem_nr = 3;
    ui.focus = 1;
}

static inline void uist_setwindow_setup_default(void)
{
    uiel_control_checkbox_init( &ui.p.set_default.no,  33, 52 );
    uiel_control_checkbox_set( &ui.p.set_default.no, false );
    uiel_control_checkbox_init( &ui.p.set_default.yes,  86, 52 );
    uiel_control_checkbox_set( &ui.p.set_default.yes, false );
    ui.ui_elems[0] = &ui.p.set_default.no;
    ui.ui_elems[1] = &ui.p.set_default.yes;
    ui.ui_elem_nr = 2;
    ui.focus = 1;
}


// --------------------------------------------------------------------

static void uist_mainwindow_internal_draw( int redraw_type )
{
    // if all the content should be redrawn
    if ( redraw_type == RDRW_ALL )
        Graphics_ClearScreen(0);

    // if status bar should be redrawn
    if ( redraw_type & RDRW_STATUSBAR )
    {
        if ( (redraw_type & RDRW_STATUSBAR) == RDRW_BATTERY )       // only battery should be redrawn
            uist_internal_statusbar( 0, (redraw_type == RDRW_ALL) );
        else                                                        // else redraw the whole status bar
            uist_internal_statusbar( (core->op.opmode & OPMODE_COMBINED) ? core->op.opmode : ui.main_mode, (redraw_type == RDRW_ALL) );
    }

    // if UI content should be redrawn
    if ( redraw_type & (~RDRW_STATUSBAR) )                          // if there are other things to be redrawn from the UI
    {
        switch ( ui.main_mode )
        {
            case OPMODE_CABLE:      uist_window_draw_cablerelease(redraw_type);  break;
            case OPMODE_LONGEXPO:   uist_window_draw_longexpo(redraw_type);      break;
            case OPMODE_SEQUENTIAL: uist_window_draw_sequential(redraw_type);    break;
            case OPMODE_TIMER:      uist_window_draw_timer(redraw_type);         break;
            case OPMODE_LIGHT:
            case OPMODE_SOUND:      uist_window_draw_lightsound(redraw_type);    break;
            break;
        }
    }
}


void uist_mainwindow_setup_view( bool reset )
{
    ui.focus = 0;

    // main windows
    switch ( ui.main_mode )
    {
        case OPMODE_CABLE:      uist_window_setup_cable();             break;
        case OPMODE_LONGEXPO:   uist_window_setup_longexpo( reset );   break;
        case OPMODE_SEQUENTIAL: uist_window_setup_sequential( reset ); break;
        case OPMODE_TIMER:      uist_window_setup_timer( reset );      break;
        case OPMODE_LIGHT:
        case OPMODE_SOUND:      uist_window_setup_lightsound();        break;
    }
}




static void uist_setwindow_internal_draw( int redraw )
{
    if ( redraw == RDRW_ALL )
        Graphics_ClearScreen(0);

    if ( redraw & RDRW_STATUSBAR )
    {
        uist_internal_sbarset(redraw);
    }

    if ( redraw & (~RDRW_STATUSBAR) )
    {
        switch ( ui.m_setstate )
        {
            case UI_SSET_PRETRIGGER:   uist_setwindow_draw_pretrigger(redraw);  break;
            case UI_SSET_COMBINE:      uist_setwindow_draw_combine(redraw);     break;
            case UI_SSET_SHUTTER:      uist_setwindow_draw_shutter(redraw);     break;
            case UI_SSET_DISPBRT:      uist_setwindow_draw_dispbrt(redraw);     break;
            case UI_SSET_PWRSAVE:      uist_setwindow_draw_powersave(redraw);   break;
            case UI_SSET_RESET:        uist_setwindow_draw_reset(redraw);       break;
            break;
        }
    }
}



void uist_setwindow_setup_view( void )
{
    ui.focus = 0;

    // set. windows
    switch ( ui.m_setstate )
    {
        case UI_SSET_PRETRIGGER:   uist_setwindow_setup_pretrigger();  break;
        case UI_SSET_COMBINE:      uist_setwindow_setup_combine();     break;
        case UI_SSET_SHUTTER:      uist_setwindow_setup_shutter();     break;
        case UI_SSET_DISPBRT:      uist_setwindow_setup_dispbrt();     break;
        case UI_SSET_PWRSAVE:      uist_setwindow_setup_powersave();   break;   
        case UI_SSET_RESET:        uist_setwindow_setup_default();       break;
    }
}

// --------------------------------------------------------------------
// scope window

void uist_scope_internal_draw( int redraw )
{
    struct SADC *adc;
    adc = (struct SADC*)core_analog_getbuffer();
    
    if ( redraw == RDRW_ALL )
    {
        Graphics_ClearScreen(0);

        // status bar
        if ( ui.main_mode & OPMODE_LIGHT )
            uigrf_text( 1, 4, uitxt_smallbold,  "Light" );
        else
            uigrf_text( 1, 4, uitxt_smallbold,  "Sound" );
        Graphic_SetColor(-1);
        Graphic_FillRectangle( 0, 0, 30, 14, -1 );
        Graphic_PutPixel(0,0,0);
        Graphic_PutPixel(30,0,0);
        Graphic_SetColor(1);
        Graphic_Line(0,15,127,15);
        Graphic_Line(50,0,50,15);

        if ( ui.p.scope.scope_mode == UISM_VALUELIST )
        {
            // value list mode
            uigrf_text( 54, 4, uitxt_small, "Battery:" );
            uigrf_text( 0, 28, uitxt_small,  "Low:" );
            uigrf_text( 0, 40, uitxt_small,  "High:" );
            uigrf_text( 28, 16, uitxt_small,  "min" );
            uigrf_text( 56, 16, uitxt_small,  "val" );
            uigrf_text( 84, 16, uitxt_small,  "max" );
            uigrf_text( 0, 52, uitxt_small,  "Base:" );
            uigrf_text( 56, 52, uitxt_small,  "Value:" );
            uigrf_text( 105, 16, uitxt_small,  "D" );
        }
        else
        {
            // scope mode
            Graphic_Line(59,0,59,15);
            Graphic_Line(83,0,83,15);
            Graphic_Line(107,0,107,15);
            if ( ui.p.scope.scope_mode == UISM_SCOPE_LOW )
                uigrf_text( 33, 4, uitxt_small, "LOW" );
            else 
                uigrf_text( 32, 4, uitxt_small, "HiGH" );

            uigrf_text( 61, 4, uitxt_small, "G:" );
            uigrf_text( 85, 4, uitxt_small, "T:" );
            uigrf_text( 109, 4, uitxt_small, "S:" );

        }
    }

    // -- for scope display
    if ( ui.p.scope.scope_mode != UISM_VALUELIST )
    {
        if ( redraw & RDRW_UI_CONTENT )
        {
            if ( ui.p.scope.adj_mode == UISA_SAMPLERATE )
                uigrf_text( 53, 4, uitxt_small, "S" );
            else if ( ui.p.scope.adj_mode == UISA_TRIGGER )
                uigrf_text( 53, 4, uitxt_small, "T" );
            if ( ui.p.scope.adj_mode == UISA_GAIN )
                uigrf_text( 53, 4, uitxt_small, "G" );

            Graphic_SetColor(0);
            Graphic_FillRectangle(68, 4, 82, 14, 0);
            Graphic_FillRectangle(92, 4, 106, 14, 0);
            Graphic_FillRectangle(116, 4, 127, 14, 0);
            Graphic_FillRectangle(0, 16, 2, 63, 0);
            uigrf_putnr( 68, 4, uitxt_small, ui.p.scope.osc_gain, 3, 0 );
            uigrf_putnr( 93, 4, uitxt_small, ui.p.scope.osc_trigger, 3, 0 );
            if ( ui.p.scope.tmr_run )
                uigrf_putnr( 116, 4, uitxt_small, ui.p.scope.tmr_pre, 2, 0 );
            else
                uigrf_text( 116, 4, uitxt_small, "St" );

            // draw trigger indicator
            {
                int y, y1, y2;
                if ( ui.main_mode & OPMODE_LIGHT )
                    y = 40 - ( ((uint32)ui.p.scope.osc_trigger * 24 ) >> 6 ); 
                else
                    y = 63 - ( ((uint32)ui.p.scope.osc_trigger * 48 ) >> 6 ); 

                if ( y == 16 )
                {
                    y1 = 16;
                    y2 = 17;
                }
                else if ( y == 63 )
                {
                    y1 = 62;
                    y2 = 63;
                }
                else
                {
                    y1 = y-1;
                    y2 = y+1;
                }

                Graphic_SetColor(1);
                Graphic_PutPixel( 1, y, 1);
                Graphic_Line( 0, y1, 0, y2 );
            }
        }
    }
    // -- for value list
    else    
    {
        if ( (redraw & RDRW_STATUSBAR) )
        {
            switch ( ui.p.scope.disp_unit )
            {
                case UISCOPE_DEC: uigrf_text( 33, 4, uitxt_small, "DEC" ); break;
                case UISCOPE_HEX: uigrf_text( 33, 4, uitxt_small, "HEX" ); break;
                case UISCOPE_UNIT: uigrf_text( 34, 4, uitxt_small, "Unit" ); break;
            }
        }

        if ( redraw & RDRW_BATTERY )
        {
            Graphic_SetColor(0);
            Graphic_FillRectangle(90, 4, 120, 14, 0);
            switch ( ui.p.scope.disp_unit )
            {
                case UISCOPE_DEC: uigrf_putnr( 90, 4, uitxt_small, adc->samples[0], 4, 0 ); break;
                case UISCOPE_HEX: uigrf_puthex( 90, 4, uitxt_small, adc->samples[0], 3, 0 ); break;
                case UISCOPE_UNIT: 
                    core_update_battery();
                    uigrf_putnr( 90, 4, uitxt_small, core->measure.battery, 4, 0 );
                    break;
            }
        }

        if ( redraw & RDRW_UI_CONTENT )
        {
            uigrf_putnr( 110, 16, uitxt_small, ui.p.scope.tmr_pre, 2, '0' );
        }

        if ( redraw & RDRW_UI_DYNAMIC )
        {
            uint16 *raws = ui.p.scope.raws;
            if ( redraw != RDRW_ALL )
            {
                Graphic_SetColor(0);
                Graphic_FillRectangle(20, 28, 120, 63, 0);
            }
    
            if ( ui.p.scope.disp_unit == UISCOPE_HEX )
            {
                uigrf_puthex( 22, 28, uitxt_small, raws[2], 3, '0' );       // low min
                uigrf_puthex( 50, 28, uitxt_small, raws[0], 3, '0' );       // low
                uigrf_puthex( 78, 28, uitxt_small, raws[4], 3, '0' );       // low max
                uigrf_puthex( 22, 40, uitxt_small, raws[3], 3, '0' );       // high min
                uigrf_puthex( 50, 40, uitxt_small, raws[1], 3, '0' );       // high
                uigrf_puthex( 78, 40, uitxt_small, raws[5], 3, '0' );       // high max
    
                uigrf_puthex( 22, 52, uitxt_small, raws[6], 3, '0' );       // base
            }
            else if ( ui.p.scope.disp_unit == UISCOPE_DEC )
            {
                uigrf_putnr( 22, 28, uitxt_small, raws[2], 4, '0' );      // low min
                uigrf_putnr( 50, 28, uitxt_small, raws[0], 4, '0' );      // low
                uigrf_putnr( 78, 28, uitxt_small, raws[4], 4, '0' );      // low max
                uigrf_putnr( 22, 40, uitxt_small, raws[3], 4, '0' );      // high min
                uigrf_putnr( 50, 40, uitxt_small, raws[1], 4, '0' );      // high
                uigrf_putnr( 78, 40, uitxt_small, raws[5], 4, '0' );      // high max
    
                uigrf_putnr( 22, 52, uitxt_small, raws[6], 4, '0' );       // base
            }
            else
            {
                uigrf_putfixpoint( 22, 28, uitxt_small, (2500*raws[2])>>12, 4, 3, '0' );      // low min
                uigrf_putfixpoint( 50, 28, uitxt_small, (2500*raws[0])>>12, 4, 3, '0' );      // low
                uigrf_putfixpoint( 78, 28, uitxt_small, (2500*raws[4])>>12, 4, 3, '0' );      // low max
                uigrf_putfixpoint( 22, 40, uitxt_small, (2500*raws[3])>>12, 4, 3, '0' );      // high min
                uigrf_putfixpoint( 50, 40, uitxt_small, (2500*raws[1])>>12, 4, 3, '0' );      // high
                uigrf_putfixpoint( 78, 40, uitxt_small, (2500*raws[5])>>12, 4, 3, '0' );      // high max

                uigrf_putfixpoint( 22, 52, uitxt_small, (2500*raws[6])>>12, 4, 3, '0' );      // base
            }
    
            uigrf_putnr( 81, 52, uitxt_small, adc->value, 4, 0x00 );       // value
    
            Graphic_SetColor(1);
            Graphic_Line( 20, 36, 20+( ((uint32)raws[0]*100) >> 12 ), 36 );     // low gain graph
            Graphic_Line( 20, 48, 20+( ((uint32)raws[1]*100) >> 12 ), 48 );     // high gain graph
            Graphic_Line( 20, 60, 20+( ((uint32)raws[6]*100) >> 12 ), 60 );     // base
            Graphic_Line( 70, 62, 70+( adc->value / 2 ), 62 );                  // val
        }
    }
}


static inline void uist_scope_internal_trace(void)
{
    int y1, y2;
    int vmax;
    int vmin;
    int base;

    base = ui.p.scope.raws[6];
    if ( ui.p.scope.scope_mode == UISM_SCOPE_LOW )      // low gain values
    {
        vmin = ui.p.scope.raws[2] - base;
        vmax = ui.p.scope.raws[4] - base;
    }
    else                                                // high gain values
    {
        vmin = ui.p.scope.raws[3] - base;
        vmax = ui.p.scope.raws[5] - base;
    }

    if ( ui.main_mode & OPMODE_LIGHT )
    {
        y1 = 40 - ((vmax * ui.p.scope.osc_gain * 24) / 4096 );
        y2 = 40 - ((vmin * ui.p.scope.osc_gain * 24) / 4096 );
    }
    else
    {
        y1 = 63 - ((vmax * ui.p.scope.osc_gain * 48) / 4096 );
        y2 = 63 - ((vmin * ui.p.scope.osc_gain * 48) / 4096 );
    }

    if ( y1 > 63 )
        y1 = 63;
    if ( y1 < 16 )
        y1 = 16;
    if ( y2 > 63 )
        y2 = 63;
    if ( y2 < 16 )
        y2 = 16;

    Graphic_ShiftH( 2, 16, 127, 63, 1 );

    Graphic_SetColor(0);
    Graphic_Rectangle(127, 16, 127, 63);
    Graphic_SetColor(1);
    Graphic_Rectangle(127, y1, 127, y2);
}

////////////////////////////////////////////////////
//
//   UI status dependent apply set value routines
//
////////////////////////////////////////////////////

bool uist_interval_param_validate(void)
{
    bool modified = false;

    if ( (core->op.opmode & OPMODE_SEQUENTIAL) == 0 )   // if interval isn't used then do nothing
        return false;

    // interval should be larger with 1 second than mirror locup
    if ( core->setup.seq_interval <= core->setup.mlockup )
    {
        core->setup.seq_interval = core->setup.mlockup + 1;
        modified = true;
    }

    // if longexpo is in use 
    if ( core->op.opmode & OPMODE_LONGEXPO )
    {
        // limit the longexpo time to fit in interval
        if ( (core->setup.time_longexpo.hour * 60 + core->setup.time_longexpo.minute ) > ( 99 - core->setup.mlockup - 1 ) )
        {
            core->setup.time_longexpo.hour = ( 99 - core->setup.mlockup - 1 ) / 60;
            core->setup.time_longexpo.minute = ( 99 - core->setup.mlockup - 1 ) % 60;
            modified = true;
        }
        // update the interval if needed
        if ( core->setup.seq_interval <= (core->setup.time_longexpo.hour * 3600 + core->setup.time_longexpo.minute *60 + core->setup.time_longexpo.second + core->setup.mlockup ) )
        {
            core->setup.seq_interval = core->setup.time_longexpo.hour * 3600 + core->setup.time_longexpo.minute *60 + core->setup.time_longexpo.second + core->setup.mlockup + 1;
            modified = true;
        }
    }

    // if it was on cable release page and mode combination is set up - skip to the next page (cable release should work only standalone)
    if ( (core->op.opmode & OPMODE_COMBINED) && (ui.main_mode == OPMODE_CABLE) )
    {
        ui.main_mode = OPMODE_LONGEXPO;
    }

    return modified;
}



bool uist_mainwindow_apply_values(void)
{
    switch ( ui.main_mode )
    {
        case OPMODE_LONGEXPO:
            {
                timestruct tm;
                uiel_control_time_get_time( &ui.p.longexpo.time, &tm );
                if ( (tm.hour + tm.minute + tm.second) == 0 )
                {
                    tm.second = 1;      // need to be at least one second
                    uiel_control_time_set_time( &ui.p.longexpo.time, tm );
                }
                if ( memcmp(&tm, &core->setup.time_longexpo, sizeof(tm)) )
                {
                    core->setup.time_longexpo = tm;
                    ui.setup_mod = true;
                }
                ui.setup_mod |= uist_interval_param_validate();
            }
            break;
        case OPMODE_SEQUENTIAL:
            {
                uint32 i;
                timestruct tm;
                i = uiel_control_edit_get_num( &ui.p.interval.shot_nr );
                uiel_control_time_get_time( &ui.p.interval.iv, &tm );
                if ( (tm.hour + tm.minute + tm.second) == 0 )
                {
                    tm.second = 1;      // need to be at least one second
                    uiel_control_time_set_time( &ui.p.interval.iv, tm );
                }
                if ( memcmp(&tm, &core->setup.seq_interval, sizeof(tm)) ||
                     (core->setup.seq_shotnr != i) )
                {
                    core->setup.seq_interval = tm.minute * 60 + tm.second;
                    core->setup.seq_shotnr = i;
                    ui.setup_mod = true;
                }
                ui.setup_mod |= uist_interval_param_validate();
            }
            break;
        case OPMODE_TIMER:
            {
                timestruct tm;
                uiel_control_time_get_time( &ui.p.timer.time, &tm );
                if ( (tm.hour + tm.minute + tm.second) == 0 )
                {
                    tm.second = 1;      // need to be at least one second
                    uiel_control_time_set_time( &ui.p.timer.time, tm );
                }
                if ( memcmp(&tm, &core->setup.tmr_interval, sizeof(tm)) )
                {
                    core->setup.tmr_interval = tm.minute * 60 + tm.second;
                    ui.setup_mod = true;
                }
            }
            break;
        case OPMODE_LIGHT:
            {
                int trh = uiel_control_checkbox_get( &ui.p.light.trigH );
                int trl = uiel_control_checkbox_get( &ui.p.light.trigL );
                bool br = uiel_control_checkbox_get( &ui.p.light.bright );
                int gn  = uiel_control_numeric_get( &ui.p.light.gain );
                int th  = uiel_control_numeric_get( &ui.p.light.thold );
                if ( (th != core->setup.light.thold) || 
                     (gn != core->setup.light.gain) || 
                     (trh != core->setup.light.trig_h) || 
                     (trl != core->setup.light.trig_l) ||
                     (br != core->setup.light.bright) )
                {
                    core->setup.light.thold = th;
                    core->setup.light.gain = gn;
                    core->setup.light.trig_h = trh;
                    core->setup.light.trig_l = trl;
                    core->setup.light.bright = br;
                    ui.setup_mod = true;
                    return true;
                }
            }
            break;
        case OPMODE_SOUND:
            {
                int gn  = uiel_control_numeric_get( &ui.p.light.gain );     // 'light' is not a mistake - ui uses the same structure
                int th  = uiel_control_numeric_get( &ui.p.light.thold );
                if ( (th != core->setup.sound.thold) || (gn != core->setup.sound.gain) )
                {
                    core->setup.sound.thold = th;
                    core->setup.sound.gain = gn;
                    ui.setup_mod = true;
                    return true;
                }
            }
            break;
    }

    return false;
}



void uist_setwindow_apply_values(void)
{
    switch ( ui.m_setstate )
    {
        case UI_SSET_PRETRIGGER:
            {
                int val = uiel_control_edit_get_num( &ui.p.set_pretrigger );
                if ( ui.main_mode & OPMODE_LIGHT )
                {
                    if ( val != core->setup.light.iv_pretrig )
                    {
                        core->setup.light.iv_pretrig = val;
                        ui.setup_mod = true;
                    }
                }
                else
                {
                    if ( val != core->setup.sound.iv_pretrig )
                    {
                        core->setup.sound.iv_pretrig = val;
                        ui.setup_mod = true;
                    }
                }
            }
            break;
        case UI_SSET_COMBINE:  
            uist_interval_param_validate();
            break;
        case UI_SSET_SHUTTER:
            {
                int val = uiel_control_numeric_get( &ui.p.set_shutter.mlock );
                if ( val != core->setup.mlockup )
                {
                    core->setup.mlockup = val;
                    ui.setup_mod = true;
                }
                val = uiel_control_numeric_get( &ui.p.set_shutter.shtrp );
                if ( val != (core->setup.shtr_pulse*10) )
                {
                    core->setup.shtr_pulse = val / 10;
                    ui.setup_mod = true;
                }
            }
            break;
        case UI_SSET_DISPBRT:
            {
                int v1 = uiel_control_numeric_get( &ui.p.set_brightness.full );
                int v2 = uiel_control_numeric_get( &ui.p.set_brightness.dimmed );
                int v3 = uiel_control_checkbox_get( &ui.p.set_brightness.en_beep );
                if ( (v1!=core->setup.disp_brt_on) ||
                     (v2!=core->setup.disp_brt_dim) ||
                     (v3!=core->setup.beep_on)   )
                {   
                    core->setup.disp_brt_on = v1;
                    core->setup.disp_brt_dim = v2;
                    core->setup.beep_on = v3;
                    ui.setup_mod = true;
                }
                DispHAL_SetContrast( v1 );
            }
            break;
        case UI_SSET_PWRSAVE:
            {
                int v1 = uiel_control_list_get_value( &ui.p.set_powersave.stby );
                int v2 = uiel_control_list_get_value( &ui.p.set_powersave.dispoff );
                int v3 = uiel_control_list_get_value( &ui.p.set_powersave.pwroff );
                if ( (v1!=core->setup.pwr_stdby) ||
                     (v2!=core->setup.pwr_disp_off) ||
                     (v3!=core->setup.pwr_off)   )
                {   
                    core->setup.pwr_stdby = v1;
                    core->setup.pwr_disp_off = v2;
                    core->setup.pwr_off = v3;
                    ui.setup_mod = true;
                }
            }
            break;
    }

}



static inline bool uist_setwindow_process_combine(void)
{
    uint32 bitmask = 0;
    uint32 i;
    int bitcnt = 0;
    // collect the switches
    for ( i=0; i<5; i++ )
    {
        if ( uiel_control_checkbox_get( &ui.p.set_combine.mode[i] ) )
        {
            bitmask |= ( 1 << (i+1) );  // i+1 to skip OPMODE_CABLE
        }
    }
    // check for correct combinations  - see cases in core.c - up in the description header
    if ( bitmask == 0 )
    {
        if ( ui.main_mode > OPMODE_CABLE )
            bitmask = ui.main_mode;
        else
            bitmask = OPMODE_LONGEXPO;
    }
    else
    {
        if ( (bitmask & (~(OPMODE_LONGEXPO | OPMODE_SEQUENTIAL | OPMODE_TIMER))) == ( OPMODE_LIGHT | OPMODE_SOUND ) )   // if light and sound trigger simultaneous
        {
            if ( ui.focus == 4 )        // if focus on light trigger - delete sound trigger
                bitmask &= ~OPMODE_SOUND;
            else
                bitmask &= ~OPMODE_LIGHT;
        }
        /* allow this - but with different specs
        if ( bitmask & ( OPMODE_LIGHT | OPMODE_SOUND ) )    // if trigger mode selected - clear the sequential
        {
            bitmask &= ~OPMODE_SEQUENTIAL;
            core->op.opmode = 0;        // force update
        }
        */
    }

    i = 1;
    while ( i <= OPMODE_SOUND )
    {
        if ( bitmask & i )
            bitcnt++;
        i = i << 1;
    }
    if ( bitcnt > 1 )
    {
        bitmask |= OPMODE_COMBINED;
    }

    // update mode if needed
    if ( core->op.opmode != bitmask )
    {
        core->op.opmode = bitmask;
        for ( i=0; i<5; i++)
        {
            uiel_control_checkbox_set( &ui.p.set_combine.mode[i], core->op.opmode & ( 1 << (i+1)) );    // i+1 to skip OPMODE_CABLE
        }
        return true;
    }
    return false;
}

static inline bool uist_setwindow_process_shutter(void)
{
    if ( uiel_control_checkbox_get(&ui.p.set_shutter.mlock_en) == 0 )
    {
        uiel_control_numeric_set(&ui.p.set_shutter.mlock, 0);
        return true;
    }
    if ( uiel_control_numeric_get(&ui.p.set_shutter.mlock) == 0 )
    {
        uiel_control_numeric_set(&ui.p.set_shutter.mlock, 1);
        return true;
    }
    return false;
}

static inline bool uist_setwindow_process_brightness(void)
{
    int val;
    if ( ui.focus == 2 )    // do not allow dimmed to be bigger than normal on
    {
        val = uiel_control_numeric_get( &ui.p.set_brightness.dimmed );
        DispHAL_SetContrast( val );
        if ( val > uiel_control_numeric_get( &ui.p.set_brightness.full ) )
        {
            uiel_control_numeric_set( &ui.p.set_brightness.full, val );
            return true;
        }
    }
    else
    if ( ui.focus == 1 )    // do not allow full to be smaller than dimmed
    {
        val = uiel_control_numeric_get( &ui.p.set_brightness.full );
        DispHAL_SetContrast( val );
        if ( val < uiel_control_numeric_get( &ui.p.set_brightness.dimmed ) )
        {
            uiel_control_numeric_set( &ui.p.set_brightness.dimmed, val );
            return true;
        }
    }
    else
    {
        val = uiel_control_numeric_get( &ui.p.set_brightness.full );
        DispHAL_SetContrast( val );
    }
    return false;
}

static inline bool uist_setwindow_process_powersave(void)
{
    int i;
    int val;
    bool retval = false;

    if ( ui.focus == 0 )
        return false;

    val = uiel_control_list_get_index( ui.ui_elems[ui.focus-1] );
    for ( i=(ui.focus); i<3; i++ )
    {
        if ( val > uiel_control_list_get_index( ui.ui_elems[i] ) )
        {
            uiel_control_list_set_index( ui.ui_elems[i], val );
            retval = true;
        }
    }
    for ( i=0; i<(ui.focus-1); i++ )
    {
        if ( val < uiel_control_list_get_index( ui.ui_elems[i] ) )
        {
            uiel_control_list_set_index( ui.ui_elems[i], val );
            retval = true;
        }
    }
    return retval;
}


static inline bool uist_setwindow_process_defaults(void)
{
    bool ex = false;
    if ( uiel_control_checkbox_get( ui.ui_elems[0] ) )  // "No" pressed
        ex = true;
    if ( uiel_control_checkbox_get( ui.ui_elems[1] ) )
    {
        core_setup_reset( false );
        ui.setup_mod = true;
        ex = true;
    }
    if ( ex )
    {
        ui.m_state = UI_STATE_MENU;
        ui.m_setstate = UI_SSET_SETUPMENU;
        ui.m_substate = 0;
        ui.m_return = 2;
    }
    return false;
}


bool uist_setwindow_process_elements( void )
{
    // called when an ui element in menu point changes value
    switch ( ui.m_setstate )
    {
        case UI_SSET_COMBINE:   return uist_setwindow_process_combine();
        case UI_SSET_SHUTTER:   return uist_setwindow_process_shutter();
        case UI_SSET_DISPBRT:   return uist_setwindow_process_brightness();
        case UI_SSET_PWRSAVE:   return uist_setwindow_process_powersave();
        case UI_SSET_RESET:     return uist_setwindow_process_defaults();
    }
    return false;
}




////////////////////////////////////////////////////
//
//   Generic UI functional elements
//
////////////////////////////////////////////////////

static void uist_update_display( int disp_update )
{
    if ( disp_update )
    {
        if ( disp_update & RDRW_ALL )   // if anything to be redrawn
            uist_mainwindow_internal_draw( disp_update & RDRW_ALL  );

        DispHAL_UpdateScreen();
    }
}

static int uist_timebased_updates( struct SEventStruct *evmask )
{
    if ( evmask->timer_tick_10ms )
    {
        int update = 0;

        if ( ui.main_mode & (OPMODE_LIGHT | OPMODE_SOUND) )
        {
            ui.upd_dynamics++;
            if ( ui.upd_dynamics == 3 )     // obtain 30fps for dynamic stuff update
            {
                ui.upd_dynamics = 0;
                update |= RDRW_UI_DYNAMIC;
            }
        }

        if ( evmask->timer_tick_1sec )
        {
            ui.upd_batt++;
            if( ui.upd_batt == 5 )          // battery status update on every 5sec
            {
                ui.upd_batt = 0;
                update |= RDRW_BATTERY;
            }
        }

        return update;
    }
    return 0;
}


static bool ui_adc_ui_only()
{
    if ( (core->op.op_state == OPSTATE_STOPPED) &&              // if core is stopped
         (ui.main_mode & ( OPMODE_LIGHT | OPMODE_SOUND )) )     // and ui is in light/sound detection
        return true;
    return false;
}


static inline void ui_power_management( struct SEventStruct *evmask )
{
    if ( evmask->key_event )
    {
        ui.incativity = 0;          // reset inactivity counter
        if ( ui.pwr_state != SYSSTAT_UI_ON )
        {
            ui.pwr_state = SYSSTAT_UI_ON;
            // clear the key events since ui was in off state - just wake it up
            evmask->key_event = 0;
            evmask->key_released = 0;
            evmask->key_pressed = 0;
            evmask->key_longpressed = 0;
            if ( ui_adc_ui_only() )
            {
                // start up analog aquisition module
                core_analog_start( (ui.main_mode & OPMODE_SOUND) ? true : false );
            }
        }
        if ( ui.pwr_dispdim )
        {
            DispHAL_SetContrast( core->setup.disp_brt_on );
            ui.pwr_dispdim = false;
        }
        if ( ui.pwr_dispoff )
        {
            DispHAL_Display_On();
            ui.pwr_dispoff = false;
        }
    }
    else if ( evmask->timer_tick_1sec )
    {
        if ( ui.incativity < 0x7FFF )
        {
            ui.incativity++;
        }

        if ( ( ui.pwr_state != SYSSTAT_UI_PWROFF ) &&       // power off time limit reached
             ( core->setup.pwr_off ) && 
             ( core->setup.pwr_off < ui.incativity) )       
        {
            DispHAL_Display_Off();
            ui.pwr_state = SYSSTAT_UI_PWROFF;
        }
        else if ( ( core->setup.pwr_disp_off ) &&           // display off limit reached
                  ( core->setup.pwr_disp_off < ui.incativity) )
        {
            DispHAL_Display_Off();
            ui.pwr_dispdim = true;
            ui.pwr_dispoff = true;
            ui.pwr_state = SYSSTAT_UI_STOPPED;
            if ( ui.main_mode == OPMODE_CABLE )             // if in cable release mode - set display of with immediate action on "Start" button
                ui.pwr_state = SYSSTAT_UI_STOP_W_SKEY;
            else if ( ui_adc_ui_only() )                    // stop the adc also when display is off
                core_analog_stop();
        }
        else if ( ( (ui.pwr_state & (SYSSTAT_UI_STOPPED | SYSSTAT_UI_STOP_W_ALLKEY | SYSSTAT_UI_STOP_W_SKEY)) == 0) && // display dimmed, ui stopped - waiting for interrupts
                  ( ui.pwr_dispdim == false ) &&
                  ( core->setup.pwr_stdby ) && 
                  ( core->setup.pwr_stdby < ui.incativity) )
        {
            DispHAL_SetContrast( core->setup.disp_brt_dim );
            ui.pwr_dispdim = true;
            if ( ui_adc_ui_only() == false )                // mark stopped state only if ui doesn't use the analog part
                ui.pwr_state = SYSSTAT_UI_STOP_W_ALLKEY;    // mark with all key immediate action ( ui is visible, user action will trigger valid command )
        } 
    }
}

static void uist_goto_shutdown(void)
{
    core_analog_stop();
    ui.m_state = UI_STATE_SHUTDOWN;
    ui.m_substate = 0;
}


////////////////////////////////////////////////////
//
//   UI STATUS ROUTINES
//
////////////////////////////////////////////////////

/// UI STARTUP

void uist_startup_entry( void )
{
    Graphics_ClearScreen(0);
    uibm_put_bitmap( 5, 16, BMP_START_SCREEN );
    DispHAL_UpdateScreen();
    core_beep( beep_pwron );
    ui.m_substate = 200;    // 2sec. startup screen
}


void uist_startup( struct SEventStruct *evmask )
{
    if ( evmask->timer_tick_10ms )
    {
        ui.m_substate--;
    }

    if ( evmask->key_event || (ui.m_substate == 0) )
    {
        ui.m_state = UI_STATE_MAIN_WINDOW;
        ui.m_substate = 0;
        ui.main_mode = OPMODE_CABLE;
    }
}

/// UI SHUTDOWN

void uist_shutdown_entry( void )
{
    Graphics_ClearScreen(0);
    Graphic_SetColor(1);
    Graphic_FillRectangle( 0, 0, 127, 63, 1 );
    DispHAL_UpdateScreen();
    core_beep( beep_pwroff );
    ui.m_substate = 100;    // 1sec. startup screen
}


void uist_shutdown( struct SEventStruct *evmask )
{
    int val;

    if ( ui.m_substate <= 2 )
    { 
        ui.pwr_state = SYSSTAT_UI_PWROFF;
        return;
    }

    if ( evmask->timer_tick_10ms == 0 )
        return;
    ui.m_substate--;
    if ( ui.m_substate & 0x01 )
        return;
    Graphics_ClearScreen(0);
    val = (ui.m_substate >> 1);
    if ( val >= 40 )
    {
        val -= 40;
        Graphic_FillRectangle( (10-val)*5, (10-val)*32/10, 127-((10-val)*5), 63-((10-val)*32/10), 1 );
    }
    else if ( val >= 30 )
    {
        val = 10 - (val - 30);
        Graphic_Rectangle( 50-val*3, 31, 77 + val*3, 32 );
    }
    else if ( val >= 20 )
    {
        val = (val - 20);
        Graphic_Rectangle( 50-val*3, 32, 77 + val*3, 32 );
    }
    else if ( val >= 10 )
    {
        val = 10- ( val - 10 );
        Graphic_Rectangle( 50+val, 32, 77-val, 32 );
    }
    else if ( val >= 2)
    {
        Graphic_Rectangle( 63, 31, 64, 32 );
    }
    else if ( val == 1 )
    {
        DispHAL_Display_Off();
    }

    DispHAL_UpdateScreen();
}

/// UI MAIN WINDOW

void uist_mainwindow_entry( void )
{
    uist_mainwindow_setup_view( true );
    uist_mainwindow_internal_draw( RDRW_ALL );
    DispHAL_UpdateScreen();
    ui.m_substate ++;
}


void uist_mainwindow( struct SEventStruct *evmask )
{
    int disp_update = 0;

    if ( evmask->key_event )
    {
        // generic UI buttons and events
        if ( ui.focus )
        {
            // operations if focus on ui elements
            if ( ui_element_poll( ui.ui_elems[ ui.focus - 1], evmask ) )
                disp_update  = RDRW_DISP_UPDATE;          // mark only for dispHAL update

            // move focus to the next element
            if ( evmask->key_pressed & KEY_MINUS )
            {
                if ( ui.focus < ui.ui_elem_nr )
                {
                    ui.focus++;
                    disp_update |= RDRW_UI_CONTENT;
                }
            }

            // move focus to the previous element
            if ( evmask->key_pressed & KEY_PLUS )
            {
                if ( ui.focus > 1 )
                {
                    ui.focus--;
                    disp_update |= RDRW_UI_CONTENT;
                }
            }

            // for light and sound we need to update the parameters at the moment of modification
            if ( (disp_update & RDRW_DISP_UPDATE) && (ui.main_mode & ( OPMODE_LIGHT | OPMODE_SOUND )) )
            {
                if ( uist_mainwindow_apply_values() )
                {
                    disp_update |= RDRW_UI_CONTENT_ALL;
                }
            }
        }
        else
        {
            // operations if no focus yet - OK presssed
            if ( evmask->key_pressed & KEY_OK )    // enter in edit mode
            {
                ui.focus = 1;
                disp_update |= RDRW_UI_CONTENT;

                if ( ui.ui_elem_nr == 1 )
                {
                    // since these modes have only one ui element poll this control again to select for editting
                    uist_mainwindow_internal_draw( RDRW_UI_CONTENT );
                    ui_element_poll( ui.ui_elems[0], evmask );
                }
            }
        }

        // for cable release we need to check the prefocus checkbox and set up the prefocus signal
        if ( ui.main_mode == OPMODE_CABLE )
        {
            if ( uiel_control_checkbox_get( &ui.p.cable.prefocus ) )
            {
                HW_Shutter_Prefocus_ON();
            }
            else
            {
                HW_Shutter_Prefocus_OFF();
            }
        }

        // mode button
        if ( evmask->key_released & KEY_POWER )
        {
            uist_mainwindow_apply_values();
            HW_Shutter_Prefocus_OFF();              // this is for when exiting from cable release mode
            ui.main_mode = ui.main_mode << 1;
            if (ui.main_mode > OPMODE_SOUND)
            {
                core_analog_stop();
                if ( core->op.opmode & OPMODE_COMBINED )        // do not allow entering in cable release mode if combinations are in use
                    ui.main_mode = OPMODE_LONGEXPO;
                else 
                    ui.main_mode = OPMODE_CABLE;
            }
            if ( (core->op.opmode & OPMODE_COMBINED) == 0)
            {
                core->op.opmode = ui.main_mode;
            }

            if ( ui.main_mode == OPMODE_LIGHT )
                core_analog_start( false );
            if ( ui.main_mode == OPMODE_SOUND )
            {
                core_analog_stop();
                core_analog_start( true );
            }

            uist_mainwindow_setup_view( true );
            disp_update |= RDRW_ALL;                    // redraw the whole screen
        }

        // power button activated
        if ( evmask->key_longpressed & KEY_POWER )
        {
            uist_goto_shutdown();
        }

        // menu button
        if ( evmask->key_pressed & KEY_MENU )           // menu button pressed
        {
            uist_mainwindow_apply_values();
            ui.m_state = UI_STATE_MENU;
            ui.m_substate = 0;
            return;
        }

        // start button
        if ( evmask->key_pressed & KEY_STARTSTOP )
        {
            uist_mainwindow_apply_values();
            ui.m_state = UI_STATE_STARTED;
            ui.m_substate = 0;
        }

        // start button
        if ( evmask->key_pressed & KEY_CANCEL )
        {
            uist_mainwindow_apply_values();
            ui.focus = 0;
            disp_update |= RDRW_UI_CONTENT;
        }
    }

    // update screen on timebase
    disp_update |= uist_timebased_updates( evmask );

    uist_update_display( disp_update );
}


/// UI MENU

void uist_menu_entry( void )
{
    Graphics_ClearScreen(0);
    uist_internal_statusbar( ui.main_mode, false );
    if ( ui.m_setstate == 0 )
        uigrf_text( 20, 4, uitxt_smallbold, "Main menu:" );
    else
        uigrf_text( 20, 4, uitxt_smallbold, "Setup menu:" );

    Graphic_FillRectangle( 16, 0, 116, 15, -1);
    Graphic_PutPixel( 16, 0, 0 );
    Graphic_PutPixel( 116, 0, 0 );

    uiel_dropdown_menu_init( &ui.p.menu, 20, 100, 16, 61 );

    if ( ui.m_setstate == 0 )       // case of main menu
    {
        if ( ui.main_mode & ( OPMODE_LIGHT | OPMODE_SOUND )  )
        {
            char txt[16];
            int len;
            uint32 dly;
    
            if ( ui.main_mode == OPMODE_LIGHT )
                dly = core->setup.light.iv_pretrig;
            else
                dly = core->setup.sound.iv_pretrig;
    
            strcpy( txt, "delay: ");
            itoa( dly / 10, txt+7, 10 );
            strcat( txt, "." );
            len = strlen(txt);
            txt[len] = '0' + dly % 10;
            txt[len+1] = 0;
            strcat( txt, "ms" );
            uiel_dropdown_menu_add_item( &ui.p.menu, txt );
        }
        uist_internal_setup_menu( &ui.p.menu, c_menu_main );

        if ( ui.main_mode & ( OPMODE_LIGHT | OPMODE_SOUND )  )
            uiel_dropdown_menu_add_item( &ui.p.menu, "scope" );

    }
    else                            // case of submenu
    {
        uist_internal_setup_menu( &ui.p.menu, c_menu_setup );
    }

    uiel_dropdown_menu_set_index( &ui.p.menu, ui.m_return );
    ui_element_display( &ui.p.menu, true );

    DispHAL_UpdateScreen();
    ui.m_substate ++;
}

void uist_menu( struct SEventStruct *evmask )
{
    int disp_update = 0;

    if ( evmask->key_event )
    {
        disp_update = ui_element_poll( &ui.p.menu, evmask );

        if ( evmask->key_pressed & KEY_CANCEL )
        {
            if ( ui.m_setstate == 0 )
            {
                ui.m_state = UI_STATE_MAIN_WINDOW;
                ui.m_return = 0;
            }
            else
            {
                ui.m_setstate = 0;
                if ( ui.main_mode & ( OPMODE_LIGHT | OPMODE_SOUND ) )
                    ui.m_return = 3;
                else
                    ui.m_return = 2;
            }
            ui.m_substate = 0;
            return;
        }

        // power button activated
        if ( evmask->key_longpressed & KEY_POWER )
        {
            uist_goto_shutdown();
        }

        if ( evmask->key_pressed & KEY_OK )
        {
            int m_item;

            m_item = uiel_dropdown_menu_get_index( &ui.p.menu );
            ui.m_substate = 0;

            if ( ui.m_setstate == 0 )
            {
                if ( ui.main_mode & ( OPMODE_LIGHT | OPMODE_SOUND )  )
                    m_item--;
                switch ( m_item )
                {
                    case -1:        // us. delay for light / sound trigger
                        ui.m_state = UI_STATE_SET_WINDOW;
                        ui.m_setstate = UI_SSET_PRETRIGGER;
                        break;
                    case 0:         // combine operation modes
                        ui.m_state = UI_STATE_SET_WINDOW;
                        ui.m_setstate = UI_SSET_COMBINE;
                        break;
                    case 1:
                        ui.m_state = UI_STATE_SET_WINDOW;
                        ui.m_setstate = UI_SSET_SHUTTER;
                        break;
                    case 2:
                        ui.m_setstate = UI_SSET_SETUPMENU;
                        ui.m_return = 0;
                        break;
                    case 3:
                        ui.m_state = UI_STATE_SCOPE;
                        break;
                }
            }
            else
            {
                ui.m_state = UI_STATE_SET_WINDOW;
                switch ( m_item )
                {
                    case 0:
                        ui.m_setstate = UI_SSET_DISPBRT;
                        break;
                    case 1:
                        ui.m_setstate = UI_SSET_PWRSAVE;
                        break;
                    case 2:
                        ui.m_setstate = UI_SSET_RESET;
                        break;
                }

            }

        }

    }

    if ( disp_update )
        DispHAL_UpdateScreen();
}



/// UI SETUP WINDOW

void uist_setwindow_entry( void )
{
    uist_setwindow_setup_view();
    uist_setwindow_internal_draw( RDRW_ALL );
    DispHAL_UpdateScreen();
    ui.m_substate ++;
}


void uist_setwindow( struct SEventStruct *evmask )
{
    int disp_update = 0;

    if ( evmask->key_event )
    {
        // operations if focus on ui elements
        if ( ui_element_poll( ui.ui_elems[ ui.focus - 1], evmask ) )
        {
            disp_update  = RDRW_DISP_UPDATE;                    // mark only for dispHAL update
            if ( uist_setwindow_process_elements() )
                disp_update |= RDRW_UI_CONTENT;
        }

        // move focus to the next element
        if ( evmask->key_pressed & KEY_MINUS )
        {
            if ( ui.focus < ui.ui_elem_nr )
            {
                ui.focus++;
                disp_update |= RDRW_UI_CONTENT;
            }
            if ( uist_setwindow_process_elements() )
                disp_update |= RDRW_UI_CONTENT;
        }

        // move focus to the previous element
        if ( evmask->key_pressed & KEY_PLUS )
        {
            if ( ui.focus > 1 )
            {
                ui.focus--;
                disp_update |= RDRW_UI_CONTENT;
            }
            if ( uist_setwindow_process_elements() )
                disp_update |= RDRW_UI_CONTENT;
        }

        // power button activated
        if ( evmask->key_longpressed & KEY_POWER )
        {
            uist_goto_shutdown();
        }

        // esc. is pressed
        if ( evmask->key_pressed & ( KEY_CANCEL | KEY_MENU ) )
        {
            uist_setwindow_apply_values();

            ui.m_state = UI_STATE_MENU;
            ui.m_substate = 0;

            if ( (ui.m_setstate == UI_SSET_RESET) ||
                 (ui.m_setstate == UI_SSET_PWRSAVE) ||
                 (ui.m_setstate == UI_SSET_DISPBRT) )
            {
                ui.m_return = ui.m_setstate - UI_SSET_DISPBRT;
                ui.m_setstate = UI_SSET_SETUPMENU;
            }
            else
            {
                if ( ui.main_mode & ( OPMODE_LIGHT | OPMODE_SOUND ) )
                    ui.m_return = ui.m_setstate - UI_SSET_PRETRIGGER;       // we have a menu point before
                else
                    ui.m_return = ui.m_setstate - UI_SSET_COMBINE;
                ui.m_setstate = 0;
            }
            return;
        }

    }

    disp_update |= uist_timebased_updates(evmask);
    if ( disp_update )
    {
        if ( disp_update & RDRW_ALL )   // if anything to be redrawn
            uist_setwindow_internal_draw( disp_update & RDRW_ALL );
        DispHAL_UpdateScreen();
    }
}


/// UI STARTED

void uist_started_entry( void )
{
    core_reset_longexpo();
    core_reset_sequential();
    core_reset_timer();

    uist_mainwindow_setup_view( true );
    uist_mainwindow_internal_draw( RDRW_ALL );
    DispHAL_UpdateScreen();

    if ( ui.main_mode != OPMODE_CABLE )
        core_beep( beep_expo_start );

    ui.m_substate = 1;
}


void uist_started( struct SEventStruct *evmask )
{
    int disp_update = 0;
    bool cbl_stop = false;

    if ( ui.m_substate == 1 )
    {
        ui.m_substate++;
        core_start();           // call core start after display is up-to-date to prevent huge lags
    }

    // for cable release - special case:
    if ( ui.main_mode == OPMODE_CABLE )
    {
        if ( evmask->key_longpressed & KEY_STARTSTOP )      // if (S) is long-pressed - start the bulb mode
        {
            core_start_bulb();
            core_beep( beep_expo_start );
        }

        if ( (evmask->key_released & KEY_STARTSTOP) && (core->op.op_state != OPSTATE_CABLEBULB) )   // if (S) button is released - stop the exposure
            cbl_stop = true;
    }


    // stop is given or program stopped
    if ( (evmask->key_pressed & KEY_STARTSTOP) || (core->op.op_state == 0) || (cbl_stop) )
    {
        core_stop();
        if ( (ui.main_mode & ( OPMODE_LIGHT | OPMODE_SOUND )) == 0 )
        {
            core_analog_stop();
        }
        ui.m_state = UI_STATE_MAIN_WINDOW;
        ui.m_substate = 0;
        ui.m_return = 0;

        if ( ui.main_mode != OPMODE_CABLE )
            core_beep( beep_expo_end );

        return;
    }

    // change screens
    if ( evmask->key_released & KEY_POWER )
    {
        // shift to the next active window
        do
        {
            ui.main_mode = ui.main_mode << 1;
            if (ui.main_mode > OPMODE_SOUND)
            {
                ui.main_mode = OPMODE_CABLE;
            }
        } while ( (ui.main_mode & core->op.opmode) == 0 );

        uist_mainwindow_setup_view( false );
        disp_update |= RDRW_ALL;    // redraw the whole screen
    }

    // power button activated
    if ( evmask->key_longpressed & KEY_POWER )
    {
        core_stop();
        uist_goto_shutdown();
    }

    // special case of updating for run mode
    if ( evmask->timer_tick_1sec )
    {
        timestruct tm = {0, };

        switch( ui.main_mode )
        {
            case OPMODE_CABLE:
                uiel_control_time_set_time( &ui.p.cable.time, core->op.time_longexpo );
                break;
            case OPMODE_LONGEXPO:
                uiel_control_time_set_time( &ui.p.longexpo.time, core->op.time_longexpo );
                break;
            case OPMODE_SEQUENTIAL:
                tm.minute = core->op.seq_interval / 60;
                tm.second = core->op.seq_interval % 60;
                uiel_control_time_set_time( &ui.p.interval.iv, tm );
                uiel_control_edit_set_num( &ui.p.interval.shot_nr, core->op.seq_shotnr );
                break;
            case OPMODE_TIMER:
                tm.minute = core->op.tmr_interval / 60;
                tm.second = core->op.tmr_interval % 60;
                uiel_control_time_set_time( &ui.p.timer.time, tm );
                break;

        }
        disp_update |= RDRW_UI_CONTENT | RDRW_STATUSBAR;
    }

    // generic cases for updating
    disp_update |= uist_timebased_updates(evmask);

    uist_update_display( disp_update );
}


/// SCOPE WINDOW

void uist_scope_entry( void )
{
    memset( &ui.p.scope, 0, sizeof(ui.p.scope) );
    ui.p.scope.tmr_pre = 5;
    ui.p.scope.tmr_run = 1;
    ui.p.scope.osc_gain = 1;
    core_analog_get_raws( ui.p.scope.raws);
    uist_scope_internal_draw( RDRW_ALL );
    DispHAL_UpdateScreen();
    ui.m_substate = 1;

    // disable power management
    ui.p.scope.pwrorg_dim = core->setup.pwr_stdby;
    ui.p.scope.pwrorg_doff = core->setup.pwr_disp_off;
    ui.p.scope.pwrorg_off = core->setup.pwr_off;
    core->setup.pwr_stdby = 0;
    core->setup.pwr_disp_off = 0;
    core->setup.pwr_off = 0;
}


void uist_scope( struct SEventStruct *evmask )
{
    int disp_update = 0;

    if ( evmask->timer_tick_10ms && ui.p.scope.tmr_run )
    {
        ui.p.scope.tmr_ctr++;
        if ( ui.p.scope.tmr_ctr >= ui.p.scope.tmr_pre )
        {
            ui.p.scope.tmr_ctr = 0;
            disp_update |= RDRW_DISP_UPDATE | RDRW_UI_DYNAMIC | RDRW_BATTERY;
        }
    }

    // stop is given or program stopped
    if ( evmask->key_pressed & KEY_CANCEL )
    {
        struct SADC *adc;
        adc = (struct SADC*)core_analog_getbuffer();
        adc->channel_lock = 0;
        ui.m_state = UI_STATE_MENU;
        ui.m_substate = 0;
        ui.m_return = 0;
        core->setup.pwr_stdby = ui.p.scope.pwrorg_dim;
        core->setup.pwr_disp_off = ui.p.scope.pwrorg_doff;
        core->setup.pwr_off = ui.p.scope.pwrorg_off;
        return;
    }

    // switch display modes
    if ( evmask->key_pressed & KEY_STARTSTOP )
    {
        ui.p.scope.tmr_run ^= 1;
        disp_update |= RDRW_UI_CONTENT;
    }

    // switch display modes
    if ( evmask->key_pressed & KEY_OK )
    {
        disp_update |= RDRW_DISP_UPDATE | RDRW_UI_DYNAMIC | RDRW_BATTERY;
    }

    // switch operation modes
    if ( evmask->key_pressed & KEY_MENU )
    {
        struct SADC *adc;
        adc = (struct SADC*)core_analog_getbuffer();

        ui.p.scope.scope_mode ++;
        if ( ui.p.scope.scope_mode > UISM_SCOPE_HI )
        {
            ui.p.scope.scope_mode = UISM_VALUELIST;
            if ( ui.p.scope.tmr_pre < 2 )
                ui.p.scope.tmr_pre = 2;
        }

        adc->channel_lock = ui.p.scope.scope_mode;
        disp_update |= RDRW_ALL;
    }

    // switch display modes
    if ( evmask->key_pressed & KEY_POWER )
    {
        // for value display - switch the hex/dev/unit 
        if ( ui.p.scope.scope_mode == UISM_VALUELIST )
        {
            ui.p.scope.disp_unit ++;
            if ( ui.p.scope.disp_unit > UISCOPE_UNIT )
                ui.p.scope.disp_unit = UISCOPE_HEX;
            disp_update = RDRW_ALL;
        }
        // for scope displays - switch the adjustment modes
        else
        {
            ui.p.scope.adj_mode ++;
            if ( ui.p.scope.adj_mode > UISA_TRIGGER )
                ui.p.scope.adj_mode = UISA_SAMPLERATE;
            disp_update |= RDRW_UI_CONTENT;
        }
    }

    // display prescaler up
    if ( evmask->key_pressed & KEY_PLUS )
    {
        if ( (ui.p.scope.scope_mode == UISM_VALUELIST) || (ui.p.scope.adj_mode == UISA_SAMPLERATE) )
        {
            if ( ui.p.scope.tmr_pre < 100 )
            {
                ui.p.scope.tmr_pre ++;
                disp_update |= RDRW_UI_CONTENT;
            }
        }
        else if ( ui.p.scope.adj_mode == UISA_GAIN )
        {
            if ( ui.p.scope.osc_gain < 200 )
            {
                ui.p.scope.osc_gain ++;
                disp_update |= RDRW_UI_CONTENT;
            }
        }
        else
        {
            if ( ui.p.scope.osc_trigger < 64 )
            {
                ui.p.scope.osc_trigger ++;
                disp_update |= RDRW_UI_CONTENT;
            }
        }
    }
    // display prescaler down
    if ( evmask->key_pressed & KEY_MINUS )
    {
        if ( (ui.p.scope.scope_mode == UISM_VALUELIST) || (ui.p.scope.adj_mode == UISA_SAMPLERATE) )
        {
            if ( (ui.p.scope.tmr_pre > 2) ||
                 ((ui.p.scope.scope_mode != UISM_VALUELIST) && (ui.p.scope.tmr_pre > 1)) )
            {
                ui.p.scope.tmr_pre --;
                disp_update |= RDRW_UI_CONTENT;
            }
        }
        else if ( ui.p.scope.adj_mode == UISA_GAIN )
        {
            if ( ui.p.scope.osc_gain > 1 )
            {
                ui.p.scope.osc_gain --;
                disp_update |= RDRW_UI_CONTENT;
            }
        }
        else
        {
            if ( ui.p.scope.osc_trigger > 0 )
            {
                ui.p.scope.osc_trigger --;
                disp_update |= RDRW_UI_CONTENT;
            }
        }
    }

    // if values should be updted
    if ( disp_update & RDRW_DISP_UPDATE )
    {
        core_analog_get_raws( ui.p.scope.raws);
        disp_update &= ~RDRW_DISP_UPDATE;
        if ( ui.p.scope.scope_mode )
            uist_scope_internal_trace();
    }
    if ( disp_update )
    {
        uist_scope_internal_draw( disp_update );
        DispHAL_UpdateScreen();
    }

}



////////////////////////////////////////////////////
//
//   UI core
//
////////////////////////////////////////////////////

void ui_init( struct SCore *instance )
{
    core = instance;

    Graphics_Init( NULL, NULL );

    memset( &ui, 0, sizeof(ui) );
    ui.pwr_state = SYSSTAT_UI_ON;

}//END: ui_init


int ui_poll( struct SEventStruct *evmask )
{
    struct SEventStruct evm_bkup = *evmask;         // save a backup since ui routines can alter states, and those will not be cleared then

    // process power management stuff for ui part
    ui_power_management( evmask );

    // ui state machine
    if ( ui.m_substate == UI_SUBST_ENTRY )
    {
        switch ( ui.m_state )
        {
            case UI_STATE_STARTUP:
                uist_startup_entry();
                break;
            case UI_STATE_SHUTDOWN:
                uist_shutdown_entry();
                break;
            case UI_STATE_MAIN_WINDOW:
                uist_mainwindow_entry();
                break;
            case UI_STATE_MENU:
                uist_menu_entry();
                break;
            case UI_STATE_SET_WINDOW:
                uist_setwindow_entry();
                break;
            case UI_STATE_SCOPE:
                uist_scope_entry();
                break;
            case UI_STATE_STARTED:
                uist_started_entry();
                break;
        }
    }
    else
    {
        switch ( ui.m_state )
        {
            case UI_STATE_STARTUP:
                uist_startup( evmask );
                break;
            case UI_STATE_SHUTDOWN:
                uist_shutdown( evmask );
                break;
            case UI_STATE_MAIN_WINDOW:
                uist_mainwindow( evmask );
                break;
            case UI_STATE_MENU:
                uist_menu( evmask );
                break;
            case UI_STATE_SET_WINDOW:
                uist_setwindow( evmask );
                break;
            case UI_STATE_SCOPE:
                uist_scope( evmask );
                break;
            case UI_STATE_STARTED:
                uist_started( evmask );
                break;
        }
    }

    *evmask = evm_bkup;
    if ( (ui.pwr_state == SYSSTAT_UI_PWROFF) && (ui.setup_mod == true) )
    {   
        if ( DispHAL_ReleaseSPI() )             // release display SPI bus - will be reactivated as soon a new display operation is done
        {
            core_setup_save();
            ui.setup_mod = false;
        }
        else
        { 
            ui.pwr_state = SYSSTAT_UI_STOPPED; // do not send power off state till settings are not saved
        }
    }
    
    return ui.pwr_state;
}
