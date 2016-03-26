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


static void local_uist_display_init(void)
{
    if ( ui.pwr_dispinit == false )
    {
        Graphics_Init( NULL, NULL );
        DispHAL_Display_On( );
        DispHAL_SetContrast( core.nv.setup.disp_brt_on );
        ui.pwr_state = SYSSTAT_UI_ON;
        ui.pwr_dispdim = false;
        ui.pwr_dispoff = false;
        ui.pwr_dispinit = true;
    }
}

static void local_ui_set_shutdown_state(void)
{
    ui.m_state = UI_STATE_STARTUP;      // just put in start-up mode (wait for long-press if in shutdown)
    ui.m_substate = UI_SUBST_ENTRY;
    Graphics_ClearScreen(0);
    DispHAL_Display_Off();
    ui.pwr_dispdim = true;
    ui.pwr_dispoff = true;
    ui.pwr_state = SYSSTAT_UI_PWROFF;   // mark UI down
}


////////////////////////////////////////////////////
//
//   UI status dependent apply set value routines
//
////////////////////////////////////////////////////


static inline bool ui_imchanges_gauge_thermo( bool to_default )
{
    switch ( ui.focus )
    {
        case 1:                 // temperature measurement unit change actions
            if ( to_default )
            {
                ui.p.mgThermo.unitT = (enum ETemperatureUnits)core.nv.setup.show_unit_temp;
                uiel_control_list_set_index( &ui.p.mgThermo.units, ui.p.mgThermo.unitT );
                core.measure.dirty.b.upd_th_tendency = 1;       // force tendency graph update
                return true;
            }
            else
            {
                int val = uiel_control_list_get_index( &ui.p.mgThermo.units );
                if ( val != ui.p.mgThermo.unitT )
                {
                    ui.p.mgThermo.unitT = (enum ETemperatureUnits)val;
                    core.measure.dirty.b.upd_th_tendency = 1;   // force tendency graph update
                    return true;
                }
            }
            break;
        case 2:
        case 3:
        case 4:
            if ( to_default )
            {
                core_op_monitoring_reset_minmax( ss_thermo, GET_MM_SET_SELECTOR( core.nv.setup.show_mm_temp, ui.focus - 2 ) );
                return true;
            }
            else
            {
                int val = uiel_control_list_get_value( ui.ui_elems[ui.focus-1] );
                if ( val != GET_MM_SET_SELECTOR( core.nv.setup.show_mm_temp, ui.focus - 2 ) )
                {
                    SET_MM_SET_SELECTOR( core.nv.setup.show_mm_temp, val, ui.focus - 2 );
                    return true;
                }
            }
            break;
    }
    return false;
}

static inline bool ui_imchanges_gauge_hygro( bool to_default )
{
    switch ( ui.focus )
    {
        case 1:                 // temperature measurement unit change actions
            if ( to_default )
            {
                ui.p.mgHygro.unitH = (enum EHumidityUnits)core.nv.setup.show_unit_hygro;
                uiel_control_list_set_index( &ui.p.mgHygro.units, ui.p.mgHygro.unitH );
                core.measure.dirty.b.upd_th_tendency = 1;       // force tendency graph update
                return true;
            }
            else
            {
                int val = uiel_control_list_get_index( &ui.p.mgHygro.units );
                if ( val != ui.p.mgHygro.unitH )
                {
                    ui.p.mgHygro.unitH = (enum EHumidityUnits)val;
                    core.measure.dirty.b.upd_th_tendency = 1;   // force tendency graph update
                    return true;
                }
            }
            break;
        case 2:
        case 3:
        case 4:
            if ( to_default )
            {
                core_op_monitoring_reset_minmax( ss_rh, GET_MM_SET_SELECTOR( core.nv.setup.show_mm_hygro, ui.focus - 2 ) );
                return true;
            }
            else
            {
                int val = uiel_control_list_get_value( ui.ui_elems[ui.focus-1] );
                if ( val != GET_MM_SET_SELECTOR( core.nv.setup.show_mm_hygro, ui.focus - 2 ) )
                {
                    SET_MM_SET_SELECTOR( core.nv.setup.show_mm_hygro, val, ui.focus - 2 );
                    return true;
                }
            }
            break;
    }
    return false;
}

