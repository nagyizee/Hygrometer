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


    enum EPowerMode
    {
        pm_full = 0,            // full cpu power
        pm_sleep,               // executes one loop after an interrupt source ( interrupt sources in simu are - 1ms tick timer, 0.5s (or scheduled) RTC timer ticks )
        pm_hold_btn,            // CPU core/periph. stopped, Exti and RTC wake-up only - wake up uppon button operation and RTC alarm
        pm_hold,                // CPU core/periph. stopped, RTC wake-up and Pwr button wake up only.
        pm_down,                // all electronics switched off, RTC alarm will wake it up, Starts from reset state
                                //    - in simulation - app. will respond only for pwr button and RTC alarm, by starting from INIT
        pm_close,               // used for simulation only - closes the application
        pm_exti                 // used for simulation only - notifies EXTI event
    };

    // power mode bitmasks
    #define PM_FULL     (1<<pm_full)
    #define PM_SLEEP    (1<<pm_sleep)
    #define PM_HOLD_BTN (1<<pm_hold_btn)
    #define PM_HOLD     (1<<pm_hold)
    #define PM_DOWN     (1<<pm_down)
    #define PM_MASK     0xff

    // power modes/component
    // all these modes are mutally exclussive per group
    #define SYSSTAT_DISP_BUSY           PM_SLEEP        // flag indicating that display is busy

    // wake up reasons
    #define WUR_NONE        0x00
    #define WUR_FIRST       0x01        // first startup
    #define WUR_USR         0x02        // user produced wake-up condition (button pressed) - from InitHW (reset state): when Power button pressed by user
                                        //                                                  - from HW_Sleep(): when Power or other buttons are pressed by user (power mode dependent)
    #define WUR_RTC         0x04        // timer produced wake-up condition - from InitHW or HW_Sleep(): when RTC counter = Alarm counter  - can be combined with WUR_USR if button pressed
    #define WUR_SENS_IRQ    0x08        // sensor IRQ line produced wake-up from HW_Sleep() - when IRQ line is toggled in stopped state



void InitHW(void);

bool BtnGet_OK();
bool BtnGet_Esc();
bool BtnGet_Mode();
bool BtnGet_Up();
bool BtnGet_Down();
bool BtnGet_Left();
bool BtnGet_Right();

bool HW_Charge_Detect();

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
uint32 Sensor_Acquire( uint32 mask );
// check the sensor state - returning a mask with the sensors which have read out value
uint32 Sensor_Is_Ready(void);
// check the sensor state - returning a mask busy sensors
uint32 Sensor_Is_Busy(void);

uint32 Sensor_Is_Failed(void);
// get the acquired value from the sensor (returns the base formatted value from a sensor - Temp: 16fp9+40*, RH: 16fp8, Press: TBD) and clears the reagy flag
uint32 Sensor_Get_Value( uint32 sensor );
// sensor submodule polling
void Sensor_Poll(bool tick_ms);

uint32 Sensor_GetPwrStatus(void);


// polled for each ms
bool Sensor_simu_poll();

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
void HW_SetRTC_NextAlarm( uint32 alarm );

uint32 HW_ADC_GetBattery(void);

void ADC_ISR_simulation(void);

void HW_EXTI_ISR( void );
uint32 HW_Sleep( enum EPowerMode mode);
uint32 HW_GetWakeUpReason(void);


void HWDBG( int val );


int HWDBG_Get_Temp();           // temperature in 16fp9 + 40*C
int HWDBG_Get_Humidity();
int HWDBG_Get_Pressure();


#define BKP_DR1     0
#define BKP_DR2     1
#define BKP_DR3     2
#define BKP_DR4     3
#define BKP_DR5     4
#define BKP_DR6     5
#define BKP_DR7     6
#define BKP_DR8     7
#define BKP_DR9     8
#define BKP_DR10    9

void BKP_WriteBackupRegister( int reg, uint16 data );
uint16 BKP_ReadBackupRegister( int reg );




#define __disable_interrupt()       do {  } while(0)
#define __enable_interrupt()        do {  } while(0)


#ifdef __cplusplus
 }
#endif


#endif // HW_STUFF_H
