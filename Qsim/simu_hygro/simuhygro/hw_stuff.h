#ifndef HW_STUFF_H
#define HW_STUFF_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32f10x.h"
#include "typedefs.h"

enum EHWAxisPower
{
    pwr_min = 0,
    pwr_med,
    pwr_high,
    pwr_max
};


// Always update from the real hw_stuff.h
#define SYSTEM_T_10MS_COUNT      20             // 10ms is 40 timer ticks
#define SYSTEM_MAX_TICK          4000           // timer ticks per second  ( 250us )

#define TIMER_SYSTEM            TIM1
#define TIMER_RTC               TIM1
#define ADC1                    TIM1
#define RTC_ALARM_FLAG          ((uint16)0x0002)
#define ADC_IF_EOC              ((uint32)0x00000002)
#define ADC_IE_EOC              ((uint32)0x00000020)


#define LED_RED     0
#define LED_GREEN   1

#define ENCODER_MAX 20

// Dummy stuff for simulation
struct STIM1
{
    uint16 SR;
    uint16 CRL;
};

#define TIM_FLAG_Update     0
// -----------

// all these modes are mutally exclussive per group
    #define SYSSTAT_DISP_BUSY           0x0001          // flag indicating that display is busy
    #define SYSSTAT_CORE_BULK           0x0002          // core needs bulk calculation, need to run main loop continuously
    #define SYSSTAT_CORE_RUN_FULL       0x0004          // core run with light or sound detection, may sleep at end of main loop
    #define SYSSTAT_CORE_RUN_LOW        0x0008          // core run with low speed operations. execution is needed only in 1sec. interval
    #define SYSSTAT_CORE_STOPPED        0x0010          // core stopped. no operation in progress
    #define SYSSTAT_UI_ON               0x0020          // ui is fully functional, display is eventually dimmed 
    #define SYSSTAT_UI_STOPPED          0x0080          // ui stopped, wake up on keypress but keys are not captured
    #define SYSSTAT_UI_STOP_W_ALLKEY    0x0100          // ui stopped, wake up with immediate key action for any key
    #define SYSSTAT_UI_STOP_W_SKEY      0x0200          // ui stopped, wake up with immediate key action for 'Start' key
    #define SYSSTAT_UI_PWROFF           0x8000          // ui.pwroff timeout reached and no user action and no core run. OR power button (long press on pwr/mode button)


    #define HWSLEEP_FULL            0x00            // continuous main loop, no sleep mode in use
    #define HWSLEEP_WAIT            0x01            // sleepmode - wait for irq - cpu clock down only - sysclock is working
    #define HWSLEEP_STOP            0x02            // stop mode - wake-up at keypress or rtc 1sec. clock
    #define HWSLEEP_STOP_W_ALLKEYS  0x03            // same as the mode abowe - but all keys are detected with rising edge - to generate UI keyboard event also 
    #define HWSLEEP_STOP_W_SKEY     0x04            // same as HWSLEEP_STOP - but 'Start' key will be detected with rising edge
    #define HWSLEEP_OFF             0x05            // shuts down the system


void InitHW(void);

bool BtnGet_OK();
bool BtnGet_Esc();
bool BtnGet_Mode();
bool BtnGet_Up();
bool BtnGet_Down();
bool BtnGet_Left();
bool BtnGet_Right();
bool BtnPollStick();                // the stick used for directional control uses combination of signals - this routine polls it's status and prevents faulty reads


void TimerSysIntrHandler(void);
void TimerRTCIntrHandler(void);


void HW_ASSERT();


// just a wrapper solution
#define BTN_MODE    0
#define BTN_OK      1
#define BTN_ESC     2
#define BTN_UP      3
#define BTN_DOWN    4
#define BTN_LEFT    5
#define BTN_RIGHT   6


void main_entry(uint32 *stack_top);
void main_loop(void);

void DispHAL_UpdateScreen();
void DispHAL_ISR_Poll();
void CoreADC_ISR_Complete();
void Core_ISR_PretriggerCompleted();


#define SENSOR_TEMP     0x01
#define SENSOR_RH       0x02
#define SENSOR_PRESS    0x04

#define SENSOR_VALUE_FAIL 0xffffffff

// init sensor module
void Sensors_Init();
// shut down individual sensor block ( RH and temp are in one - they need to be provided in pair )
void Sensors_Shutdown( uint32 mask );
// set up acquire request on one or more sensors ( it will wake up the sensor if needed )
void Sensors_Acquire( uint32 mask );
// check the sensor state - returning a mask with the sensors which have read out value
uint32 Sensor_Is_Ready(void);
// check the sensor state - returning a mask busy sensors
uint32 Sensor_Is_Busy(void);
// get the acquired value from the sensor (returns the base formatted value from a sensor - Temp: 16fp9+40*, RH: 16fp8, Press: TBD) and clears the reagy flag
uint32 Sensor_Get_Value( uint32 sensor );
// sensor submodule polling
void Sensor_Poll(bool tick_ms);

// polled for each ms
void Sensor_simu_poll();

// they don't belong here normally

void HW_Seconds_Start(void);    // set up RTC 1 second interrupt for period beginning from this moment
void HW_Seconds_Restore(void);  // restore the original interrupt interval

void HW_Buzzer_On(int pulse);   // Pulse is in 8MHz units
void HW_Buzzer_Off(void);

#define RTC_WaitForLastTask()           do {  } while(0)
#define RTC_SetAlarm(a)                 do {  } while(0)
#define RTC_GetCounter()                RTCctr + 1
#define RCC_AdjustHSICalibrationValue(a)do {  } while(0)

#define VBAT_MIN        0x0000                   // 2.6V
#define VBAT_MAX        0x0fff                   // 3.1V
#define VBAT_DIFF       ( VBAT_MAX - VBAT_MIN )


uint32 HW_ADC_GetBattery(void);

void HW_PWR_An_On( void );
void HW_PWR_An_Off( void );

void ADC_ISR_simulation(void);

void HW_EXTI_ISR( void );
bool HW_Sleep(int mode);

void HWDBG( int val );


int HWDBG_Get_Temp();           // temperature in 16fp9 + 40*C
int HWDBG_Get_Humidity();
int HWDBG_Get_Pressure();


#define __disable_interrupt()       do {  } while(0)
#define __enable_interrupt()        do {  } while(0)


#ifdef __cplusplus
 }
#endif


#endif // HW_STUFF_H