static inline bool ui_imchanges_gauge_pressure( bool to_default )
{
    
}


static bool ui_process_intermediate_changes( bool to_default )
{
    switch ( ui.m_state )
    {
        case UI_STATE_MAIN_GAUGE:
            switch ( ui.main_mode )
            {
                case UImm_gauge_thermo: return ui_imchanges_gauge_thermo(to_default);
                case UImm_gauge_hygro: return ui_imchanges_gauge_hygro(to_default);
                case UImm_gauge_pressure: return ui_imchanges_gauge_pressure(to_default);
            }
            break;
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
            uist_drawview_mainwindow( disp_update & RDRW_ALL  );

        DispHAL_UpdateScreen();
    }
}

static int uist_timebased_updates( struct SEventStruct *evmask )
{
    if ( evmask->timer_tick_10ms )
    {
        int update = 0;
        
        switch ( ui.m_state )
        {
            case UI_STATE_MAIN_GAUGE:
                switch ( ui.main_mode )
                {
                    case UImm_gauge_thermo:
                        if ( core.measure.dirty.b.upd_temp )
                            update |= RDRW_UI_DYNAMIC;
                        if ( core.measure.dirty.b.upd_temp_minmax ||
                             core.measure.dirty.b.upd_th_tendency )
                            update |= RDRW_UI_CONTENT;
                        break;
                    case UImm_gauge_hygro:
                        if ( core.measure.dirty.b.upd_hygro )
                            update |= RDRW_UI_DYNAMIC;
                        if ( core.measure.dirty.b.upd_hum_minmax ||
                             core.measure.dirty.b.upd_abshum_minmax ||
                             core.measure.dirty.b.upd_th_tendency )
                            update |= RDRW_UI_CONTENT;
                        break;
                    case UImm_gauge_pressure:
                        if ( core.measure.dirty.b.upd_pressure )
                            update |= RDRW_UI_DYNAMIC;
                        if ( core.measure.dirty.b.upd_press_minmax ||
                             core.measure.dirty.b.upd_press_tendency )
                            update |= RDRW_UI_CONTENT;
                        break;
                }
                break;
            case UI_STATE_MODE_SELECT:
                break;
        }
    
        if ( evmask->timer_tick_05sec )
        {
            ui.upd_batt++;
            if( (ui.upd_batt == 10) || core.measure.dirty.b.upd_battery )          // battery status update on every 5sec
            {
                core.measure.dirty.b.upd_battery = 0;
                ui.upd_batt = 0;
                update |= RDRW_BATTERY;
            }
            update |= RDRW_CLOCKTICK;
            if ( ui.upd_time != (core_get_clock_counter() / (60*2)) )
            {
                ui.upd_time = (core_get_clock_counter() / (60*2));
                update |= RDRW_STATUSBAR;
            }
        }
        return update;
    }
    return 0;
}

static void uist_goto_shutdown(void)
{
    core_op_realtime_sensor_select( ss_none );
    ui.m_state = UI_STATE_SHUTDOWN;
    ui.m_substate = 0;
}



