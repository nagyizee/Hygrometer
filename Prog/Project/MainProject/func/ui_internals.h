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
        UI_STATE_SHUTDOWN                       // shut-down window
    };


    // user interface substate
    #define UI_SUBST_ENTRY          0

    // maximum user interface elements on screen
    #define UI_MAX_ELEMS            10

    // display redraw options
    #define RDRW_BATTERY            0x01        // 0000 0001b - redraw battery only
    #define RDRW_STATUSBAR          0x03        // 0000 0011b - redraw battery and status bar
    #define RDRW_UI_CONTENT         0x04        // 0000 0100b - redraw ui content
    #define RDRW_UI_DYNAMIC         0x08        // 0000 1000b - redraw ui dynamic elements only (high update rate stuff like indicators)
    #define RDRW_UI_CONTENT_ALL     0x0C        // 0000 1100b - redraw ui content including dynamic elements
    #define RDRW_ALL                0x0F        // redraw everything

    #define RDRW_DISP_UPDATE        0x80        // just a dummy value to enter to the display update routine


    #define UIMODE_GAUDE_THERMO     0x00
    #define UIMODE_GAUGE_HYGRO      0x01
    #define UIMODE_GAUGE_PRESSURE   0x02


    struct SUIMainGaugeThermo
    {
        struct Suiel_control_numeric    temp;
    };


    union UUIstatusParams
    {
        // main window stuff
        struct SUIMainGaugeThermo  mgThermo;
    };



    struct SUIstatus
    {
        uint32 m_state;         // main ui state - see enum EUIStates
        uint32 m_substate;      // 0 means state entry
        uint32 m_setstate;      // internal state for windows with settings
        uint32 m_return;        // return value set by inner state for an outer state
        uint32 main_mode;       // mode for gauge/graph display - selects bw. temperature - humidity - pressure

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
    };


#ifdef __cplusplus
    }
#endif


#endif // UI_INTERNALS_H
