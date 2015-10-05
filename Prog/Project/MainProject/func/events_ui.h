/**
  ******************************************************************************
  * 
  *   System and User Interface related routines and event
  *   handling
  *   Handles the system timer, sensor infos and button / led actions
  * 
  ******************************************************************************
  */ 

#ifndef __EVENT_UI_H
#define __EVENT_UI_H

#ifdef __cplusplus
    extern "C" {
#endif


    #include "stm32f10x.h"
    #include "typedefs.h"


    #define KEY_UP          0x01            // |
    #define KEY_DOWN        0x02            // |
    #define KEY_LEFT        0x04            // |  these keys will generate repeated accelerated key_pressed event when long pressed
    #define KEY_RIGHT       0x08            // |  will not generate key_released and key_longpressed
    #define KEY_OK          0x10            // -- will not generate key key_longpressed, neither repeated key_pressed
    #define KEY_ESC         0x20            // |
    #define KEY_MODE        0x40            // |  these keys will generate key_longpressed and key_released

    struct SEventStruct
    {
        uint32  key_pressed:8;              // pressed keys
        uint32  key_longpressed:8;          // long pressed keys
        uint32  key_released:8;             // released keys    - long pressed is mutually exclussive with released

        uint32  timer_tick_system:1;        // system tick (200us)
        uint32  timer_tick_10ms:1;          // 10ms timing tick
        uint32  timer_tick_1sec:1;          // 1second timet tick - precise RTC clock
        uint32  timer_tick_05sec:1;         // 0.5second timet tick - precise RTC clock

        uint32  key_event:1;                // key status changed

        uint32  reset_sec:1;                // application settable sec. counter reset
    };


    
    // Beep a given sequence
    // Sequence is a bit list scrolled from left to right. LSB is first
    // elements are packed:  nnn....222111000 - 
    // where each element consists:   [beep][pitch][duration]
    //      beep:       1 - beep, 0 - no beep
    //      pitch:      1 - low,  0 - high
    //      duration:   1 - long, 0 - short
    // sequence element [000] ( no beep, high pitch, short ) means end of sequence
    void BeepSequence( uint32 seq );
    // set frequency for low and high beep
    void BeepSetFreq( int low, int high );
    // if beep sequence is running
    uint32 BeepIsRunning( void );

    // Polls the system and user interface events,(led blinks, sensor, buttons, etc )
    struct SEventStruct Event_Poll( void );

    // clear the events which are processed
    // parameters: - evmask: events which are confirmed by application
    void Event_Clear( struct SEventStruct evmask );

#ifdef __cplusplus
    }
#endif

#endif
