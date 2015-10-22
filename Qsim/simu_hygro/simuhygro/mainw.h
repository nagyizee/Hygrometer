#ifndef MAINW_H
#define MAINW_H

#include <QMainWindow>
#include <QTimer>
#include "graph_disp.h"
#include "typedefs.h"

#define DISPSIM_MAX_W      256
#define DISPSIM_MAX_H      132


namespace Ui {
class mainw;
}

class mainw : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit mainw(QWidget *parent = 0);
    ~mainw();
    void setup_graphic( graph_disp *_graph );

private slots:
    void TimerTick();


    void on_btn_power_pressed();
    void on_btn_power_released();

    void on_btn_up_pressed();
    void on_btn_up_released();

    void on_btn_down_pressed();
    void on_btn_down_released();

    void on_btn_left_pressed();
    void on_btn_left_released();

    void on_btn_right_pressed();
    void on_btn_right_released();

    void on_btn_ok_pressed();
    void on_btn_ok_released();

    void on_btn_esc_pressed();
    void on_btn_esc_released();

    void on_num_temperature_valueChanged(double arg1);
    void on_num_humidity_valueChanged(double arg1);
    void on_num_pressure_valueChanged(double arg1);


    void on_tb_time_editingFinished();


private:
    Ui::mainw *ui;
    QTimer *ticktimer;
    graph_disp *graph;

    bool buttons[8];
    int ms_temp;
    int ms_hum;
    int ms_press;

    QGraphicsScene *scene;
    QGraphicsPixmapItem *G_item;
    uchar *gmem;                    // graphic memory for display simulator

    void dispsim_mem_clean();

    void HW_wrapper_setup( int interval );            // set up the this pointer in the hardware wrapper (simulation module)
    void HW_wrapper_update_display();


public:

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
   
    void HW_wrapper_DBG(int val);
    int HW_wrapper_get_temperature();
    int HW_wrapper_get_humidity();
    int HW_wrapper_get_pressure();

    void HW_wrapper_Beep( int op );


    void dispsim_redraw_content();
    void HW_assertion(const char *reason);
    bool HW_wrapper_getButton(int index);
    int HW_wrapper_getHit(int index);
    void HW_wrapper_set_disp_brt(int brt);
    int HW_wrapper_ADC_battery(void);


    bool btn_pressed;
    int hw_power_mode;
    int hw_disp_brt;
    int sec_ctr;

    uint32  RTCcounter;
    uint32  RTCalarm;
    enum EPowerMode PwrMode;

private:
    void Application_MainLoop( bool tick );
    void CPULoopSimulation( bool tick );
    void CPUWakeUpOnEvent( bool pwrbtn );

};

#endif // MAINW_H
