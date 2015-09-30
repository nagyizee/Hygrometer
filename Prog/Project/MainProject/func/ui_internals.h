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
        UI_STATE_STARTUP = 0,
        UI_STATE_MAIN_WINDOW,
        UI_STATE_MENU,
        UI_STATE_SET_WINDOW,
        UI_STATE_STARTED,
        UI_STATE_SCOPE,
        UI_STATE_SHUTDOWN
    };

    enum EUISetStatus
    {
        UI_SSET_PRETRIGGER = 1,
        UI_SSET_COMBINE,
        UI_SSET_SHUTTER,
        UI_SSET_SETUPMENU,
        // setup menu (don't change order)
        UI_SSET_DISPBRT,
        UI_SSET_PWRSAVE,
        UI_SSET_RESET
    };



    // user interface substate
    #define UI_SUBST_ENTRY          0

    // maximum user interface elements on screen
    #define UI_MAX_ELEMS            5

    // display redraw options
    #define RDRW_BATTERY            0x01        // 0000 0001b - redraw battery only
    #define RDRW_STATUSBAR          0x03        // 0000 0011b - redraw battery and status bar
    #define RDRW_UI_CONTENT         0x04        // 0000 0100b - redraw ui content
    #define RDRW_UI_DYNAMIC         0x08        // 0000 1000b - redraw ui dynamic elements only (high update rate stuff like indicators)
    #define RDRW_UI_CONTENT_ALL     0x0C        // 0000 1100b - redraw ui content including dynamic elements
    #define RDRW_ALL                0x0F        // redraw everything

    #define RDRW_DISP_UPDATE        0x80        // just a dummy value to enter to the display update routine

    struct SUIMainCablerel
    {
        struct Suiel_control_checkbox   prefocus;
        struct Suiel_control_time       time;
    };

    struct SUIMainLongexpo
    {
        struct Suiel_control_time   time;
    };

    struct SUIMainInterval
    {
        struct Suiel_control_edit       shot_nr;
        struct Suiel_control_time       iv;
    };

    struct SUIMainTimer
    {
        struct Suiel_control_time       time;
    };

    struct SUIMainLight
    {
        struct Suiel_control_numeric    gain;
        struct Suiel_control_numeric    thold;
        struct Suiel_control_checkbox   trigH;
        struct Suiel_control_checkbox   trigL;
        struct Suiel_control_checkbox   bright;
    };


    struct SUISetCombineModes
    {
        struct Suiel_control_checkbox   mode[5];
    };

    struct SUISetShutter 
    {
        struct Suiel_control_checkbox   mlock_en;
        struct Suiel_control_numeric    mlock;
        struct Suiel_control_numeric    shtrp;
    };

    struct SUISetDispbrt
    {
        struct Suiel_control_numeric    full;
        struct Suiel_control_numeric    dimmed;
        struct Suiel_control_checkbox   en_beep;
    };

    struct SUISetPowerSave
    {
        struct Suiel_control_list       stby;
        struct Suiel_control_list       dispoff;
        struct Suiel_control_list       pwroff;
    };

    struct SUISetDefault
    {
        struct Suiel_control_checkbox   no;
        struct Suiel_control_checkbox   yes;
    };

    #define UISCOPE_HEX     0x00    // hexa display
    #define UISCOPE_DEC     0x01    // decimal display
    #define UISCOPE_UNIT    0x02    // converted unit display

    #define UISM_VALUELIST  0x00
    #define UISM_SCOPE_LOW  0x01
    #define UISM_SCOPE_HI   0x02

    #define UISA_SAMPLERATE 0x00
    #define UISA_GAIN       0x01
    #define UISA_TRIGGER    0x02

    struct SUIScope
    {
        uint16  raws[8];            // raw measurement values:  0-low, 1-high, 2-lowmin, 3-highmin, 4-lowmax, 5-highmax, 6-base
        uint16  pwrorg_dim;
        uint16  pwrorg_doff;
        uint16  pwrorg_off;
        uint8   disp_unit;          // display format
        uint8   scope_mode;         // scope mode
        uint8   adj_mode;           // adjustment mode in scope display
        uint8   osc_trigger;        // trigger level - 0 - not used, 1 - 64 - level from middle line to display top
        uint8   osc_gain;           // gain from 1 - 200: 1- 4096 is the display top, 20 is the display top
        uint8   tmr_run;            // run mode
        uint32  tmr_pre;            // prescaler value for display refresh
        uint32  tmr_ctr;            // counter for prescaler
    };

    union UUIstatusParams
    {
        // main window stuff
        struct SUIMainCablerel  cable;
        struct SUIMainLongexpo  longexpo;
        struct SUIMainInterval  interval;
        struct SUIMainTimer     timer;
        struct SUIMainLight     light;

        // setwindow stuff
        struct Suiel_control_edit   set_pretrigger;
        struct SUISetCombineModes   set_combine;
        struct SUISetShutter        set_shutter;
        struct SUISetDispbrt        set_brightness;
        struct SUISetPowerSave      set_powersave;
        struct SUISetDefault        set_default;
        struct SUIScope             scope;

        // menu stuff
        struct Suiel_dropdown_menu  menu;

    };



    struct SUIstatus
    {
        uint32 m_state;
        uint32 m_substate;      // 0 means entry
        uint32 m_setstate;      // internal state for windows with settings
        uint32 m_return;
        uint32 main_mode;       // main window selected mode ( currently on-display ) - see OPMODE_xxx in core.h an from ui.h
        bool setup_mod;         // true if setup parameter is modified
        void *ui_elems[ UI_MAX_ELEMS ];
        uint32 ui_elem_nr;

        int focus;

        union UUIstatusParams   p;

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
