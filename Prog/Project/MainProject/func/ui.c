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

void uist_enter_popup( uint8 cont_ok, uiel_callback call_ok, uint8 cont_cancel, uiel_callback call_cancel );
void uist_close_popup(void);
void uist_change_state( enum EUIStates m_state, enum EUISetupSelect set_state, bool init );
uint32 internal_regtaskmem_maxlenght( struct SRecTaskInstance task );


static void internal_pwrmngm_reset_inactivities( uint32 ctr )
{
    if ( core.nv.setup.pwr_stdby )
        ui.pwr_sched_dim        = ctr + core.nv.setup.pwr_stdby;
    if ( core.nv.setup.pwr_disp_off )
        ui.pwr_sched_dispoff    = ctr + core.nv.setup.pwr_disp_off;
    if ( core.nv.setup.pwr_off )
        ui.pwr_sched_pwroff     = ctr + core.nv.setup.pwr_off;
}

static void local_uist_display_init(void)
{
    if ( ui.pwr_dispinit == false )
    {
        Graphics_Init( NULL, NULL );
        DispHal_GreySetup( core.nv.setup.grey_disprate, core.nv.setup.grey_frame_all, core.nv.setup.grey_frame_grey );
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

    if ( core.vstatus.ui_cmd & CORE_UISTATE_CHARGING )
        ui.pwr_state = SYSSTAT_UI_ON;       // in charging state don't mark UI pwr off
    else
        ui.pwr_state = SYSSTAT_UI_PWROFF;   // mark UI down

}

void internal_get_regtask_set_from_ui(struct SRecTaskInstance *task)
{
    int i;
    uint32 elems = 0;
    for (i=0; i<3; i++)
    {
        if ( uiel_control_checkbox_get( &ui.p.swRegTaskSet.THP[i] ) )
            elems |= (1<<i);
    }
    if ( elems == 0 )       // do not permit 0 value
        elems = rtt_t;

    task->task_elems = elems;
    task->size = ui.p.swRegTaskSet.task.size;
    task->sample_rate = uiel_control_list_get_value( &ui.p.swRegTaskSet.m_rate );
    task->mempage = ui.p.swRegTaskSet.task.mempage;
}

bool internal_graph_is_zoomed(void)
{
    if ( (ui.p.grDisp.view_elemstart < core.nvrec.func[ui.m_return].c) ||
         (ui.p.grDisp.view_elemend > 0) )
        return true;
    return false;
}

uint32 internal_graphdisp_cursor2samplenr( uint32 cursor )
{
    // convert local cursor display cursor position to global samplenr
    uint32 smpl;
    smpl = ((ui.p.grDisp.view_elemstart - ui.p.grDisp.view_elemend) * (WB_DISPPOINT - cursor)) / WB_DISPPOINT + ui.p.grDisp.view_elemend;
    return smpl;
}


static enum ESensorSelect internal_graphdisp_get_element( uint32 index )
{
    uint32 elem;

    elem = core.nvrec.task[ui.m_return].task_elems & (1<<index);
    switch ( elem )
    {
        case 0x01:  return ss_thermo;
        case 0x02:  return ss_rh;
        case 0x04:  return ss_pressure;
        default:
            return ss_none;
    }
    return ss_none;
}

static bool internal_graphdisp_operate_elemselect( bool increase )
{
    uint32 elem;

    elem = ui.p.grDisp.view_elem;
    if ( increase )
    {
        while ( elem < 2 )
        {
            elem++;
            if ( internal_graphdisp_get_element(elem) )
                goto _okvalue;
        }
    }
    else
    {
        while ( elem > 0 )
        {
            elem--;
            if ( internal_graphdisp_get_element(elem) )
                goto _okvalue;
        }
    }
    return false;
_okvalue:
    if ( elem != ui.p.grDisp.view_elem )
    {
        ui.p.grDisp.view_elem = elem;
        ui.upd_ui_disp |= RDRW_UI_CONTENT;
        return true;
    }
    return false;
}

static void internal_graphdisp_adjust_cursor( bool c1_master )
{
    // c1 should be <= c2. Area under c1/c2 has width
    // - so at 1100 samples, no zoom, c1=c2 - samples under cursor: 110 samples
    //  c2 = c1 + 1 -> 220 samples
    uint32 smpl1;
    uint32 smpl2;

    if ( c1_master && (ui.p.grDisp.view_cursor2 < ui.p.grDisp.view_cursor1) )
        ui.p.grDisp.view_cursor2 = ui.p.grDisp.view_cursor1;
    if ( (c1_master == false) && (ui.p.grDisp.view_cursor1 > ui.p.grDisp.view_cursor2) )
        ui.p.grDisp.view_cursor1 = ui.p.grDisp.view_cursor2;

    do
    {
        smpl1 = internal_graphdisp_cursor2samplenr( ui.p.grDisp.view_cursor1 );
        smpl2 = internal_graphdisp_cursor2samplenr( ui.p.grDisp.view_cursor2 + 1 );

        if ( (smpl1 - smpl2) >= WB_DISPPOINT )
            break;

        if ( c1_master )
        {
            if ( ui.p.grDisp.view_cursor2 < (WB_DISPPOINT-1) )
                ui.p.grDisp.view_cursor2++;
            else if ( ui.p.grDisp.view_cursor1 > 0)
                ui.p.grDisp.view_cursor1--;
        }
        else
        {
            if ( ui.p.grDisp.view_cursor1 > 0 )
                ui.p.grDisp.view_cursor1--;
            else if ( ui.p.grDisp.view_cursor2 < (WB_DISPPOINT-1))
                ui.p.grDisp.view_cursor2++;
        }
    } while ( 1 );
}



static bool internal_graphdisp_operate_cursor( bool increase, bool c2 )
{
    bool change = false;

    if ( (ui.p.grDisp.d_state & GRSTATE_SELECT_ZOOM) == 0 )
        c2 = false;

    if ( c2 == false )
    {
        if ( (ui.p.grDisp.view_cursor1 < (WB_DISPPOINT-1)) && increase )
        {
            ui.p.grDisp.view_cursor1++;
            if ( (ui.p.grDisp.d_state & GRSTATE_SELECT_ZOOM) == 0 )
                ui.p.grDisp.view_cursor2++;
            change = true;
        }
        else if ( (ui.p.grDisp.view_cursor1 > 0) && (increase == false) )
        {
            ui.p.grDisp.view_cursor1--;
            if ( (ui.p.grDisp.d_state & GRSTATE_SELECT_ZOOM) == 0 )
                ui.p.grDisp.view_cursor2--;
            change = true;
        }
    }
    else
    {
        if ( (ui.p.grDisp.view_cursor2 < (WB_DISPPOINT-1)) && increase )
        {
            ui.p.grDisp.view_cursor2++;
            change = true;
        }
        else if ( (ui.p.grDisp.view_cursor2 > 0) && (increase == false) )
        {
            ui.p.grDisp.view_cursor2--;
            change = true;
        }
    }

    if ( ui.p.grDisp.d_state & GRSTATE_SELECT_ZOOM )
        internal_graphdisp_adjust_cursor( c2 == false );

    return change;
}


static inline void internal_graphdisp_btnpress_OK(void)
{
    bool rebuild = false;

    switch ( ui.p.grDisp.d_state & GRSTATE_MASK )
    {
        case GRSTATE_DISP:                          // from display the OK button enters in the graph menu
            ui.p.grDisp.d_state = GRSTATE_MENU;
            ui.p.grDisp.d_upd_ctr = 0;
            DispHal_ClearFlipBuffer();
            rebuild = true;
            break;
        case GRSTATE_DETAIL:
        case GRSTATE_SELECT_ZOOM:
            if ( (ui.p.grDisp.d_state & GRSTATE_MASK) == GRSTATE_SELECT_ZOOM )
            {
                uint32 smpl1;
                uint32 smpl2;

                smpl1 = internal_graphdisp_cursor2samplenr( ui.p.grDisp.view_cursor1 );
                smpl2 = internal_graphdisp_cursor2samplenr( ui.p.grDisp.view_cursor2 + 1 );

                ui.p.grDisp.view_elemstart = smpl1;
                ui.p.grDisp.view_elemend = smpl2;

                core_op_recording_read_cancel();   
                core_op_recording_read_request( ui.m_return, ui.p.grDisp.view_elemstart, ui.p.grDisp.view_elemstart - ui.p.grDisp.view_elemend );    // get the entire recording
                ui.p.grDisp.d_state = GRSTATE_DISP | GRSTATE_FLAG_FILL;
                ui.p.grDisp.graph_dirty = 1;
            }
            else
                ui.p.grDisp.d_state = GRSTATE_DISP;

            ui.p.grDisp.view_cursor2 = ui.p.grDisp.view_cursor1;
            ui.p.grDisp.d_upd_ctr = 0;
            rebuild = true;
            break;
    }

    if ( rebuild )
    {
        ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
        uist_setupview_mainwindow(false);
    }

}


static inline void internal_graphdisp_btnpress_ESC(void)
{
    bool rebuild = false;

    switch ( ui.p.grDisp.d_state & GRSTATE_MASK )
    {
        // GRSTATE_DISP, GRSTATE_MENU, GRSTATE_ZOOM_MENU are handled through callbacks
        case GRSTATE_SELECT_ZOOM:
        case GRSTATE_DETAIL:
            ui.p.grDisp.d_state = GRSTATE_DISP;
            ui.p.grDisp.view_cursor2 = ui.p.grDisp.view_cursor1;
            ui.p.grDisp.d_upd_ctr = 0;
            rebuild = true;
            break;
    }

    if ( rebuild )
    {
        ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
        uist_setupview_mainwindow(false);
    }
}


////////////////////////////////////////////////////
//
//   UI callbacks
//
//   General rule:
//      Do not change UI states directly
//      Use uist_change_state() routine instead
//
////////////////////////////////////////////////////

// Generic gauge callbacks
void ui_call_maingauge_esc_pressed( int context, void *pval )
{
    // ESC pressed on an object in focus in non-editting mode - clear the focus
    ui.focus = 0;
    ui.upd_ui_disp |= RDRW_UI_CONTENT;
}

// Thermo gauge callbacks

void ui_call_maingauge_thermo_unit_toDefault( int context, void *pval )
{
    ui.p.mgThermo.unitT = (enum ETemperatureUnits)core.nv.setup.show_unit_temp;
    uiel_control_list_set_index( &ui.p.mgThermo.units, ui.p.mgThermo.unitT );
    core.measure.dirty.b.upd_th_tendency = 1;       // force tendency graph update
    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;          // set all-content update flag for the next display update
}

void ui_call_maingauge_thermo_unit_vchange( int context, void *pval )
{
    int val = *((int*)pval);         // index is the same as the value - we can use it directly

    if ( val != ui.p.mgThermo.unitT )
    {
        ui.p.mgThermo.unitT = (enum ETemperatureUnits)val;
        core.measure.dirty.b.upd_th_tendency = 1;               // force tendency graph update
        ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;                  // set all-content update flag for the next display update
    } 
}

void ui_call_maingauge_thermo_minmax_toDefault( int context, void *pval )
{
    core_op_monitoring_reset_minmax( ss_thermo, GET_MM_SET_SELECTOR( core.nv.setup.show_mm_temp, context ) );
    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;                  // set all-content update flag for the next display update
}

void ui_call_maingauge_thermo_minmax_vchange( int context, void *pval )
{
    int val = *((int*)pval);                                // index is the same as the value - we can use it directly
    if ( val != GET_MM_SET_SELECTOR( core.nv.setup.show_mm_temp, context ) )
    {
        SET_MM_SET_SELECTOR( core.nv.setup.show_mm_temp, val, context );
        ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
    }
}

// Hygro gauge callbacks

void ui_call_maingauge_hygro_unit_toDefault( int context, void *pval )
{
    ui.p.mgHygro.unitH = (enum EHumidityUnits)core.nv.setup.show_unit_hygro;
    uiel_control_list_set_index( &ui.p.mgHygro.units, ui.p.mgHygro.unitH );
    core.measure.dirty.b.upd_th_tendency = 1;       // force tendency graph update
    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;          // set all-content update flag for the next display update
}

void ui_call_maingauge_hygro_unit_vchange( int context, void *pval )
{
    int val = *((int*)pval);         // index is the same as the value - we can use it directly

    if ( val != ui.p.mgHygro.unitH )
    {
        ui.p.mgHygro.unitH = (enum EHumidityUnits)val;
        core.measure.dirty.b.upd_th_tendency = 1;   // force tendency graph update
        ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;                  // set all-content update flag for the next display update
    }
}

void ui_call_maingauge_hygro_minmax_toDefault( int context, void *pval )
{
    core_op_monitoring_reset_minmax( ss_rh, GET_MM_SET_SELECTOR( core.nv.setup.show_mm_hygro, context ) );
    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;                  // set all-content update flag for the next display update
}

void ui_call_maingauge_hygro_minmax_vchange( int context, void *pval )
{
    int val = *((int*)pval);                                // index is the same as the value - we can use it directly
    if ( val != GET_MM_SET_SELECTOR( core.nv.setup.show_mm_hygro, context ) )
    {
        SET_MM_SET_SELECTOR( core.nv.setup.show_mm_hygro, val, context );
        ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
    }
}


// --- Setup window callbacks

// --- Setup quick switches 

const char popup_msg_op_monitoring[] =  "tendency graph will be";
const char popup_msg_op_registering[] = "Recorded data will be";
const char popup_msg_op_monitoring2[] = "discarded at restart!";
const char popup_msg_op_2[] =          "Proceed?";
const char popup_msg_op_register_nosel1[] =  "no task is selected for";
const char popup_msg_op_register_nosel2[] =  "recording. select one first";

void ui_call_setwindow_quickswitch_op_switch( int context, void *pval )
{
    bool val = *((bool*)pval);

    if ( val == true )
    {
        // start monitoring / registering
        if ( context == 0 )
            core_op_monitoring_switch(true);
        else
        {
            if ( core.nvrec.running == 0 )
            {
                ui.popup.params.line1 = (uint32)popup_msg_op_register_nosel1;       // need to trick the compiler to not to complain about const type
                ui.popup.params.line2 = (uint32)popup_msg_op_register_nosel2;
                ui.popup.params.line3 = 0;
                ui.popup.params.style1 = uitxt_micro;
                ui.popup.params.x1 = 4;
                ui.popup.params.y1 = 8;
                ui.popup.params.y2 = 16;
                ui.popup.params.popup_action = uipa_close;
                uist_enter_popup( 0, NULL, 0, NULL );
                // reset switch
                uiel_control_checkbox_set( &ui.p.swQuickSw.reg, false );
            }
            else
                core_op_recording_switch(true);
        }
    }
    else
    {
        // stop monitoring / registering
        // context 0 - monitoring,   context 1 - registering
        if ( context == 0 )
            ui.popup.params.line1 = (uint32)popup_msg_op_monitoring;       // need to trick the compiler to not to complain about const type
        else
            ui.popup.params.line1 = (uint32)popup_msg_op_registering;       // need to trick the compiler to not to complain about const type

        ui.popup.params.line2 = (uint32)popup_msg_op_monitoring2;
        ui.popup.params.line3 = (uint32)popup_msg_op_2;
        ui.popup.params.style1 = uitxt_micro;
        ui.popup.params.style3 = uitxt_small;
        ui.popup.params.x1 = 15;
        ui.popup.params.y1 = 2;
        ui.popup.params.y2 = 10;
        ui.popup.params.x3 = 40;
        ui.popup.params.y3 = 18;
        ui.popup.params.popup_action = uipa_ok_cancel;
        uist_enter_popup( context, ui_call_setwindow_quickswitch_op_switch_ok, 
                          context, ui_call_setwindow_quickswitch_op_switch_cancel );
    }
}

void ui_call_setwindow_quickswitch_op_switch_ok( int context, void *pval )
{
    if ( context == 0 )
        core_op_monitoring_switch(false);
    else if ( context == 1 )
        core_op_recording_switch(false);
    else
    {

    }

    uist_close_popup();
}

void ui_call_setwindow_quickswitch_op_switch_cancel( int context, void *pval )
{
    if (context == 0)
        uiel_control_checkbox_set( (struct Suiel_control_checkbox*)(&ui.p.swQuickSw.monitor), true );
    else if ( context == 1)
        uiel_control_checkbox_set( (struct Suiel_control_checkbox*)(&ui.p.swQuickSw.reg), true );

    uist_close_popup();
}


void ui_call_setwindow_quickswitch_monitor_rate( int context, void *pval )
{
    // context points to the ui element, to obtain the sensor use context-1
    int val;
    val = uiel_control_list_get_value( (struct Suiel_control_list*)(ui.ui_elems[context]) );
    core_op_monitoring_rate( (enum ESensorSelect)(context + ss_thermo - 1), (enum EUpdateTimings)val );
}


void ui_call_setwindow_quickswitch_monitor_rate_val( int context, void *pval )
{
    ui.upd_ui_disp |= RDRW_UI_CONTENT;
}

void ui_call_setwindow_quickswitch_esc_pressed( int context, void *pval )
{
    // esc pressed - return to the mode selector window
    uist_change_state( UI_STATE_MODE_SELECT, UI_SET_NONE, true );
}


const char popup_msg_reset_minmax_1[] = "Really want to reset all";
const char popup_msg_reset_minmax_2[] = "min/max values ?";

void ui_call_setwindow_quickswitch_reset_minmax( int context, void *pval )
{
    ui.popup.params.line1 = (uint32)popup_msg_reset_minmax_1;       // need to trick the compiler to not to complain about const type
    ui.popup.params.line2 = 0;
    ui.popup.params.line3 = (uint32)popup_msg_reset_minmax_2;
    ui.popup.params.style1 = uitxt_small;
    ui.popup.params.style3 = uitxt_small;
    ui.popup.params.x1 = 10;
    ui.popup.params.y1 = 2;
    ui.popup.params.x3 = 10;
    ui.popup.params.y3 = 14;
    ui.popup.params.popup_action = uipa_ok_cancel;
    uist_enter_popup( 0, ui_call_setwindow_quickswitch_reset_minmax_ok, 0, NULL );
}

void ui_call_setwindow_quickswitch_reset_minmax_ok( int context, void *pval )
{
    // callback from the popup if OK is selected
    int i;
    for ( i=0; i<STORAGE_MINMAX; i++ )
    {
        core_op_monitoring_reset_minmax( ss_thermo, i );
        core_op_monitoring_reset_minmax( ss_rh, i );
        core_op_monitoring_reset_minmax( ss_pressure, i );
    }
    
    uist_close_popup();
}

void ui_call_setwindow_quickswitch_task_ok( int context, void *pval )
{
    uist_change_state( UI_STATE_NONE, UI_SET_RegTaskSet, true );
    ui.m_return = context + 1;          // task index is stored in m_return in 1-based value
}


// --- Setup registering tasks

const char popup_msg_regtaskset_setup1[] = "Setup changed, will discard";
const char popup_msg_regtaskset_setup2[] = "recorded data";
const char popup_msg_regtaskset_stop1[] = "Recording in progess";
const char popup_msg_regtaskset_stop2[] = "stop it?";
const char popup_msg_regtaskset_start1[] = "Recording started";
const char popup_msg_regtaskset_start2[] = "data will be discarded";

void ui_call_setwindow_regtaskset_next_action( int context, void *pval )
{
    // exitting from registering task set - check for changes
    struct SRecTaskInstance task_ui;
    bool change = false;

    internal_get_regtask_set_from_ui( &task_ui );
    if ( memcmp( &task_ui, &core.nvrec.task[ui.p.swRegTaskSet.task_index], sizeof(task_ui) ) )     // if setup is changed
    {   
        ui.p.swRegTaskSet.task = task_ui;
        ui.popup.params.line1 = (uint32)popup_msg_regtaskset_setup1;
        ui.popup.params.line2 = (uint32)popup_msg_regtaskset_setup2;
        change = true;
    }
    else 
    {
        if ( (core.nvrec.running & (1<<ui.p.swRegTaskSet.task_index)) &&
             (uiel_control_checkbox_get( &ui.p.swRegTaskSet.run ) == false)  )      // stopping
        {
            ui.popup.params.line1 = (uint32)popup_msg_regtaskset_stop1;
            ui.popup.params.line2 = (uint32)popup_msg_regtaskset_stop2;
            change = true;
        }
        else if (  ((core.nvrec.running & (1<<ui.p.swRegTaskSet.task_index)) == 0) &&
                   uiel_control_checkbox_get( &ui.p.swRegTaskSet.run )  )           // starting
        {
            ui.popup.params.line1 = (uint32)popup_msg_regtaskset_start1;
            ui.popup.params.line2 = (uint32)popup_msg_regtaskset_start2;
            change = true;
        }

        ui.p.swRegTaskSet.task.size = 0;        // mark that task setup is not changed
    }

    if ( change )
    {
        // if changes in setup - go through popup
        ui.popup.params.style1 = uitxt_small;
        ui.popup.params.style3 = uitxt_small;
        ui.popup.params.line3 = (uint32)popup_msg_op_2;
        ui.popup.params.x1 = 5;
        ui.popup.params.y1 = 2;
        ui.popup.params.y2 = 10;
        ui.popup.params.x3 = 40;
        ui.popup.params.y3 = 18;
        ui.popup.params.popup_action = uipa_ok_cancel;
        uist_enter_popup( context | UI_REG_OK_PRESSED, 
                          ui_call_setwindow_regtaskset_close, 
                          context, 
                          ui_call_setwindow_regtaskset_close );
    }
    else
    {
        // if no change at all - just proceed
        if ( context == UI_REG_TO_BEFORE )
            uist_change_state( UI_STATE_NONE, UI_SET_QuickSwitch, true );
        else
            uist_change_state( UI_STATE_NONE, UI_SET_RegTaskMem, true );
    }
    // ui.m_return - 1 indicates the task index
}

void ui_call_setwindow_regtaskset_close( int context, void *pval )
{
    // callback finished - there is for sure a change - apply it if UI_REG_OK_PRESSED flag is set in config
    if ( context & UI_REG_OK_PRESSED )
    {
        bool set_run;

        set_run = uiel_control_checkbox_get( &ui.p.swRegTaskSet.run );
        // apply new params if needed
        if ( ui.p.swRegTaskSet.task.size )
        {
            core_op_recording_setup_task( ui.p.swRegTaskSet.task_index, &ui.p.swRegTaskSet.task );
        }

        // change run mode if needed
        if ( set_run != ((core.nvrec.running & (1<<ui.p.swRegTaskSet.task_index)) != 0) )
            core_op_recording_task_run( ui.p.swRegTaskSet.task_index, set_run );
    }

    uist_close_popup();

    if ( context & UI_REG_TO_REALLOC )
        uist_change_state( UI_STATE_NONE, UI_SET_RegTaskMem, true );
    else
        uist_change_state( UI_STATE_NONE, UI_SET_QuickSwitch, true );
    // ui.m_return - 1 indicates the task index
}

void ui_call_setwindow_regtaskset_valch( int context, void *pval )
{
    struct SRecTaskInstance task_ui;
    uint32 max_len;

    // take care to not to exceed max. nr. of elements in a task
    internal_get_regtask_set_from_ui( &task_ui );
    max_len = internal_regtaskmem_maxlenght( task_ui );
    ui.p.swRegTaskSet.task.size = core.nvrec.task[ ui.p.swRegTaskSet.task_index ].size;

    if ( ui.p.swRegTaskSet.task.size > max_len )
        ui.p.swRegTaskSet.task.size = max_len;
    
    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
}


void ui_call_setwindow_dbg_gen_data( int context, void *pval )
{
    // hidden debug feature - generate test data
    core_op_recording_dbgfill( ui.p.swRegTaskSet.task_index );
}



// --- Setup registering task allocation

const char popup_msg_regtaskmem_change[] = "Data will be cleared";
char popup_msg_regtaskmem_tasks[] = "for tasks         ";

void ui_call_setwindow_regtaskmem_exit( int context, void *pval )
{
    int i;
    uint32 change = 0;

    // check for changes
    for (i=0; i<4; i++)
    {
        if ( memcmp( &ui.p.swRegTaskMem.task[i], &core.nvrec.task[i], sizeof(struct SRecTaskInstance) ) )
        {
            change |= ( 1 << i );
            popup_msg_regtaskmem_tasks[10] = i + '1';
        }
    }

    // set the selected task at the return window
    ui.m_return = uiel_control_numeric_get( &ui.p.swRegTaskMem.select );

    // if change - put callback
    if ( change )
    {
        ui.popup.params.line1 = (uint32)popup_msg_regtaskmem_change;
        ui.popup.params.line2 = (uint32)popup_msg_regtaskmem_tasks;
        ui.popup.params.line3 = (uint32)popup_msg_op_2;

        ui.popup.params.style1 = uitxt_small;
        ui.popup.params.style3 = uitxt_small;
        ui.popup.params.x1 = 5;
        ui.popup.params.y1 = 2;
        ui.popup.params.y2 = 10;
        ui.popup.params.x3 = 40;
        ui.popup.params.y3 = 18;
        ui.popup.params.popup_action = uipa_ok_cancel;
        uist_enter_popup( change, 
                          ui_call_setwindow_regtaskmem_exit_close, 
                          0, 
                          ui_call_setwindow_regtaskmem_exit_close );
    }
    else
        uist_change_state( UI_STATE_NONE, UI_SET_RegTaskSet, true );
}

void ui_call_setwindow_regtaskmem_exit_close( int context, void *pval )
{
    if ( context )
    {
        int i;
        for (i=0; i<4; i++)
        {
            if ( context & (1<<i) )
                core_op_recording_setup_task( i, &ui.p.swRegTaskMem.task[i] );
        }
    }
    uist_close_popup();
    uist_change_state( UI_STATE_NONE, UI_SET_RegTaskSet, true );
}


void ui_call_setwindow_regtaskmem_chtask( int context, void *pval )
{
    int val = *((int*)pval);
    val--;      // since it is 1 based

    ui.p.swRegTaskMem.task_index = val;
    uiel_control_numeric_set( &ui.p.swRegTaskMem.start, ui.p.swRegTaskMem.task[val].mempage );
    uiel_control_numeric_set( &ui.p.swRegTaskMem.lenght, ui.p.swRegTaskMem.task[val].size );

    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
}

bool internal_regtaskmem_inside( uint32 mstart, uint32 size, struct SRecTaskInstance task )
{
    if ( (mstart <= task.mempage) &&                // current addess <= checked task's address
         ((mstart+size) > task.mempage) )   // total lenght > checked task's address
        return true;

    if ( (mstart > task.mempage) &&
         ( mstart < (task.mempage+task.size)) )
        return true;
        
    return false;
}

uint32 internal_regtaskmem_maxlenght( struct SRecTaskInstance task )
{
    // maximum memory length is limmited to 2^16 elements
    switch ( task.task_elems )
    {
        case rtt_t:
        case rtt_h:
        case rtt_p:
            return 96;          // 1.5 byte  - 1x 12bit -> 98304 bytes -> 96 pages
            break;
        case rtt_th:
        case rtt_hp:
        case rtt_tp:
            return 192;       // 3 bytes   - 2x 12bit -> 196608 bytes -> 192 pages
            break;
        case rtt_thp:
            return 288;       // 4.5 bytes - 3x 12bit -> 294912 bytes -> 288 pages
            break;
    }
    return 0;
}


void ui_call_setwindow_regtaskmem_chstart( int context, void *pval )
{
    struct SRecTaskInstance task;
    int val = *((int*)pval);
    bool iter = false;
    bool up = false;
    int i;
    int idx;

    idx = ui.p.swRegTaskMem.task_index;
    task = ui.p.swRegTaskMem.task[idx];

    if ( core.nvrec.running & (1<<idx) )        // editing a running task is not allowed
        goto _set_original;

    if ( val > task.mempage )
        up = true;

    do
    {
        iter = false;
        for (i=0;i<4;i++)
        {
            if ( (i != idx) && internal_regtaskmem_inside(val, task.size, ui.p.swRegTaskMem.task[i]) )      // if current task with the new position is inside of one of tasks
            {
                if ( up )
                {
                    val = ui.p.swRegTaskMem.task[i].mempage + ui.p.swRegTaskMem.task[i].size;           // move the start address to the conflicting task's end
                    if ( (val+task.size) > CORE_RECMEM_MAXPAGE )                                        // if task doesn't fit in memory - revert to original value
                        goto _set_original;
                }
                else
                {
                    val = (int)ui.p.swRegTaskMem.task[i].mempage - (int)task.size;                      // move the value before the confligting taks
                    if ( val < 0 )                                                                      // if task doesn't fit in memory - revert to original value
                        goto _set_original;
                }
                iter = true;
            }
        }

        if ( up )
        {
            if ( (val+task.size) > CORE_RECMEM_MAXPAGE )                                        // if task doesn't fit in memory - revert to original value
                        goto _set_original;
        }
        else if ( val < 0 )                                                                     // if task doesn't fit in memory - revert to original value
                        goto _set_original;

    } while( iter );

    // moving succeeded - save the new mempage value and update ui
    ui.p.swRegTaskMem.task[idx].mempage = val;
    uiel_control_numeric_set( &ui.p.swRegTaskMem.start, val );
    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
    return;
_set_original:
    // moving failed - revert to original mempage value
    uiel_control_numeric_set( &ui.p.swRegTaskMem.start, ui.p.swRegTaskMem.task[idx].mempage );
    ui.upd_ui_disp |= RDRW_UI_CONTENT;
}


void ui_call_setwindow_regtaskmem_chlenght( int context, void *pval )
{
    struct SRecTaskInstance task;
    int val = *((int*)pval);

    int i;
    int idx;

    idx = ui.p.swRegTaskMem.task_index;
    task = ui.p.swRegTaskMem.task[idx];

    if ( core.nvrec.running & (1<<idx) )        // editing a running task is not allowed
        goto _set_original;
        
    // no need to check for smaller value, numeric control takes care of the 1 minimum
    // check only for increasing values
    if ( val >= task.size )
    {   
        if ( (val+task.mempage) > CORE_RECMEM_MAXPAGE )                                     // if task doesn't fit in memory - revert to original value
            goto _set_original;
        if ( val > internal_regtaskmem_maxlenght(task) )
            goto _set_original;

        for (i=0;i<4;i++)
        {
            if ( (i != idx) && internal_regtaskmem_inside(task.mempage, val, ui.p.swRegTaskMem.task[i]) )
                goto _set_original;
        }
    }
    ui.p.swRegTaskMem.task[idx].size = val;
    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
    return;
_set_original:
    uiel_control_numeric_set( &ui.p.swRegTaskMem.lenght, ui.p.swRegTaskMem.task[idx].size );
    ui.upd_ui_disp |= RDRW_UI_CONTENT;
}


// --- Recorded graph displaying

// --- graph select window

void ui_call_graphselect_action( int context, void *pval )
{
    // context = 0 if esc. pressed, 1..4 for the 4 tasks
    if ( context == 0 )
    { 
        uist_change_state( UI_STATE_MODE_SELECT, UI_SET_NONE, true );
        return;
    }
    // graph display selected
    context--;

    if ( core.nvrec.func[context].c == 0 )      // do not enter in empty recordings
        return;

    ui.m_return = context;
    uist_change_state( UI_STATE_MAIN_GRAPH, UI_SET_NONE, true );
}

void ui_call_graphdisp_unit_select( int context, void *pval )
{


}

void ui_call_graphdisp_menu_action( int context, void *pval )
{
    // context 0 -> OK,  1 -> Esc,  2 -> Vchange
    int val = *((int*)pval);
    bool zoomed;

    zoomed = internal_graph_is_zoomed();

    switch ( context )
    {
        case 0:
            if ( val == 0 )                             // cursor on "details"
                goto _enter_details;
            else if ( val == 1 )                        // "zoom select"
            {
                if ( core.nvrec.func[ui.m_return].c < WB_DISPPOINT )
                    return;

                ui.p.grDisp.d_state &= ~GRSTATE_MASK;
                ui.p.grDisp.d_state |= GRSTATE_SELECT_ZOOM;
                ui.p.grDisp.view_cursor2 = ui.p.grDisp.view_cursor1;
                ui.focus = 1;
                internal_graphdisp_adjust_cursor( true );
                goto _reset_display;
            }
            else if ( zoomed && (val == 2)  )           // pan
            {
            }   
            else if ( zoomed && (val == 3) )            // zoom out
            {
                ui.p.grDisp.view_elemstart = core.nvrec.func[ui.m_return].c;
                ui.p.grDisp.view_elemend = 0;
                core_op_recording_read_cancel();   
                core_op_recording_read_request( ui.m_return, ui.p.grDisp.view_elemstart, ui.p.grDisp.view_elemstart );    // get the entire recording
                ui.p.grDisp.d_state = GRSTATE_DISP | GRSTATE_FLAG_FILL;
                ui.p.grDisp.graph_dirty = 1;
                goto _reset_display;
            }
            else if ( (zoomed && (val == 4)) ||         // zoom to menu
                      (!zoomed && (val == 2) ) )
            {
            }
            else if ( (zoomed && (val == 5)) ||         // change min/max global/local
                      (!zoomed && (val == 3) ) )
            {
            }
            break;
        case 1:
            ui.p.grDisp.d_state = GRSTATE_DISP;
            ui.p.grDisp.view_cursor2 = ui.p.grDisp.view_cursor1;
            ui.p.grDisp.d_upd_ctr = 0;
            goto _reset_display;
        case 2:                 // Vchange
            if ( (val == 0) && (ui.p.grDisp.d_upd_ctr == 0) )
            {
                // show cursor info
        _enter_details:
                ui.p.grDisp.d_state &= ~GRSTATE_MASK;
                ui.p.grDisp.d_state |= GRSTATE_DETAIL;
                goto _reset_display;
            }
            else
                ui.p.grDisp.d_upd_ctr = 1;
            break;
    }
    return;
_reset_display:
    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
    uist_setupview_mainwindow(false);
    return;
}


// ---- Setup menu

// ---- menu itself

void ui_call_setmenu_action( int context, void *pval )
{
    // context 0 -> OK,  1 -> Esc
    int val = *((int*)pval);

    if ( context == 1 )
    {
        uist_change_state( UI_STATE_MODE_SELECT, UI_SET_NONE, true );
        return;
    }

    ui.m_return = val;
    switch ( val )
    {
        case 0:         // display setup
            uist_change_state( UI_STATE_NONE, UI_SET_SetupDisplay, true );
            return;
        case 3:         // time setup
            uist_change_state( UI_STATE_NONE, UI_SET_SetupTime, true );
            return;
    }
}


// ---- display menu
static int internal_setdisplay_esc(int context)
{
    if (context & 0x30)     // not an ESC
        return 0;

    core.nv.setup.disp_brt_on = uiel_control_numeric_get( &ui.p.setDisplay.bright[0] );
    core.nv.setup.disp_brt_dim = uiel_control_numeric_get( &ui.p.setDisplay.bright[1] );

    core.nv.setup.grey_disprate = uiel_control_numeric_get( &ui.p.setDisplay.grey[0] );
    core.nv.setup.grey_frame_all = uiel_control_numeric_get( &ui.p.setDisplay.grey[1] );
    core.nv.setup.grey_frame_grey = uiel_control_numeric_get( &ui.p.setDisplay.grey[2] );

    DispHAL_SetContrast( core.nv.setup.disp_brt_on );
    DispHal_GreySetup( core.nv.setup.grey_disprate, core.nv.setup.grey_frame_all, core.nv.setup.grey_frame_grey );
    DispHal_ClearFlipBuffer();
    uist_change_state( UI_STATE_NONE, UI_SET_SetupMenu, true );
    return 1;
}


void ui_call_setdisplay_brightness( int context, void *pval )
{
    if ( internal_setdisplay_esc(context) )
        return;
    
    if ( context & 0x20 )       // long esc - defaults
    {
        context &= ~0x20;
        if ( context == 0 )
            uiel_control_numeric_set( &ui.p.setDisplay.bright[0], core.nv.setup.disp_brt_on );
        else
            uiel_control_numeric_set( &ui.p.setDisplay.bright[1], core.nv.setup.disp_brt_dim );
        return;
    }
    else
    {
        int brt_max;
        int brt_dim;
        context &= 0x01;

        brt_max = uiel_control_numeric_get( &ui.p.setDisplay.bright[0] );
        brt_dim = uiel_control_numeric_get( &ui.p.setDisplay.bright[1] );

        if ( context )          // dimmed
        {   
            DispHAL_SetContrast( brt_dim );
            if ( brt_dim > brt_max )
            {
                uiel_control_numeric_set( &ui.p.setDisplay.bright[0], brt_dim );
                ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
            }
        }
        else
        {
            DispHAL_SetContrast( brt_max );
            if ( brt_max < brt_dim )
            {
                uiel_control_numeric_set( &ui.p.setDisplay.bright[1], brt_max );
                ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
            }
        }
    }

}

void ui_call_setdisplay_greysetup( int context, void *pval )
{
    if ( internal_setdisplay_esc(context) )
        return;

    if ( context & 0x20 )       // long esc - defaults
    {
        context &= ~0x20;
        if ( context == 0 )
            uiel_control_numeric_set( &ui.p.setDisplay.grey[0], core.nv.setup.grey_disprate );
        else if ( context == 1 )
            uiel_control_numeric_set( &ui.p.setDisplay.grey[1], core.nv.setup.grey_frame_all );
        else
            uiel_control_numeric_set( &ui.p.setDisplay.grey[2], core.nv.setup.grey_frame_grey );
        return;
    }
    else
    {
        uint32 g_rate;
        uint32 g_all;
        uint32 g_grey;
        context &= 0x03;

        g_rate = uiel_control_numeric_get( &ui.p.setDisplay.grey[0] );
        g_all = uiel_control_numeric_get( &ui.p.setDisplay.grey[1] );
        g_grey = uiel_control_numeric_get( &ui.p.setDisplay.grey[2] );

        if ( context != 0 )                 // control the timing
        {           
            if ( context == 1 )             // all frame length setup
            {
                if ( g_grey > g_all )
                {
                    uiel_control_numeric_set( &ui.p.setDisplay.grey[2], g_all );
                    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
                }
            }
            else if ( g_all < g_grey )      // grey frame lenght setup
            {
                uiel_control_numeric_set( &ui.p.setDisplay.grey[1], g_grey );
                ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
            }
        }

        DispHal_GreySetup( g_rate, g_all, g_grey );
    }
}

// ---- time setup menu

void ui_call_settime_action( int context, void *pval )
{
    datestruct date;
    timestruct time;
    uint32 clock;

    if ( context == 0xff )      // ESC
    {   
        uist_change_state( UI_STATE_NONE, UI_SET_SetupMenu, true );
        return;
    }

    if ( context == 0xee )      // OK - edit finished
    {
        // edit finished
        uiel_control_time_get_time( &ui.p.setTime.time, (void*)&time );
        uiel_control_time_get_time( &ui.p.setTime.date, (void*)&date );

        // set clock
        clock = utils_convert_date_2_counter( &date, &time );
        core_set_clock_counter( clock );

        // reschedule UI power management
        internal_pwrmngm_reset_inactivities( clock );
    }
    else
    {
        clock = core_get_clock_counter();
        utils_convert_counter_2_hms(clock, &time.hour, &time.minute, &time.second );
        utils_convert_counter_2_ymd(clock, &date.year, &date.mounth, &date.day );
        if ( context )
            uiel_control_time_set_time( &ui.p.setTime.date, (void*)&date );
        else
            uiel_control_time_set_time( &ui.p.setTime.time, (void*)&time );
    }

    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL | RDRW_STATUSBAR;
}


// Popup window default callback

void ui_call_popup_default( int context, void *pval )
{
    uist_close_popup();
}



////////////////////////////////////////////////////
//
//   Generic UI functional elements
//
////////////////////////////////////////////////////

void uist_change_state( enum EUIStates m_state, enum EUISetupSelect set_state, bool init )
{
    // call this to change UI states from callback 
    // - this will postpone state update, and prevent display setup with non-existing controls from the new states
    ui.newst.dirty = 1;
    ui.newst.m_setstate = (uint8)set_state;
    ui.newst.m_state    = (uint8)m_state;
    ui.newst.init       = init ? 1 : 0;
}

bool uist_apply_newstate(void)
{
    // called in the state loops
    if ( ui.newst.dirty == 0 )
        return false;

    ui.newst.dirty = 0;

    if ( ui.newst.m_state != UI_STATE_NONE )
        ui.m_state = (enum EUIStates)ui.newst.m_state;

    if ( ui.newst.m_setstate != UI_SET_NONE )
        ui.m_setstate = (enum EUISetupSelect)ui.newst.m_setstate;

    if ( ui.newst.init )
        ui.m_substate = UI_SUBST_ENTRY;

    return true;
}

static void uist_update_display( int disp_update )
{
    if ( disp_update )
    {
        if ( disp_update & RDRW_ALL )   // if anything to be redrawn
        {
            switch ( ui.m_state )
            {
                case UI_STATE_MAIN_GRAPH:
                case UI_STATE_MAIN_GAUGE: uist_drawview_mainwindow( disp_update & RDRW_ALL ); break;
                case UI_STATE_SETWINDOW:  uist_drawview_setwindow( disp_update & RDRW_ALL ); break;
                case UI_STATE_POPUP:      uist_drawview_popup( disp_update & RDRW_ALL ); break;
                case UI_STATE_MODE_SELECT:uist_drawview_modeselect( disp_update & RDRW_ALL ); break;

            }
        }
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
                if ( evmask->timer_tick_05sec )
                {
                    update |= RDRW_UI_DYNAMIC;
                }

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

static void uist_infocus_generic_key_processing( struct SEventStruct *evmask )
{
    bool focus_moved = false;

    // move focus to the next element
    if ( evmask->key_pressed & KEY_DOWN )
    {
        if ( ui.focus < ui.ui_elem_nr )
            ui.focus++;
        else
            ui.focus = 1;
        ui.upd_ui_disp |= RDRW_UI_CONTENT;
        focus_moved = true;
    }
    // move focus to the previous element
    if ( evmask->key_pressed & KEY_UP )
    {
        if ( ui.focus > 1 )
            ui.focus--;
        else
            ui.focus = ui.ui_elem_nr;
        ui.upd_ui_disp |= RDRW_UI_CONTENT;
        focus_moved = true;
    }

    // treate couple of special cases
    if ( focus_moved )
    {
        if ( ui.m_state == UI_STATE_SETWINDOW )
        {
            switch (ui.m_setstate)
            {
                case UI_SET_QuickSwitch:
                    // revert all monitoring rates to their setup values
                    uiel_control_list_set_index( &ui.p.swQuickSw.m_rates[0], core.nv.setup.tim_tend_temp );
                    uiel_control_list_set_index( &ui.p.swQuickSw.m_rates[1], core.nv.setup.tim_tend_hygro );
                    uiel_control_list_set_index( &ui.p.swQuickSw.m_rates[2], core.nv.setup.tim_tend_press );
                    break;
                case UI_SET_SetupDisplay:
                    if ( ui.focus == 2 )    // for dimmed - setup dimmed value
                        DispHAL_SetContrast( uiel_control_numeric_get( &ui.p.setDisplay.bright[1] ));
                    else
                        DispHAL_SetContrast( core.nv.setup.disp_brt_on );
                    break;
            }
        }
    }
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
        internal_pwrmngm_reset_inactivities( crt_rtc );

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

    if ( uist_apply_newstate() )
        return;

    if ( (ui.m_substate == 0) &&                                    // timeout reached
         ((core.vstatus.ui_cmd & CORE_UISTATE_CHARGING) == 0 ) )    // no charging
    {
        // no pwr.on long press, timed out - power down
        ui.pwr_state = SYSSTAT_UI_PWROFF;
    }
}


/// POPUP WINDOW

// no input parameters for this routine (no reason to ocupy memory in plus)
// --> when calling this entering routine - first fill the ui.popup.params structure
void uist_enter_popup( uint8 cont_ok, uiel_callback call_ok, uint8 cont_cancel, uiel_callback call_cancel )
{
    ui.popup.ret_state = ui.m_state;
    uist_change_state( UI_STATE_POPUP, UI_SET_NONE, false );

    ui.popup.focus = 0;
    ui.popup.elems = 1;

    switch ( ui.popup.params.popup_action )
    {
        case uipa_close:
            uiel_control_pushbutton_init( &ui.popup.pb1, 40, 45, 48, 12 );
            uiel_control_pushbutton_set_content( &ui.popup.pb1, uicnt_text, 0, "Close", uitxt_smallbold ); 
            break;
        case uipa_ok:
            uiel_control_pushbutton_init( &ui.popup.pb1, 40, 45, 48, 12 );
            uiel_control_pushbutton_set_content( &ui.popup.pb1, uicnt_text, 0, "OK", uitxt_smallbold ); 
            break;
        case uipa_ok_cancel:
            ui.popup.focus = 1;
            ui.popup.elems = 2;
            uiel_control_pushbutton_init( &ui.popup.pb1, 12, 45, 48, 12 );
            uiel_control_pushbutton_set_content( &ui.popup.pb1, uicnt_text, 0, "OK", uitxt_smallbold ); 
            uiel_control_pushbutton_init( &ui.popup.pb2, 67, 45, 48, 12 );
            uiel_control_pushbutton_set_content( &ui.popup.pb2, uicnt_text, 0, "Cancel", uitxt_smallbold ); 
            break;
    }

    uiel_control_pushbutton_set_callback( &ui.popup.pb1, UICpb_OK, cont_ok, call_ok ? call_ok : ui_call_popup_default );
    uiel_control_pushbutton_set_callback( &ui.popup.pb2, UICpb_OK, cont_cancel, call_cancel ? call_cancel : ui_call_popup_default );
    uiel_control_pushbutton_set_callback( &ui.popup.pb1, UICpb_Esc, cont_cancel, call_cancel ? call_cancel : ui_call_popup_default );
    uiel_control_pushbutton_set_callback( &ui.popup.pb2, UICpb_Esc, cont_cancel, call_cancel ? call_cancel : ui_call_popup_default );

    uist_drawview_popup( RDRW_ALL );
    DispHAL_UpdateScreen();
    ui.upd_ui_disp = RDRW_ALL;
}


// to be called at OK/Cancel callbacks
void uist_close_popup(void)
{
    // revert original state
    ui.m_state = ui.popup.ret_state;    // set directly the main state - if a callback sets new state through uist_change_state - we will
                                        // lose the org. main state otherwise
                                        // The controls from the org. state are still valid, so it is safe to use
    // redraw the original display
    uist_update_display( RDRW_ALL );
    ui.upd_ui_disp = RDRW_ALL;
}


void uist_popupwindow( struct SEventStruct *evmask )
{
    if ( evmask->key_event )
    {
        if ( evmask->key_pressed & KEY_RIGHT )
        {
            if ( ui.popup.elems == 2 )
                ui.popup.focus = 1;
            ui.upd_ui_disp |= RDRW_UI_CONTENT;
        }
        // move focus to the previous element
        if ( evmask->key_pressed & KEY_LEFT )
        {
            if ( ui.popup.elems == 2 )
                ui.popup.focus = 0;
            ui.upd_ui_disp |= RDRW_UI_CONTENT;
        }

        // operations if focus on ui elements
        if ( ui_element_poll( ui.popup.focus ? &ui.popup.pb2 : &ui.popup.pb1, evmask ) )
        {
            ui.upd_ui_disp  |= RDRW_DISP_UPDATE;          // mark only for dispHAL update
        }

        if ( uist_apply_newstate() )
            return;

        // power button activated
        if ( evmask->key_longpressed & KEY_MODE )
        {
            uist_goto_shutdown();
            return;
        }
    }

    uist_update_display( ui.upd_ui_disp );
    ui.upd_ui_disp = 0;                     // do this after refresh to force a display update at first entry
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
    //core_beep( beep_pwroff );
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
    if ( evmask->key_event )
    {
        if ( evmask->key_pressed & KEY_UP )
        {
            ui.m_state = UI_STATE_MAIN_GAUGE;
            ui.m_substate = UI_SUBST_ENTRY;
            return;
        }
        if ( evmask->key_pressed & KEY_LEFT )
        {
            ui.m_state = UI_STATE_SETWINDOW;
            ui.m_setstate = UI_SET_QuickSwitch;
            ui.m_substate = UI_SUBST_ENTRY;
            ui.m_return = 0;
            return;
        }
        if ( evmask->key_pressed & KEY_RIGHT )
        {
            ui.m_state = UI_STATE_SETWINDOW;
            ui.m_setstate = UI_SET_GraphSelect;
            ui.m_substate = UI_SUBST_ENTRY;
            ui.m_return = 1;
            return;
        }
        if ( evmask->key_pressed & KEY_OK )
        {
            ui.m_state = UI_STATE_SETWINDOW;
            ui.m_setstate = UI_SET_SetupMenu;
            ui.m_substate = UI_SUBST_ENTRY;
            ui.m_return = 0;
            return;
        }
        if ( evmask->key_longpressed & KEY_MODE )
        {
            uist_goto_shutdown();
            return;
        }
        if ( uist_apply_newstate() )
            return;
    }

    // update screen on timebase
    ui.upd_ui_disp |= uist_timebased_updates( evmask );
    uist_update_display( ui.upd_ui_disp );
    ui.upd_ui_disp = 0;
}


/// UI MAIN GAUGE WINDOW
void uist_setwindow_entry( void )
{
    uist_setupview_setwindow( true );
    uist_drawview_setwindow( RDRW_ALL );
    DispHAL_UpdateScreen();
    ui.m_substate ++;
    ui.upd_ui_disp = 0;
}

void uist_setwindow( struct SEventStruct *evmask )
{
    if ( evmask->key_event )
    {
        // generic UI buttons and events
        if ( ui.focus )
        {
            // operations if focus on ui elements
            if ( ui_element_poll( ui.ui_elems[ ui.focus - 1], evmask ) )
            {
                ui.upd_ui_disp  |= RDRW_DISP_UPDATE;          // mark only for dispHAL update
            }

            if ( uist_apply_newstate() )
                return;

            uist_infocus_generic_key_processing( evmask );
        }

        // power button activated
        if ( evmask->key_longpressed & KEY_MODE )
        {
            DispHal_ClearFlipBuffer();
            uist_goto_shutdown();
            return;
        }
        if ( evmask->key_released & KEY_MODE )
        {
            DispHal_ClearFlipBuffer();
            ui.m_state = UI_STATE_MODE_SELECT;
            ui.m_substate = UI_SUBST_ENTRY;
            return;
        }
    }

    // update screen on timebase
    ui.upd_ui_disp |= uist_timebased_updates( evmask );
    uist_update_display( ui.upd_ui_disp );
    ui.upd_ui_disp = 0;
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
    ui.upd_ui_disp = 0;
}


void uist_mainwindowgauge( struct SEventStruct *evmask )
{
    if ( evmask->key_event )
    {
        // generic UI buttons and events
        if ( ui.focus )
        {
            // operations if focus on ui elements
            if ( ui_element_poll( ui.ui_elems[ ui.focus - 1], evmask ) )
            {
                ui.upd_ui_disp |= RDRW_DISP_UPDATE;
            }

            if ( uist_apply_newstate() )
                return;

            uist_infocus_generic_key_processing( evmask );
        }
        else
        {
            // operations if no focus yet - OK presssed
            if ( (evmask->key_pressed & KEY_LEFT) && (ui.main_mode > 0) )
            {
                ui.main_mode--;
                uist_setupview_mainwindow( true );
                ui.upd_ui_disp = RDRW_ALL;
                core.measure.dirty.b.upd_th_tendency = 1;       // force an update on the tendency graph values
                core_op_realtime_sensor_select( (enum ESensorSelect)(ui.main_mode + 1) );
            }
            if ( (evmask->key_pressed & KEY_RIGHT) && (ui.main_mode < UImm_gauge_pressure) )
            {
                ui.main_mode++;
                uist_setupview_mainwindow( true );
                ui.upd_ui_disp = RDRW_ALL;
                core.measure.dirty.b.upd_th_tendency = 1;       // force an update on the tendency graph values
                core_op_realtime_sensor_select( (enum ESensorSelect)(ui.main_mode + 1) );
            }
            if ( evmask->key_pressed & KEY_OK )    // enter in edit mode
            {
                ui.focus = 1;
                ui.upd_ui_disp |= RDRW_UI_CONTENT;

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
            return;
        }
        if ( evmask->key_released & KEY_MODE )
        {
            core_op_realtime_sensor_select( ss_none );
            ui.m_state = UI_STATE_MODE_SELECT;
            ui.m_substate = UI_SUBST_ENTRY;
            return;
        }
    }

    // update screen on timebase
    ui.upd_ui_disp |= uist_timebased_updates( evmask );
    uist_update_display( ui.upd_ui_disp );
    ui.upd_ui_disp = 0;
}


/// UI MAIN GRAPHIC WINDOW

/***************************************
    GRSTATE_FLAG_FILL: - filling display buffer from NVRAM (valid in any state)
        operations: - ESC / PWR - exit to prew. / opmode select screen  - these are valid for all the other states
    GRSTATE_DISP:
        operations: UP/DN: select focus bw. [element selector][graph cursor][unit selector]
                    L/R:   change value of unit selector, element selector, or move the cursor
                                - cursor move will display current AVG value under the cursor
                    OK:    Quick menu with other graph operations -> GRSTATE_MENU
    GRSTATE_MENU:
        operations: UP - right after entry: - show detail for the point under the cursor:   - date/time, min/max, avg value - 
                                            - any button press - exit from detail window -> GRSTATE_DISP
                    UP/DN - if dn pressed first: - select menu point
                    OK:    Enter menu point:
                                - "details"     GRSTATE_DETAIL      - show detail for cursor
                                - <"select">    GRSTATE_SELECT_ZOOM - (default) back to main window but with dual cursor
                                - ("pan")       GRSTATE_PAN         - (if zoomed) pan the zoomed section
                                - ("zoom out")  GRSTATE_DISP
                                - "zoom to"     GRSTATE_ZOOM_MENU
                                - "local mm"/"global mm"   - select if local min/max scale is used (per zoom) or global min/max (per all)  
    GRSTATE_DETAIL: see abowe,
        operations: UP/DN/L/R -> GRSTATE_DISP

    GRSTATE_SELECT_ZOOM: operates dual cursor. X endpoints showing the start/end date of the two cursors
        operations: L/R:    start cursor value
                    UP/DN:  end cursor value
                    ESC:    -> cancel zoom -> GRSTATE_DISP
                    OK:     -> apply zoom (with constraints) -> GRSTATE_DISP

    GRSTATE_PAN:    change the displaying location:
        operations: L/R:    pan zoom window (constraints) 
                    OK/timeout: apply pan -> GRSTATE_DISP

    GRSTATE_ZOOM_MENU:   menu with special zoom options
        operations: UP/DN: - select menu item:
                    OK:    Enter menu item:
                                - <"day">   - zoom the day under the cursor -> GRSTATE_DISP
                                - "week"    - zoom the week under the cursor -> GRSTATE_DISP
                                - "mounth"  - zoom the mounth under the cursor -> GRSTATE_DISP
                                - "date"    -> goes to a setwindow with start and end date


***************************************/

void uist_mainwindowgraph_entry( void )
{
    memset( &ui.p.grDisp, 0, sizeof(ui.p.grDisp) );
    // m_return holds the task index
    ui.p.grDisp.d_state = GRSTATE_DISP | GRSTATE_FLAG_FILL;
    ui.p.grDisp.d_progr = 16;     // means maximum wait state
    ui.p.grDisp.view_cursor1 = 54;    // cursor to middle
    ui.p.grDisp.view_cursor2 = 54;    // cursor to middle
    ui.p.grDisp.graph_dirty = 1;      // update display points from grahp data
    ui.p.grDisp.view_elemstart = core.nvrec.func[ui.m_return].c;
    ui.p.grDisp.view_elemend = 0;

    ui.p.grDisp.units[0] = core.nv.setup.show_unit_temp;
    ui.p.grDisp.units[1] = core.nv.setup.show_unit_hygro;
    ui.p.grDisp.units[2] = core.nv.setup.show_unit_press;

    while ( internal_graphdisp_get_element(ui.p.grDisp.view_elem) == ss_none )
        ui.p.grDisp.view_elem++;

    core_op_recording_read_request( ui.m_return, ui.p.grDisp.view_elemstart, ui.p.grDisp.view_elemstart );    // get the entire recording

    uist_setupview_mainwindow( true );
    uist_drawview_mainwindow( RDRW_ALL );
    DispHAL_UpdateScreen();

    ui.focus = 1;
    ui.m_substate ++;
    ui.upd_ui_disp = 0;
}

void uist_mainwindowgraph( struct SEventStruct *evmask )
{
    if ( evmask->key_event )
    {
        // operations allowed only in non_filling state
        if ( (ui.p.grDisp.d_state & GRSTATE_FLAG_FILL) == 0 )
        {
            if ( (ui.p.grDisp.d_state & GRSTATE_DETAIL) == 0 )
            {
                if ( ui.p.grDisp.d_state & (GRSTATE_MENU | GRSTATE_ZOOM_MENU) )
                {
                    if ( ui_element_poll( &ui.p.grDisp.ctrl.menu, evmask ))
                       ui.upd_ui_disp |= RDRW_DISP_UPDATE;
                }
                else if (ui.focus == 2 )
                {
                    if ( ui_element_poll( &ui.p.grDisp.ctrl.unit, evmask ))
                       ui.upd_ui_disp |= RDRW_DISP_UPDATE;
                }
                else
                {
                    bool update = false;
                    if ( evmask->key_pressed & KEY_LEFT )
                    {
                        if ( ui.focus == 0 )
                            update = internal_graphdisp_operate_elemselect( false );
                        else
                            update = internal_graphdisp_operate_cursor( false, false );
                    }
                    if ( evmask->key_pressed & KEY_RIGHT )
                    {
                        if ( ui.focus == 0 )
                            update = internal_graphdisp_operate_elemselect( true );
                        else
                            update = internal_graphdisp_operate_cursor( true, false );
                    }
                    if ( ui.p.grDisp.d_state & GRSTATE_SELECT_ZOOM )             // in select zoom - the up/down operates the C2
                    {
                        if ( evmask->key_pressed & KEY_UP )
                            update = internal_graphdisp_operate_cursor( true, true );
                        if ( evmask->key_pressed & KEY_DOWN )
                            update = internal_graphdisp_operate_cursor( false, true );
                        evmask->key_pressed &= ~(KEY_UP | KEY_DOWN);            // clear the Up/Down action because we processed here
                    }
                    if ( update )
                    {
                        if ( ui.focus == 0 )
                            ui.p.grDisp.graph_dirty = 1;
                        ui.upd_ui_disp |= RDRW_UI_DYNAMIC;
                    }
                }
    
                // up/down button
                if ( (evmask->key_pressed & KEY_UP) && (ui.focus > 0) )
                {
                    ui.focus--;
                    uist_setupview_mainwindow( false );
                    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
                }
                if ( (evmask->key_pressed & KEY_DOWN) && (ui.focus < 2) )
                {
                    ui.focus++;
                    uist_setupview_mainwindow( false );
                    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
                }
            }

            if ( (evmask->key_pressed & KEY_OK) &&                                                              // OK button processed only for
                 ( ((ui.p.grDisp.d_state & (GRSTATE_DISP | GRSTATE_SELECT_ZOOM)) && (ui.focus == 1)) ||         // - cursor in focus in main display screen
                   ((ui.p.grDisp.d_state & (GRSTATE_DISP | GRSTATE_SELECT_ZOOM)) == 0) )  )                     //   or any other screen
            {
                internal_graphdisp_btnpress_OK();
            }
        }

        // power button activated
        if ( evmask->key_longpressed & KEY_MODE )
        {
            if (core_op_recording_read_busy())
                core_op_recording_read_cancel();        

            DispHal_ClearFlipBuffer();
            uist_goto_shutdown();
            return;
        }
        if ( evmask->key_released & KEY_MODE )
        {
            if (core_op_recording_read_busy())
                core_op_recording_read_cancel();        

            core_op_realtime_sensor_select( ss_none );
            ui.m_state = UI_STATE_MODE_SELECT;
            ui.m_substate = UI_SUBST_ENTRY;
            DispHal_ClearFlipBuffer();
            return;
        }
        if ( evmask->key_released & KEY_ESC )
        {
            if ( ui.p.grDisp.d_state & GRSTATE_DISP )
            {
                if (core_op_recording_read_busy())
                    core_op_recording_read_cancel();        
                DispHal_ClearFlipBuffer();
                ui.m_state = UI_STATE_SETWINDOW;
                ui.m_setstate = UI_SET_GraphSelect;
                ui.m_substate = UI_SUBST_ENTRY;
                ui.m_return = ui.m_return+1;
                return;
            }
            else
            {
                internal_graphdisp_btnpress_ESC();
            }
        }
    }

    // timed stuff
    if ( evmask->timer_tick_10ms )
    {
        if ( ui.p.grDisp.d_state & GRSTATE_FLAG_FILL )
        {
            ui.p.grDisp.d_upd_ctr ++;                         // Update should be done at 50Hz -> 20ms
            if ( ui.p.grDisp.d_upd_ctr & 0x02 )
            {
                ui.p.grDisp.d_upd_ctr = 0;

                ui.p.grDisp.d_progr = core_op_recording_read_busy();
                if ( ui.p.grDisp.d_progr == 0 )
                {
                    ui.p.grDisp.d_state &= ~GRSTATE_FLAG_FILL;
                    ui.upd_ui_disp |= RDRW_UI_CONTENT_ALL;
                }
                else
                    ui.upd_ui_disp |= RDRW_UI_DYNAMIC;
            }
        }
    }

    // update screen on timebase
    ui.upd_ui_disp |= uist_timebased_updates( evmask );
    uist_update_display( ui.upd_ui_disp );
    ui.upd_ui_disp = 0;
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

    if ( (core_cmd & CORE_UISTATE_START_LONGPRESS) ||
         (core_cmd & CORE_UISTATE_CHARGING ) )
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
                case UI_STATE_MAIN_GRAPH:
                    uist_mainwindowgraph_entry();
                    break;
                case UI_STATE_SETWINDOW:
                    uist_setwindow_entry();
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
                case UI_STATE_MAIN_GRAPH:
                    uist_mainwindowgraph( evmask );
                    break;
                case UI_STATE_SETWINDOW:
                    uist_setwindow( evmask );
                    break;
                case UI_STATE_MODE_SELECT:
                    uist_opmodeselect( evmask );
                    break;
                case UI_STATE_STARTUP:
                    uist_startup( evmask );
                    break;
                case UI_STATE_POPUP:
                    uist_popupwindow( evmask );
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
