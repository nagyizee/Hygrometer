#ifndef UI_INTERNALS_H
#define UI_INTERNALS_H

#ifdef __cplusplus
    extern "C" {
#endif

    #include "stm32f10x.h"
    #include "typedefs.h"
    #include "ui_elements.h"
    #include "core.h"


    // user interface main states
    enum EUIStates
    {
        UI_STATE_NONE = 0,
        UI_STATE_STARTUP,                       // state entered after power on
        UI_STATE_MAIN_GAUGE,                    // main window with value gauges (temperature, humidity, pressure)
        UI_STATE_MAIN_GRAPH,                    // graph display for the abowe stuff
        UI_STATE_MAIN_ALTIMETER,                // altimeter mode - this include variometer function
        UI_STATE_SETWINDOW,                     // setup window
        UI_STATE_MODE_SELECT,                   // mode selector window - navigate bw. gauge - graph - altimeter - recording - setup
        UI_STATE_SHUTDOWN,                      // shut-down window
        UI_STATE_POPUP,                         // message pop-up window
        UI_STATE_DBG_INPUTS                     // debug button press, sensor readings, etc
    };

    enum EUISetupSelect
    {
        UI_SET_QuickSwitch = 0,                 // set up quick switches
        UI_SET_RegTaskSet,                      // set up registering task
        UI_SET_RegTaskMem,                      // set up registering task recording lenght and memory repartization

    };


    enum UIMainMode
    {
        UImm_gauge_thermo = 0,
        UImm_gauge_hygro,
        UImm_gauge_pressure
    };


    // user interface substate
    #define UI_SUBST_ENTRY          0

    // maximum user interface elements on screen
    #define UI_MAX_ELEMS            10

    // display redraw options
    #define RDRW_BATTERY            0x01        // 0000 0001b - redraw battery
    #define RDRW_CLOCKTICK          0x02        // 0000 0010b - redraw clock tick
    #define RDRW_STATUSBAR          0x07        // 0000 0111b - redraw all the status bar
    #define RDRW_UI_CONTENT         0x08        // 0000 1000b - redraw ui content
    #define RDRW_UI_DYNAMIC         0x10        // 0001 0000b - redraw ui dynamic elements only (high update rate stuff like indicators)
    #define RDRW_UI_CONTENT_ALL     0x18        // 0001 1000b - redraw ui content including dynamic elements
    #define RDRW_ALL                0x1F        // redraw everything

    #define RDRW_DISP_UPDATE        0x80        // just a dummy value to enter to the display update routine


    struct SUIMainGaugeThermo
    {
        struct Suiel_control_list    units;
        struct Suiel_control_list    minmaxset[3];
        enum ETemperatureUnits       unitT;
    };

    struct SUIMainGaugeHygro
    {
        struct Suiel_control_list    units;
        struct Suiel_control_list    minmaxset[3];
        enum EHumidityUnits          unitH;
    };

    struct SUIMainGaugePress
    {
        struct Suiel_control_list    units;
        struct Suiel_control_list    minmaxset;
        struct Suiel_control_edit    msl_press;
        struct Suiel_control_edit    crt_altitude;
        bool                         msl_ref;       // if true - mean see level pressure is the reference -> calculate altitude
    };                                              // if false - altitude is the reference -> calculate the msl pressure

    struct SUISetQuickSwitch
    {
        struct Suiel_control_checkbox   monitor;
        struct Suiel_control_pushbutton resetMM;
        struct Suiel_control_checkbox   reg;
        struct Suiel_control_list       m_rates[3];     // set up monitoring rates for Temp / Hygro / Pressure ( indexes 0, 1, 2 )
        struct Suiel_control_pushbutton taskbutton[4];
    };

    struct SUISetRegTaskSet
    {
        uint32                          task_index;
        struct SRegTaskInstance         task;
        struct Suiel_control_checkbox   run;
        struct Suiel_control_checkbox   THP[3];
        struct Suiel_control_list       m_rate;
        struct Suiel_control_numeric    lenght;
        struct Suiel_control_pushbutton reallocate;
    };

    struct SUISetRegTaskMemory
    {
        uint32                          task_index;
        struct SRegTaskInstance         task[4];    // task setups
        struct Suiel_control_numeric    select;     // task selector
        struct Suiel_control_numeric    start;      // start address
        struct Suiel_control_numeric    lenght;     // sequence 
    };

    union UUIstatusParams
    {
        // main window stuff
        struct SUIMainGaugeThermo  mgThermo;
        struct SUIMainGaugeHygro   mgHygro;
        struct SUIMainGaugePress   mgPress;

        struct SUISetQuickSwitch    swQuickSw;      // params for quick switch
        struct SUISetRegTaskSet     swRegTaskSet;
        struct SUISetRegTaskMemory  swRegTaskMem;

    };


    enum EPopupAction
    {
        uipa_close = 0,     // One "Close" button
        uipa_ok,            // One "OK" button
        uipa_ok_cancel,     // "OK" and "Cancel" buttons, Cancel is the default
    };


    struct SPopupSetup
    {
        uint32 line1;   // pointer to the text line1
        uint32 line2;   // pointer to the text line2 
        uint32 line3;   // pointer to the text line2 
        uint8 x1;       // coordinates of line 1
        uint8 y1;
        uint8 y2;
        uint8 x3;       // coordinates of line 2
        uint8 y3; 

        uint8 popup_action;

        uint8 style1;   // style of line 1
        uint8 style3;   // style of line 2
    };

    struct SPopUpWindow
    {
        struct SPopupSetup  params;
        uint8 focus;
        uint8 elems;
        uint32 ret_state;
        struct Suiel_control_pushbutton pb1;
        struct Suiel_control_pushbutton pb2;
    };
    

    struct SUIstatus
    {
        uint32 m_state;             // main ui state - see enum EUIStates
        uint32 m_substate;          // 0 means state entry
        uint32 m_setstate;          // internal state for windows with settings - see enum EUISetupSelect
        uint32 m_return;            // return value set by inner state for an outer state
        enum UIMainMode main_mode;  // mode for gauge/graph display - selects bw. temperature - humidity - pressure

        void *ui_elems[ UI_MAX_ELEMS ]; // active elements on the current screen
        uint32 ui_elem_nr;              // number of active elements

        int focus;                      // element in focus - it is 1-based, if 0 then nothing is selected on the screen

        union UUIstatusParams   p;      // parameters for the curent ui state
        struct SPopUpWindow     popup;

        uint32 pwr_sched_dim;       // inactivity time scheduler for display dimming
        uint32 pwr_sched_dispoff;   // inactivity time scheduler for display off
        uint32 pwr_sched_pwroff;    // inactivity time scheduler for power down
        uint32 pwr_state;           // current power management state
        bool pwr_dispdim;           // display is dimmed
        bool pwr_dispoff;           // display is off
        bool pwr_dispinit;          // if display is initted

        uint16 upd_batt;        // battery update counter
        uint16 upd_dynamics;    // update dynamic stuff
        uint32 upd_time;        // old saved clock- used to compare with current one and update display if needed

        uint32 upd_ui_disp;     // display update type to be used at polling. It will set in ui callbacks also
    };

    #define UI_REG_TO_BEFORE    0x00
    #define UI_REG_TO_REALLOC   0x01
    #define UI_REG_OK_PRESSED   0x10

    // callbacks
    void ui_call_maingauge_esc_pressed( int context, void *pval );
    
    void ui_call_maingauge_thermo_unit_toDefault( int context, void *pval );
    void ui_call_maingauge_thermo_unit_vchange( int context, void *pval  );
    void ui_call_maingauge_thermo_minmax_toDefault( int context, void *pval );
    void ui_call_maingauge_thermo_minmax_vchange( int context, void *pval );
    
    void ui_call_maingauge_hygro_unit_toDefault( int context, void *pval );
    void ui_call_maingauge_hygro_unit_vchange( int context, void *pval );
    void ui_call_maingauge_hygro_minmax_toDefault( int context, void *pval );
    void ui_call_maingauge_hygro_minmax_vchange( int context, void *pval );

    void ui_call_setwindow_quickswitch_op_switch( int context, void *pval );
    void ui_call_setwindow_quickswitch_op_switch_ok( int context, void *pval );
    void ui_call_setwindow_quickswitch_op_switch_cancel( int context, void *pval );
    void ui_call_setwindow_quickswitch_monitor_rate( int context, void *pval );
    void ui_call_setwindow_quickswitch_monitor_rate_val( int context, void *pval );
    void ui_call_setwindow_quickswitch_esc_pressed( int context, void *pval );
    void ui_call_setwindow_quickswitch_reset_minmax( int context, void *pval );
    void ui_call_setwindow_quickswitch_reset_minmax_ok( int context, void *pval );
    void ui_call_setwindow_quickswitch_task_ok( int context, void *pval );

    void ui_call_setwindow_regtaskset_next_action( int context, void *pval );
    void ui_call_setwindow_regtaskset_close( int context, void *pval );

    void ui_call_setwindow_regtaskmem_exit( int context, void *pval );
    void ui_call_setwindow_regtaskmem_chtask( int context, void *pval );
    void ui_call_setwindow_regtaskmem_chstart( int context, void *pval );
    void ui_call_setwindow_regtaskmem_chlenght( int context, void *pval );

    // routines
    void uist_drawview_modeselect( int redraw_type );
    void uist_drawview_mainwindow( int redraw_type );
    void uist_drawview_setwindow( int redraw_type );
    void uist_drawview_debuginputs( int redraw_type, uint32 key_bits );
    void uist_drawview_popup(int redraw_type);

    void uist_setupview_modeselect( bool reset );
    void uist_setupview_mainwindow( bool reset );
    void uist_setupview_setwindow( bool reset );
    void uist_setupview_debuginputs( void );


#ifdef __cplusplus
    }
#endif


#endif // UI_INTERNALS_H
