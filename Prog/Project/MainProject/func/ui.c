/*
 * - User interface state machine
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
#include "utilities.h"

#ifndef ON_QT_PLATFORM
  #include "stdlib_extension.h"
  #include "dispHAL.h"
#endif


struct SUIstatus ui;
extern struct SCore core;



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
        Graphic_SetColor(1);
        Graphic_FillRectangle( 0, 0, 118, 15, 1);
        Graphic_SetColor(0);
        // draw the page selection
        for ( x=0; x<3; x++ )
        {
            if ( x == ui.main_mode )
                Graphic_Rectangle( 1+x*5, 13, 3+x*5, 14 );
            else
                Graphic_PutPixel( 2+x*5, 14, 0 );
        }
        Graphic_PutPixel( 0, 0, 0 );
        Graphic_PutPixel( 118, 0, 0 );

        if ( opmode == UIMODE_GAUDE_THERMO )
        {
            uigrf_text_inv( 2, 4, uitxt_smallbold, "Thermo:" );
        }
        else if ( opmode == UIMODE_GAUGE_HYGRO )
        {
            uigrf_text_inv( 2, 4, uitxt_smallbold, "Hygro:" );
        }
        else if ( opmode == UIMODE_GAUGE_PRESSURE )
        {
            uigrf_text_inv( 2, 4, uitxt_smallbold, "Baro:" );
        }

        // draw time
        {
            timestruct tm;
            datestruct dt;
            utils_convert_counter_2_hms( clock, &tm.hour, &tm.minute, NULL );
            utils_convert_counter_2_ymd( clock, &dt.year, &dt.mounth, &dt.day );
            uigrf_puttime( 92, 1, uitxt_small, 0, tm, true, false );
            uigrf_putdate( 93, 10, uitxt_micro, 0, dt, false, true );
        }
    }

    // clock blink
    if ( clock & 0x01 )
    {
        Graphic_PutPixel( 104, 3, 1 );
        Graphic_PutPixel( 104, 6, 1 );
    }
    else
    {
        Graphic_PutPixel( 104, 3, 0 );
        Graphic_PutPixel( 104, 6, 0 );
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



////////////////////////////////////////////////////
//
//   UI status dependent redraw routines
//
////////////////////////////////////////////////////

static inline void uist_draw_gauge_thermo( int redraw_all )
{
    int temp = ( 23.65 * 100 );       // testing purpose
    int temp_int = temp / 100;
    int temp_fract = temp % 100;

    if (temp_fract < 0)
        temp_fract = -temp_fract;

    // temperature display
    int x = 13;
    int y = 17;
    uigrf_putnr(x, y, uitxt_large_num | uitxt_MONO, temp_int, 2, 0 , true );
    uigrf_putnr(x+37, y+14, uitxt_smallbold | uitxt_MONO, temp_fract, 2, '0', false );
    Graphic_SetColor( 1 );
    Graphic_Rectangle( x+33, y+19, x+34, y+20 );
    Graphic_Rectangle( x+40, y+2, x+42, y+4 );
    uigrf_text( x+45, y+2, uitxt_small,  "C" );

    // min/max set display
    x = 0;
    y = 41;

    uigrf_text( x, y+1, uitxt_micro,  "  SET1     SET2      DAY-" );
    Graphic_SetColor( -1 );
    Graphic_FillRectangle( x, y, x + 75, y + 6, -1 );

    uigrf_putfixpoint( x, y+8, uitxt_small, -124, 3, 1, 0x00, false );
    uigrf_putfixpoint( x, y+16, uitxt_small, -325, 3, 1, 0x00, false );
    uigrf_putfixpoint( x+28, y+8, uitxt_small, -124, 3, 1, 0x00, false );
    uigrf_putfixpoint( x+28, y+16, uitxt_small, -325, 3, 1, 0x00, false );
    uigrf_putfixpoint( x+54, y+8, uitxt_small, -124, 3, 1, 0x00, false );
    uigrf_putfixpoint( x+54, y+16, uitxt_small, -325, 3, 1, 0x00, false );

    // tendency meter
    x = 77;
    y = 16;
    uigrf_putfixpoint( x+4, y+43, uitxt_micro, -2253, 3, 2, 0x00, false );
    uigrf_text( x+32, y+43, uitxt_micro, "C/MIN" );
    Graphic_Rectangle( x+27, y+43, x+29, y+45 );    // *C

    // tendency graph
    uigrf_putnr( x+40, y, uitxt_micro, 20, 2, 0x00, true );
    uigrf_putnr( x+40, y+36, uitxt_micro, -18, 3, 0x00, true );
    Graphic_Rectangle( x+40, y+5, x+41, y+35 );
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

//    ui_element_display( &ui.p.mgThermo.temp, ui.focus );
}


static inline void uist_draw_gauge_hygro( int redraw_all )
{
    int hum = ( 98.6 * 10 );       // testing purpose
    int hum_int = hum / 10;
    int hum_fract = hum % 10;

    // humidity display
    int x = 13;
    int y = 17;
    uigrf_putnr(x, y, uitxt_large_num | uitxt_MONO, hum_int, 3, ' ', false );
    uigrf_putnr(x+37, y+14, uitxt_smallbold | uitxt_MONO, hum_fract, 1, '0', false );
    Graphic_SetColor( 1 );
    Graphic_Rectangle( x+33, y+19, x+34, y+20 );
    uigrf_text( x+45, y+2, uitxt_small,  "%" );

    // min/max set display
    x = 0;
    y = 41;

    uigrf_text( x, y+1, uitxt_micro,  "  SET1     SET2      DAY-" );
    Graphic_SetColor( -1 );
    Graphic_FillRectangle( x, y, x + 75, y + 6, -1 );

    uigrf_putfixpoint( x, y+8, uitxt_small, 1000, 3, 1, 0x00, false );
    uigrf_putfixpoint( x, y+16, uitxt_small, 607, 3, 1, 0x00, false );
    uigrf_putfixpoint( x+28, y+8, uitxt_small, 889, 3, 1, 0x00, false );
    uigrf_putfixpoint( x+28, y+16, uitxt_small, 561, 3, 1, 0x00, false );
    uigrf_putfixpoint( x+54, y+8, uitxt_small, 567, 3, 1, 0x00, false );
    uigrf_putfixpoint( x+54, y+16, uitxt_small, 600, 3, 1, 0x00, false );

    // tendency meter
    x = 77;
    y = 16;
    uigrf_putfixpoint( x+4, y+43, uitxt_micro, -246, 3, 1, 0x00, false );
    uigrf_text( x+32, y+43, uitxt_micro, "%/MIN" );

    // tendency graph
    uigrf_putnr( x+40, y, uitxt_micro, 100, 2, 0x00, false );
    uigrf_putnr( x+40, y+36, uitxt_micro, 40, 3, 0x00, false );
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


static inline void uist_draw_gauge_pressure( int redraw_all )
{
    int press = ( 1013.25 * 100 );       // testing purpose
    int press_int = press / 100;
    int press_fract = press % 100;

    // pressure display
    int x = 7;
    int y = 17;
    uigrf_putnr(x, y, uitxt_large_num | uitxt_MONO, press_int, 4, ' ', false );
    uigrf_putnr(x+48, y+14, uitxt_smallbold | uitxt_MONO, press_fract, 2, '0', false );
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
    uigrf_putfixpoint( x+40, y, uitxt_micro, 2.9, 2, 1, 0x00, false );
    uigrf_putfixpoint( x+40, y+36, uitxt_micro, 0.1, 2, 1, 0x00, false );
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
    uiel_control_numeric_init( &ui.p.mgThermo.temp, -50, 100, 0, 80, 16, 4, 0x00, uitxt_large_num );
    ui.ui_elem_nr = 1;
    ui.ui_elems[0] = &ui.p.mgThermo.temp;
}

static inline void uist_setview_mainwindowgauge_hygro( void )
{
    ui.ui_elem_nr = 0;
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

static void uist_drawview_mainwindow( int redraw_type )
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
            case UIMODE_GAUDE_THERMO:   uist_draw_gauge_thermo(redraw_type); break;
            case UIMODE_GAUGE_HYGRO:    uist_draw_gauge_hygro(redraw_type); break;
            case UIMODE_GAUGE_PRESSURE: uist_draw_gauge_pressure(redraw_type); break;
            break;
        }
    }
}


void uist_setupview_mainwindow( bool reset )
{
    ui.focus = 0;

    // main windows
    switch ( ui.main_mode )
    {
        case UIMODE_GAUDE_THERMO:      uist_setview_mainwindowgauge_thermo(); break;
        case UIMODE_GAUGE_HYGRO:       uist_setview_mainwindowgauge_hygro(); break;
        case UIMODE_GAUGE_PRESSURE:    uist_setview_mainwindowgauge_pressure(); break;
    }
}


////////////////////////////////////////////////////
//
//   UI status dependent apply set value routines
//
////////////////////////////////////////////////////




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
            uist_drawview_mainwindow( disp_update & RDRW_ALL  );

        DispHAL_UpdateScreen();
    }
}

static int uist_timebased_updates( struct SEventStruct *evmask )
{
    if ( evmask->timer_tick_10ms )
    {
        int update = 0;

        if ( 0 ) // do it for dynamic modes (will be needed for altimeter)
        {
            ui.upd_dynamics++;
            if ( ui.upd_dynamics == 3 )     // obtain 30fps for dynamic stuff update
            {
                ui.upd_dynamics = 0;
                update |= RDRW_UI_DYNAMIC;
            }
        }

        if ( evmask->timer_tick_05sec )
        {
            ui.upd_batt++;
            if( ui.upd_batt == 10 )          // battery status update on every 5sec
            {
                ui.upd_batt = 0;
                update |= RDRW_BATTERY;
            }
            update |= RDRW_CLOCKTICK;
        }
        return update;
    }
    return 0;
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
        }
        if ( ui.pwr_dispdim )
        {
            DispHAL_SetContrast( core.setup.disp_brt_on );
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
             ( core.setup.pwr_off ) &&
             ( core.setup.pwr_off < ui.incativity) )
        {
            DispHAL_Display_Off();
            ui.pwr_state = SYSSTAT_UI_PWROFF;
        }
        else if ( ( core.setup.pwr_disp_off ) &&           // display off limit reached
                  ( core.setup.pwr_disp_off < ui.incativity) )
        {
            DispHAL_Display_Off();
            ui.pwr_dispdim = true;
            ui.pwr_dispoff = true;
            ui.pwr_state = SYSSTAT_UI_STOPPED;
            if ( ui.main_mode == OPMODE_CABLE )             // if in cable release mode - set display of with immediate action on "Start" button
                ui.pwr_state = SYSSTAT_UI_STOP_W_SKEY;
        }
        else if ( ( (ui.pwr_state & (SYSSTAT_UI_STOPPED | SYSSTAT_UI_STOP_W_ALLKEY | SYSSTAT_UI_STOP_W_SKEY)) == 0) && // display dimmed, ui stopped - waiting for interrupts
                  ( ui.pwr_dispdim == false ) &&
                  ( core.setup.pwr_stdby ) &&
                  ( core.setup.pwr_stdby < ui.incativity) )
        {
            DispHAL_SetContrast( core.setup.disp_brt_dim );
            ui.pwr_dispdim = true;
        } 
    }
}

static void uist_goto_shutdown(void)
{
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
    ui.m_substate = 50;    // 2sec. startup screen
}


void uist_startup( struct SEventStruct *evmask )
{
    if ( evmask->timer_tick_10ms )
    {
        ui.m_substate--;
    }

    if ( evmask->key_event || (ui.m_substate == 0) )
    {
        ui.m_state = UI_STATE_MAIN_GAUGE;
        ui.m_substate = 0;
        ui.main_mode = UIMODE_GAUDE_THERMO;
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

/// UI MAIN GAUGE WINDOW

void uist_mainwindowgauge_entry( void )
{
    uist_setupview_mainwindow( true );
    uist_drawview_mainwindow( RDRW_ALL );
    DispHAL_UpdateScreen();
    ui.m_substate ++;
}


void uist_mainwindowgauge( struct SEventStruct *evmask )
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
            if ( evmask->key_pressed & KEY_LEFT )
            {
                if ( ui.focus < ui.ui_elem_nr )
                {
                    ui.focus++;
                    disp_update |= RDRW_UI_CONTENT;
                }
            }

            // move focus to the previous element
            if ( evmask->key_pressed & KEY_RIGHT )
            {
                if ( ui.focus > 1 )
                {
                    ui.focus--;
                    disp_update |= RDRW_UI_CONTENT;
                }
            }
        }
        else
        {
            // operations if no focus yet - OK presssed
            if ( (evmask->key_pressed & KEY_UP) && (ui.main_mode > 0) )
            {
                ui.main_mode--;
                uist_setupview_mainwindow( true );
                disp_update = RDRW_ALL;
            }
            if ( (evmask->key_pressed & KEY_DOWN) && (ui.main_mode < UIMODE_GAUGE_PRESSURE) )
            {
                ui.main_mode++;
                uist_setupview_mainwindow( true );
                disp_update = RDRW_ALL;
            }
            if ( evmask->key_pressed & KEY_OK )    // enter in edit mode
            {
                ui.focus = 1;
                disp_update |= RDRW_UI_CONTENT;

                if ( ui.ui_elem_nr == 1 )
                {
                    // since these modes have only one ui element poll this control again to select for editting
                    uist_drawview_mainwindow( RDRW_UI_CONTENT );
                    ui_element_poll( ui.ui_elems[0], evmask );
                }
            }
        }

        // power button activated
        if ( evmask->key_longpressed & KEY_MODE )
        {
            uist_goto_shutdown();
        }

        // esc button
        if ( evmask->key_pressed & KEY_ESC )
        {
            ui.focus = 0;
            disp_update |= RDRW_UI_CONTENT;
        }
    }

    // update screen on timebase
    disp_update |= uist_timebased_updates( evmask );
    uist_update_display( disp_update );
}



////////////////////////////////////////////////////
//
//   UI core
//
////////////////////////////////////////////////////

void ui_init( struct SCore *instance )
{
    //core = instance;

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
            case UI_STATE_MAIN_GAUGE:
                uist_mainwindowgauge_entry();
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
            case UI_STATE_MAIN_GAUGE:
                uist_mainwindowgauge( evmask );
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
