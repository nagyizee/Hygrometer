#ifndef MAINW_H
#define MAINW_H

#include <QMainWindow>
#include <QTimer>
#include "graph_disp.h"
#include "hw_stuff.h"
#include "typedefs.h"

#define DISPSIM_MAX_W      256
#define DISPSIM_MAX_H      132

#define DISPPWR_MAX_W      600
#define DISPPWR_MAX_H      50

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
    enum EColorMap
    {
        cm_backgnd = 0,
        cm_grid_dark,
        cm_grid_bright,
        cm_pwr_full,
        cm_pwr_sleep,
        cm_pwr_slpdisp,
        cm_pwr_hold_btn,
        cm_pwr_hold,
        cm_pwr_down,
        cm_pwr_exti,
        cm_pwr_br_full,
        cm_pwr_br_sleep,
        cm_pwr_br_slpdisp,
        cm_pwr_br_hold_btn,
        cm_pwr_br_hold,
        cm_pwr_drk_full,
        cm_pwr_drk_sleep,
        cm_pwr_drk_slpdisp,
        cm_pwr_drk_hold_btn,
        cm_pwr_drk_hold
    };

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

    QGraphicsScene *pwr_scene;
    QGraphicsPixmapItem *pwr_G_item;
    uchar *pwr_gmem;                // graphic memory for power management indicator
    QVector<QRgb> pwr_colors;

    int pwr_ptr;
    int pwr_xptr;



    void dispsim_mem_clean();
    void disppwr_mem_clean();

    void HW_wrapper_setup( int interval );            // set up the this pointer in the hardware wrapper (simulation module)
    void HW_wrapper_update_display();


public:

    void HW_wrapper_DBG(int val);
    int HW_wrapper_get_temperature();
    int HW_wrapper_get_humidity();
    int HW_wrapper_get_pressure();

    void HW_wrapper_Beep( int op );
    void HW_wrapper_show_sensor_read( uint32 sensor, bool on );

    void dispsim_redraw_content();
    void disppwr_redraw_content();

    void HW_assertion(const char *reason);
    bool HW_wrapper_getButton(int index);
    bool HW_wrapper_getChargeState(void);
    int HW_wrapper_getHit(int index);
    void HW_wrapper_set_disp_brt(int brt);
    int HW_wrapper_ADC_battery(void);


    bool btn_pressed;
    int hw_power_mode;
    int hw_disp_brt;
    int sec_ctr;

    uint32  RTCcounter;
    uint32  RTCalarm;
    uint32  RTCNextAlarm;
    enum EPowerMode PwrMode;
    enum EPowerMode PwrModeToDisp;
    uint32  PwrDispUd;
    uint32  PwrWUR;

    uint16  BKPregs[10];

private:
    void Application_MainLoop( bool tick );
    void CPULoopSimulation( bool tick );
    void CPUWakeUpOnEvent( bool pwrbtn );
    void pwrdisp_add_pwr_state( enum EPowerMode mode );
};

#endif // MAINW_H
