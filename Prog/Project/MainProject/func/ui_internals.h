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
        UI_STATE_STARTUP = 0,                   // state entered after power on (by user) and device was shut down before (no monitoring)
        UI_STATE_MAIN_GAUGE,                    // main window with value gauges (temperature, humidity, pressure)
        UI_STATE_MAIN_GRAPH,                    // graph display for the abowe stuff
        UI_STATE_MAIN_ALTIMETER,                // altimeter mode - this include variometer function
        UI_STATE_MODE_SELECT,                   // mode selector window - navigate bw. gauge - graph - altimeter - recording - setup
        UI_STATE_SHUTDOWN,                      // shut-down window
        UI_STATE_DBG_INPUTS,                    // debug button press, sensor readings, etc
    };

    enum UIMainMode
    {
        UImm_gauge_thermo = 0,
        UImm_gauge_hygro,
        UImm_gauge_pressure,
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


    union UUIstatusParams
    {
        // main window stuff
        struct SUIMainGaugeThermo  mgThermo;
        struct SUIMainGaugeHygro   mgHygro;
    };



    struct SUIstatus
    {
        uint32 m_state;             // main ui state - see enum EUIStates
        uint32 m_substate;          // 0 means state entry
        uint32 m_setstate;          // internal state for windows with settings
        uint32 m_return;            // return value set by inner state for an outer state
        enum UIMainMode main_mode;  // mode for gauge/graph display - selects bw. temperature - humidity - pressure

        bool setup_mod;         // true if setup parameter is modified - setup will be saved in eeprom in case of shutdown
        void *ui_elems[ UI_MAX_ELEMS ]; // active elements on the current screen
        uint32 ui_elem_nr;              // number of active elements

        int focus;                      // element in focus - it is 1-based, if 0 then nothing is selected on the screen

        union UUIstatusParams   p;      // parameters for the curent ui state

        uint32 incativity;      // inactivity counter
        uint32 pwr_state;       // current power management state
        bool pwr_dispdim;       // display is dimmed
        bool pwr_dispoff;       // display is off

        uint16 upd_batt;        // battery update counter
        uint16 upd_dynamics;    // update dynamic stuff
        uint32 upd_time;        // old saved clock- used to compare with current one and update display if needed
    };


    void uist_drawview_modeselect( int redraw_type );
    void uist_drawview_mainwindow( int redraw_type );
    void uist_drawview_debuginputs( int redraw_type, uint32 key_bits );

    void uist_setupview_modeselect( bool reset );
    void uist_setupview_mainwindow( bool reset );
    void uist_setupview_debuginputs( void );


#ifdef __cplusplus
    }
#endif


#endif // UI_INTERNALS_H
