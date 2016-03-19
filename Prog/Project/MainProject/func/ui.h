#ifndef UI_H
#define UI_H

#ifdef __cplusplus
    extern "C" {
#endif

    #include "events_ui.h"
    #include "core.h"

    #define SYSSTAT_UI_ON               PM_SLEEP            // ui is fully functional, display is eventually dimmed 
    #define SYSSTAT_UI_STOPPED          PM_HOLD             // ui stopped, wake up on keypress but keys are not captured
    #define SYSSTAT_UI_STOP_W_ALLKEY    PM_HOLD_BTN         // ui stopped, wake up with immediate key action for any key
    #define SYSSTAT_UI_PWROFF           PM_DOWN             // ui is in off mode, or power off requested

    uint32 ui_init( struct SCore *instance );

    uint32 ui_poll( struct SEventStruct *evmask );


#ifdef __cplusplus
    }
#endif


#endif // UI_H
