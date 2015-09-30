
#include "events_ui.h"
#include "hw_stuff.h"
#include "core.h"

//////////////////////////////////////////////////////////////////
//
//      ISR RELATED DEFINES AND ROUTINES - handle with care
//
//////////////////////////////////////////////////////////////////

// Debug feature for loop efficiency
//#define STATISTICKS
#define STAT_TMR_CAL

#ifdef ON_QT_PLATFORM
static struct STIM1 stim;
static struct STIM1 *TIM1 = &stim;
#endif


volatile struct SEventStruct events = { 0, };
static volatile uint32 counter = 0;
static volatile uint32 sec_ctr = 0;
static volatile uint32 rcc_comp = 0x10;
static volatile uint32 tmr_over = 0;

#ifdef STAT_TMR_CAL
static volatile uint32 tmr_over_max = 0;
static volatile uint32 tmr_under_max = 0;
#endif

#define BEEP_MASK           0x07
#define BEEP_IS_ON          0x04
#define BEEP_PITCH_LOW      0x02
#define BEEP_DURATION       0x01

#define BEEP_DUR_ON_LONG    30
#define BEEP_DUR_ON_SHORT   7
#define BEEP_DUR_OFF_LONG   50
#define BEEP_DUR_OFF_SHORT  20
#define BEEP_DUR_PAUSE      2

struct SBeep
{
    uint32  seq;
    uint16  freq_low;
    uint16  freq_hi;
    uint16  timer;
    uint16  active;
} beep = {0, };

extern void DispHAL_ISR_Poll(void);


    void TimerSysIntrHandler(void)
    {
        // !!!!!!! IMPORTANT NOTE !!!!!!!!
        // IF timer 16 in use - check for the interrupt flag

        // Clear update interrupt bit
        TIMER_SYSTEM->SR = (uint16)~TIM_FLAG_Update;

        if ( sec_ctr < 2000 )  // execute this isr only for useconds inside the 1second interval
        {
            counter++;
            sec_ctr++;
            events.timer_tick_system = 1;

            if ( counter == SYSTEM_T_10MS_COUNT )
            {
                events.timer_tick_10ms = 1;
                counter = 0;
            }
            DispHAL_ISR_Poll();
        }
        else
        {
            // TODO: can do stats - detect faster than normal internal osclillator speed
            tmr_over++;
        }

    }//END: Timer1IntrHandler


    void TimerRTCIntrHandler(void)
    {
        // clear the interrrupt flag and update clock alarm for the next second
        TIMER_RTC->CRL &= (uint16)~RTC_ALARM_FLAG;
        RTC_WaitForLastTask();
        RTC_SetAlarm( RTC_GetCounter() + 1024 ); 

        // adjust internal oscillator for precision clock
       if ( tmr_over )                         // timer overshoot
        {
            if ( rcc_comp > 0 )
            {
                rcc_comp--;
                RCC_AdjustHSICalibrationValue( rcc_comp );
            }
        }
        else if ( sec_ctr < 1999 )              // timer undershoot
        {
            if ( rcc_comp < 0x1F )
            {
                rcc_comp++;
                RCC_AdjustHSICalibrationValue( rcc_comp );
            }
        }


    #ifdef STAT_TMR_CAL
        if ( (tmr_over) && (tmr_over > tmr_over_max) )
        {
            tmr_over_max = tmr_over;
        }
        else if ( (sec_ctr < 1999) && ( (1999-sec_ctr) > tmr_under_max ) && (sec_ctr > 1250) )
        {
            tmr_under_max = (1999-sec_ctr);
        }
    #endif
    


        // reset everything and signal the events
        tmr_over = 0;
        counter = 0;
        sec_ctr = 0;
        events.timer_tick_system = 1;
        events.timer_tick_10ms = 1;
        events.timer_tick_1sec = 1;
    }//END: Timer1IntrHandler



