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


static void uist_mainwindow_statusbar( uint32 opmode, bool rdrw_all )
{
    int x;
    uigrf_draw_battery(120, 0, 50);


    if ( ui.m_state == UI_STATE_MAIN_GAUGE )
    {
        Graphic_Line( 25, 15, 127, 15 );
        // draw the page selection
        for ( x=0; x<3; x++ )
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

    if ( opmode & UIMODE_GAUDE_THERMO )
    {
        uibm_put_bitmap( x, 1, BMP_ICON_CABLEREL );
        x+= 14;
    }

    {
        timestruct tm;
        uint32 i;

        if ( rdrw_all )
        {
            // first time
            Graphic_SetColor(1);
            Graphic_FillRectangle( x + 1, 0, 117, 15, 1);
            Graphic_PutPixel( x + 1, 0, 0 );
            Graphic_PutPixel( 117, 0, 0 );
        }

        tm.hour = 0;
        tm.minute = 13;
        tm.second = 56;
        uigrf_puttime( 92, 4, uitxt_small, 0, tm, true, false );
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
    if ( redraw_all == RDRW_ALL )
    {
        uigrf_text( 40, 17, uitxt_small,  "temp:" );

    }
    ui_element_display( &ui.p.mgThermo.temp, ui.focus );
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
        if ( (redraw_type & RDRW_STATUSBAR) == RDRW_BATTERY )       // only battery should be redrawn
            uist_mainwindow_statusbar( 0, (redraw_type == RDRW_ALL) );
        else                                                        // else redraw the whole status bar
            uist_mainwindow_statusbar( ui.main_mode, (redraw_type == RDRW_ALL) );
    }

    // if UI content should be redrawn
    if ( redraw_type & (~RDRW_STATUSBAR) )                          // if there are other things to be redrawn from the UI
    {
        switch ( ui.main_mode )
        {
            case UIMODE_GAUDE_THERMO:   uist_draw_gauge_thermo(redraw_type);  break;
            case UIMODE_GAUGE_HYGRO:        break;
            case UIMODE_GAUGE_PRESSURE:     break;
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
        case UIMODE_GAUGE_HYGRO:       break;
        case UIMODE_GAUGE_PRESSURE:    break;
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
        }
        else if ( ( (ui.pwr_state & (SYSSTAT_UI_STOPPED | SYSSTAT_UI_STOP_W_ALLKEY | SYSSTAT_UI_STOP_W_SKEY)) == 0) && // display dimmed, ui stopped - waiting for interrupts
                  ( ui.pwr_dispdim == false ) &&
                  ( core->setup.pwr_stdby ) && 
                  ( core->setup.pwr_stdby < ui.incativity) )
        {
            DispHAL_SetContrast( core->setup.disp_brt_dim );
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