static inline void ui_power_management( struct SEventStruct *evmask )
{
    if ( ui.pwr_state == SYSSTAT_UI_PWROFF )
        return;

    uint32 crt_rtc = core_get_clock_counter();

    if ( evmask->key_event )
    {
        ui.pwr_sched_dim = CORE_SCHED_NONE;
        ui.pwr_sched_dispoff = CORE_SCHED_NONE;
        ui.pwr_sched_pwroff = CORE_SCHED_NONE;
        // reset inactivity counter
        if ( core.nv.setup.pwr_stdby )
            ui.pwr_sched_dim        = crt_rtc + core.nv.setup.pwr_stdby;
        if ( core.nv.setup.pwr_disp_off )
            ui.pwr_sched_dispoff    = crt_rtc + core.nv.setup.pwr_disp_off;
        if ( core.nv.setup.pwr_off )
            ui.pwr_sched_pwroff     = crt_rtc + core.nv.setup.pwr_off;

        // for the case when UI was shut down - ony longpress on pwr/mode button is allowed
        // if key activity detected (most probably from pwr/mode - because evnets are set up for this only) then start up in
        // wake-up mode
        if ( ui.pwr_dispinit == false )
            return;

        if ( ui.pwr_state != SYSSTAT_UI_ON )
        {
            ui.pwr_state = SYSSTAT_UI_ON;
            // clear the key events since ui was in off state - just wake it up
            evmask->key_event = 0;
            evmask->key_released = 0;
            evmask->key_pressed = 0;
            evmask->key_longpressed = 0;
            core_restart_rtc_tick();
        }
        if ( ui.pwr_dispdim )
        {
            DispHAL_SetContrast( core.nv.setup.disp_brt_on );
            ui.pwr_dispdim = false;
        }
        if ( ui.pwr_dispoff )
        {
            DispHAL_Display_On();
            ui.pwr_dispoff = false;
        }

        if ( ui.m_state == UI_STATE_MAIN_GAUGE )
            core_op_realtime_sensor_select( (enum ESensorSelect)(ui.main_mode + 1) );
    }
    else if ( evmask->timer_tick_05sec )
    {
        if ( crt_rtc >= ui.pwr_sched_pwroff )       // power off time limit reached
        {
            ui.pwr_sched_pwroff = CORE_SCHED_NONE;
            local_ui_set_shutdown_state();
        }
        else if ( crt_rtc >= ui.pwr_sched_dispoff ) // display off limit reached
        {
            ui.pwr_sched_dispoff = CORE_SCHED_NONE;
            ui.pwr_dispdim = true;
            ui.pwr_dispoff = true;
            DispHAL_Display_Off();
            ui.pwr_state = SYSSTAT_UI_STOP_W_ALLKEY;
            core_op_realtime_sensor_select( ss_none );
        }
        else if ( ( (ui.pwr_state & (SYSSTAT_UI_STOPPED | SYSSTAT_UI_STOP_W_ALLKEY)) == 0) && // display dimmed, ui stopped - waiting for interrupts
                  ( ui.pwr_dispdim == false ) &&
                  ( core.nv.setup.pwr_stdby ) &&
                  ( crt_rtc >= ui.pwr_sched_dim ) )
        {
            ui.pwr_sched_dim = CORE_SCHED_NONE;
            DispHAL_SetContrast( core.nv.setup.disp_brt_dim );
            ui.pwr_dispdim = true;
            ui.pwr_state = SYSSTAT_UI_STOP_W_ALLKEY;
        } 
    }
}


////////////////////////////////////////////////////
//
//   UI STATUS ROUTINES
//
////////////////////////////////////////////////////

/// UI STARTUP

void uist_startup_entry( void )
{
    ui.m_substate = 110;
}


void uist_startup( struct SEventStruct *evmask )
{
    if ( evmask->timer_tick_10ms )
    {
        ui.m_substate--;
    }

    if ( evmask->key_event )
    {
        if ( evmask->key_pressed & KEY_MODE )           // if power button is pressed - put in long-press waiting state
        {
            ui.pwr_state = SYSSTAT_UI_ON;
            ui.m_substate = 110;
        }
        else if ( evmask->key_longpressed & KEY_MODE )  // when long press on power button
        {
            local_uist_display_init();
            ui.m_state = UI_STATE_MAIN_GAUGE;
            ui.m_substate = 0;
            ui.main_mode = UImm_gauge_thermo;
            return;
        }
    }

    if ( ui.m_substate == 0 )
    {
        // no pwr.on long press, timed out - power down
        ui.pwr_state = SYSSTAT_UI_PWROFF;
    }
}