//////////////////////////////////////////////////////////////////
//
//      API and internal routines
//
//////////////////////////////////////////////////////////////////


    static uint32 internal_get_keys()
    {
        uint32 keys = 0;
        uint32 enc;

        if ( BtnGet_OK() )
            keys |= KEY_OK;
        if ( BtnGet_Mode() )
            keys |= KEY_MODE;
        if ( BtnGet_Up() )
            keys |= KEY_UP;
        if ( BtnGet_Down() )
            keys |= KEY_DOWN;
        if ( BtnGet_Left() )
            keys |= KEY_LEFT;
        if ( BtnGet_Right() )
            keys |= KEY_RIGHT;
        if ( BtnGet_Esc() )
        {
            keys |= KEY_ESC;
        #ifdef STAT_TMR_CAL
            tmr_over_max = 0;
            tmr_under_max = 0;
        #endif
        }

        return keys;
    }

    uint32   keys_old        = 0;    // bitmask with previous key state
    uint8    keys_strokepause[8];

    void local_process_button( struct SEventStruct *evt)
    {
        uint32 crt_keys  = internal_get_keys();
        uint32 changed   = crt_keys ^ keys_old;
        uint32 keys_on   = changed & crt_keys;              // newly pressed
        uint32 keys_off  = changed & (~crt_keys) & 0xff;    // newly released

        int poz = 0x01;
        int pctr = 0;
        while ( pctr < 6 )
        {
            if ( keys_on & poz )    // newly pressed key
            {
                keys_strokepause[pctr]  = 200;
                evt->key_pressed |= (1 << pctr);
                evt->key_event = 1;
            }

            if ( keys_off & poz )   // newly released key
            {
                if ( keys_strokepause[pctr] )
                {
                    evt->key_released |= (1 << pctr);
                    evt->key_event = 1;
                }
            }

            if ( (crt_keys & poz) ) // if key is currently pressed
            {
                if ( keys_strokepause[pctr] )
                {
                    keys_strokepause[pctr]--;       // count down for first stroke

                    if ( keys_strokepause[pctr] == 0 )
                    {
                        evt->key_longpressed |= (1 << pctr);
                        evt->key_event = 1;
                    }
                }
            }

            poz = poz << 1;
            pctr++;
        }

        keys_old = crt_keys;
    }

    //  [beep][pitch][duration]
    //      beep:       1 - beep, 0 - no beep
    //      pitch:      1 - low,  0 - high
    //      duration:   1 - long, 0 - short
    void local_beep_play_element()
    {
        if ( (beep.seq & BEEP_MASK) == 0 )  // check for end of sequence
        {
            beep.seq = 0;
            beep.timer = 0;
            beep.active = 0;
            return;
        }

        if ( beep.seq & BEEP_IS_ON )        // beep needs to be generated
        {
            HW_Buzzer_On( (beep.seq & BEEP_PITCH_LOW) ? beep.freq_low : beep.freq_hi );
            beep.timer = (beep.seq & BEEP_DURATION)? BEEP_DUR_ON_LONG : BEEP_DUR_ON_SHORT;
        } 
        else                                // pause to be inserted
        {
            beep.timer = (beep.seq & BEEP_DURATION)? BEEP_DUR_OFF_LONG : BEEP_DUR_OFF_SHORT;
        }
        beep.active = 1;

        beep.seq = beep.seq >> 3;           // advance to the next sequence
    }


    void local_process_beep(void)
    {
        if ( BeepIsRunning() )
        {
            if ( beep.timer )
            {
                beep.timer --;
                return;
            }
    
            if ( beep.active == 1 )         // there was an active beep
            {
                HW_Buzzer_Off();
                beep.active = 0;
                beep.timer = BEEP_DUR_PAUSE;
            }
            else
            {
                local_beep_play_element();
            }
        }
    }


    void BeepSequence( uint32 seq )
    {
        beep.seq = seq;
        local_beep_play_element();
    }

    void BeepSetFreq( int low, int high )
    {
        beep.freq_hi = high;
        beep.freq_low = low;
    }

    uint32 BeepIsRunning( void )
    {
        return ( beep.seq || beep.active );
    }


    struct SEventStruct Event_Poll( void )
    {
        struct SEventStruct evtemp = { 0, };

        if ( events.timer_tick_10ms )
        {
            local_process_button( &evtemp );
            local_process_beep( );
        }

        __disable_interrupt();
        *((uint32*)&events) |= *((uint32*)&evtemp);
        __enable_interrupt();

        return events;
    }//END: Event_Poll


    void Event_Clear( struct SEventStruct evmask)
    {
        uint32 *ev  = (uint32*)&events;
        uint32 tmp  = *(  (uint32*)(&evmask) );

        __disable_interrupt();
        *ev &= ~( tmp );
        if ( evmask.reset_sec )
        {
            sec_ctr = 0;
        }
        __enable_interrupt();

    }//END: Event_Clear
