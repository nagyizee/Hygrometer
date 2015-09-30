#ifndef MAINW_H
#define MAINW_H

#include <QMainWindow>
#include <QTimer>
#include "graph_disp.h"

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

    void on_btn_OK_pressed();

    void on_btn_OK_released();

    void on_btn_esc_pressed();

    void on_btn_esc_released();

    void on_btn_startstop_pressed();

    void on_btn_startstop_released();

    void on_btn_menu_pressed();

    void on_btn_menu_released();

    void on_btn_power_pressed();

    void on_btn_power_released();


signals:
    void send_coords_to_graphic( int az, int el, int tl, int au );

private:
    Ui::mainw *ui;
    QTimer *ticktimer;
    graph_disp *graph;

    bool buttons[8];

    QGraphicsScene *scene;
    QGraphicsPixmapItem *G_item;
    uchar *gmem;                    // graphic memory for display simulator

    void dispsim_mem_clean();

    void HW_wrapper_setup( int interval );            // set up the this pointer in the hardware wrapper (simulation module)
    void HW_wrapper_update_display();


public:

    void HW_wrapper_DBG(int val);
    void HW_wrapper_Beep( int op );


    void dispsim_redraw_content();
    void HW_assertion(const char *reason);
    bool HW_wrapper_getButton(int index);
    int HW_wrapper_getEncoder(void);
    int HW_wrapper_getHit(int index);
    int HW_wrapper_shutter_pref( bool state );
    int HW_wrapper_shutter_release( bool state );
    void HW_wrapper_set_disp_brt(int brt);
    int HW_wrapper_ADC_battery(void);
    int HW_wrapper_ADC_light(void);
    int HW_wrapper_ADC_sound(void);
    void HW_wrapper_ADC_On_indicator(bool value);


    bool btn_pressed;
    int hw_power_mode;
    int hw_disp_brt;
    int sec_ctr;


private:
    void Application_MainLoop( bool tick );

};

#endif // MAINW_H
