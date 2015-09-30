#ifndef UI_H
#define UI_H

#ifdef __cplusplus
    extern "C" {
#endif

    #include "events_ui.h"
    #include "core.h"


    void ui_init( struct SCore *instance );

    int ui_poll( struct SEventStruct *evmask );


#ifdef __cplusplus
    }
#endif


#endif // UI_H
