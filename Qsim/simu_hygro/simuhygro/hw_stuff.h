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
#define SYSTEM_T_10MS_COUNT      10             // 10ms is 10 timer ticks
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
    #define SYSSTAT_CORE_RUN_FULL       0x0004          // core run with high speed clock needed (1ms ticks)
    #define SYSYTAT_CORE_MONITOR        0x0010          // core in monitoring / fast registering mode, maintain memory - no full power down possible
    #define SYSSTAT_CORE_STOPPED        0x0020          // core stopped, no operation in progress

    #define SYSSTAT_UI_ON               0x0100          // ui is fully functional, display is eventually dimmed 
    #define SYSSTAT_UI_WAKEUP           0x0200          // ui is in wake-up state after pm_down / pm_hold power mode - if long press on power button or UI wakeup event - UI will be up and running, otherwise is considered as 
    #define SYSSTAT_UI_STOPPED          0x0400          // ui stopped, wake up on keypress but keys are not captured
    #define SYSSTAT_UI_STOP_W_ALLKEY    0x0800          // ui stopped, wake up with immediate key action for any key
    #define SYSSTAT_UI_PWROFF           0x1000          // ui is in off mode, or power off requested


    enum EPowerMode
    {
        pm_full = 0,            // full cpu power
        pm_sleep,               // executes one loop after an interrupt source ( interrupt sources in simu are - 1ms tick timer, 0.5s (or scheduled) RTC timer ticks )
        pm_hold_btn,            // CPU core/periph. stopped, Exti and RTC wake-up only - wake up uppon button operation and RTC alarm
        pm_hold,                // CPU core/periph. stopped, RTC wake-up and Pwr button wake up only.
        pm_down,                // all electronics switched off, RTC alarm will wake it up, Starts from reset state
                                //    - in simulation - app. will respond only for pwr button and RTC alarm, by starting from INIT
        pm_close                // used for simulation only - closes the application
    };


void InitHW(void);

bool BtnGet_OK();
bool BtnGet_Esc();
bool BtnGet_Mode();
bool BtnGet_Up();
bool BtnGet_Down();
bool BtnGet_Left();
bool BtnGet_Right();
bool BtnPollStick();                // the stick used for directional control uses combination of signals - this routine polls it's status and prevents faulty reads

void HW_LED_On();

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
void Set_WakeUp(void);

void DispHAL_UpdateScreen();
void DispHAL_ISR_Poll();
void CoreADC_ISR_Complete();
void Core_ISR_PretriggerCompleted();

#define TEMP_FP         9           // use 16bit fixpoint at 9 bits for temperature
#define RH_FP           8

#define SENSOR_TEMP     0x01
#define SENSOR_RH       0x02
#define SENSOR_PRESS    0x04

#define SENSOR_VALUE_FAIL 0xffffffff

// init sensor module
void Sensor_Init();
// shut down individual sensor block ( RH and temp are in one - they need to be provided in pair )
void Sensor_Shutdown( uint32 mask );
// set up acquire request on one or more sensors ( it will wake up the sensor if needed )
void Sensor_Acquire( uint32 mask );
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
#define RCC_AdjustHSICalibrationValue(a)do {  } while(0)

#define VBAT_MIN        0x0000                   // 2.6V
#define VBAT_MAX        0x0fff                   // 3.1V
#define VBAT_DIFF       ( VBAT_MAX - VBAT_MIN )

uint32 RTC_GetCounter(void);
void RTC_SetAlarm(uint32);
void HW_SetRTC(uint32 RTCctr);

uint32 HW_ADC_GetBattery(void);

void ADC_ISR_simulation(void);

void HW_EXTI_ISR( void );
bool HW_Sleep( enum EPowerMode mode);

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