/// UI DEBUG INPUTS

void uist_debuginputs_entry( void )
{
    uist_setupview_debuginputs();
    uist_drawview_debuginputs( RDRW_ALL, 0 );
    DispHAL_UpdateScreen();
    ui.m_substate ++;
    ui.upd_dynamics = 0;
    ui.upd_batt = 0;
}

void uist_debuginputs( struct SEventStruct *evmask )
{
    if ( evmask->timer_tick_10ms || evmask->key_event )
    {
        uint32 bits = 0;
        bool update = false;

        // MO ES OK S1 S2 S3 S4
        if ( BtnGet_Mode() )
            bits |= ( 0x01 << 24 );
        if ( BtnGet_Esc() )
            bits |= ( 0x02 << 24 );
        if ( BtnGet_OK() )
            bits |= ( 0x04 << 24 );
        if ( BtnGet_Up() )
            bits |= ( 0x08 << 24 );
        if ( BtnGet_Down() )
            bits |= ( 0x10 << 24 );
        if ( BtnGet_Left() )
            bits |= ( 0x20 << 24 );
        if ( BtnGet_Right() )
            bits |= ( 0x40 << 24 );

        if ( evmask->key_event )
        {
            bits |= evmask->key_pressed | (evmask->key_released << 8) | (evmask->key_longpressed << 16);
            update = true;
            ui.upd_batt = 8;
        }

        if ( evmask->timer_tick_10ms )
        {
            if (ui.upd_batt)
                ui.upd_batt--;
            else
            {
                ui.upd_dynamics++;
                if ( ui.upd_dynamics == 5 )     // obtain 20fps for dynamic stuff update
                {
                    update = true;
                    ui.upd_dynamics = 0;
                }
            }
        }

        if (update)
        {
            uist_drawview_debuginputs( RDRW_UI_DYNAMIC, bits );
            DispHAL_UpdateScreen();
        }
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
        local_ui_set_shutdown_state();
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

/// OPMODE SELECTOR WINDOW

void uist_opmodeselect_entry( void )
{
    uist_setupview_modeselect( true );
    uist_drawview_modeselect( RDRW_ALL );
    DispHAL_UpdateScreen();
    ui.m_substate ++;
}

void uist_opmodeselect( struct SEventStruct *evmask )
{
    int disp_update = 0;
    if ( evmask->key_event )
    {
        if ( evmask->key_pressed & KEY_UP )
        {
            ui.m_state = UI_STATE_MAIN_GAUGE;
            ui.m_substate = UI_SUBST_ENTRY;
        }
        if ( evmask->key_longpressed & KEY_MODE )
        {
            uist_goto_shutdown();
        }
    }

    // update screen on timebase
    disp_update |= uist_timebased_updates( evmask );
    uist_update_display( disp_update );
}


/// UI MAIN GAUGE WINDOW

void uist_mainwindowgauge_entry( void )
{
    core.measure.dirty.b.upd_th_tendency = 1;       // force an update on the tendency graph values
    uist_setupview_mainwindow( true );
    uist_drawview_mainwindow( RDRW_ALL );
    DispHAL_UpdateScreen();
    core_op_realtime_sensor_select( (enum ESensorSelect)(ui.main_mode + 1) );
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
            {
                disp_update  = RDRW_DISP_UPDATE;          // mark only for dispHAL update
                if ( ui_process_intermediate_changes( false ) )
                    disp_update |= RDRW_UI_CONTENT_ALL;
            }

            // move focus to the next element
            if ( evmask->key_pressed & KEY_RIGHT )
            {
                if ( ui.focus < ui.ui_elem_nr )
                    ui.focus++;
                else
                    ui.focus = 1;
                disp_update |= RDRW_UI_CONTENT;
            }
            // move focus to the previous element
            if ( evmask->key_pressed & KEY_LEFT )
            {
                if ( ui.focus > 1 )
                    ui.focus--;
                else
                    ui.focus = ui.ui_elem_nr - 1;
                disp_update |= RDRW_UI_CONTENT;
            }
            // short press on esc will exit focus
            if ( evmask->key_released & KEY_ESC )
            {
                ui.focus = 0;
                disp_update |= RDRW_UI_CONTENT;
            }
            // long press on escape
            if ( evmask->key_longpressed & KEY_ESC )
            {
                ui_process_intermediate_changes( true );
                disp_update |= RDRW_UI_CONTENT_ALL;
            }

        }
        else
        {
            // operations if no focus yet - OK presssed
            if ( (evmask->key_pressed & KEY_LEFT) && (ui.main_mode > 0) )
            {
                ui.main_mode--;
                uist_setupview_mainwindow( true );
                disp_update = RDRW_ALL;
                core.measure.dirty.b.upd_th_tendency = 1;       // force an update on the tendency graph values
                core_op_realtime_sensor_select( (enum ESensorSelect)(ui.main_mode + 1) );
            }
            if ( (evmask->key_pressed & KEY_RIGHT) && (ui.main_mode < UImm_gauge_pressure) )
            {
                ui.main_mode++;
                uist_setupview_mainwindow( true );
                disp_update = RDRW_ALL;
                core.measure.dirty.b.upd_th_tendency = 1;       // force an update on the tendency graph values
                core_op_realtime_sensor_select( (enum ESensorSelect)(ui.main_mode + 1) );
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
        if ( evmask->key_released & KEY_MODE )
        {
            ui_process_intermediate_changes( false );
            core_op_realtime_sensor_select( ss_none );
            ui.m_state = UI_STATE_MODE_SELECT;
            ui.m_substate = UI_SUBST_ENTRY;
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

uint32 ui_init( struct SCore *instance )
{
    uint32 core_cmd;

    memset( &ui, 0, sizeof(ui) );
    ui.pwr_state = SYSSTAT_UI_PWROFF;
    core_cmd = core.vstatus.ui_cmd;

    if ( core_cmd &= CORE_UISTATE_START_LONGPRESS )
    {
        ui.m_state = UI_STATE_STARTUP;
        ui.pwr_state = SYSSTAT_UI_ON;
    }


    return ui.pwr_state;
}//END: ui_init


uint32 ui_poll( struct SEventStruct *evmask )
{
    struct SEventStruct evm_bkup = *evmask;         // save a backup since ui routines can alter states, and those will not be cleared then

    if ( ui.m_state == UI_STATE_NONE)
        return SYSSTAT_UI_PWROFF;

    // process power management stuff for ui part
    ui_power_management( evmask );

    // ui state machine
    if ( (ui.pwr_state & SYSSTAT_UI_PWROFF) == 0 )
    {
        if ( ui.m_substate == UI_SUBST_ENTRY )
        {
            switch ( ui.m_state )
            {
                case UI_STATE_MAIN_GAUGE:
                    uist_mainwindowgauge_entry();
                    break;
                case UI_STATE_MODE_SELECT:
                    uist_opmodeselect_entry();
                    break;
                case UI_STATE_STARTUP:
                    uist_startup_entry();
                    break;
                case UI_STATE_SHUTDOWN:
                    uist_shutdown_entry();
                    break;
                case UI_STATE_DBG_INPUTS:
                    uist_debuginputs_entry();
                    break;
            }
        }
        else
        {
            switch ( ui.m_state )
            {
                case UI_STATE_MAIN_GAUGE:
                    uist_mainwindowgauge( evmask );
                    break;
                case UI_STATE_MODE_SELECT:
                    uist_opmodeselect( evmask );
                    break;
                case UI_STATE_STARTUP:
                    uist_startup( evmask );
                    break;
                case UI_STATE_SHUTDOWN:
                    uist_shutdown( evmask );
                    break;
                case UI_STATE_DBG_INPUTS:
                    uist_debuginputs( evmask );
                    break;
            }
        }
    }

    *evmask = evm_bkup;
    
    return ui.pwr_state;
}
