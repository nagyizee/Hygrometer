
#include <QTextBlock>
#include "string.h"
#include "mainw.h"
#include "ui_mainw.h"
#include "hw_stuff.h"
#include "utilities.h"
#include "events_ui.h"


#define TIMER_INTERVAL          20       // 20ms
#define TICKS_TO_SIMULATE       30       // to obtain 0.5sec simulated time ( 500cycles of 1ms timer )


mainw::mainw(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::mainw)
{
    ui->setupUi(this);

    // init stuff
    btn_pressed = false;
    hw_disp_brt = 0x40;
    sec_ctr = 0;
    memset( buttons, 0, sizeof(bool)*8 );

    // set up the graphic display simulator
    gmem    = (uchar*)malloc( DISPSIM_MAX_W * DISPSIM_MAX_H * 3 );
    scene   = new QGraphicsScene( this );
    QImage image( gmem, DISPSIM_MAX_W, DISPSIM_MAX_H, DISPSIM_MAX_W * 3, QImage::Format_RGB888 );
    image.fill( Qt::black );
    G_item  = new QGraphicsPixmapItem( QPixmap::fromImage( image ));
    scene->addItem(G_item);
    ui->grf_display->setScene(scene);

    dispsim_mem_clean();
    dispsim_redraw_content();

    ticktimer = new QTimer( this );
    connect(ticktimer, SIGNAL(timeout()), this, SLOT(TimerTick()));
    ticktimer->start( TIMER_INTERVAL );
    HW_wrapper_setup( TIMER_INTERVAL );

    ms_temp = 23.54;
    ms_hum = 56.7;
    ms_press = 1013.25;

    ui->num_temperature->setValue( ms_temp );
    ui->num_humidity->setValue( ms_hum );
    ui->num_pressure->setValue( ms_press );

    datestruct date;
    timestruct time;
    date.day = 15;
    date.mounth = 3;
    date.year = 2015;
    time.hour = 23;
    time.minute = 18;
    time.second = 22;
    RTCcounter = utils_convert_date_2_counter( &date, &time );
    RTCalarm = RTCcounter+1;
    PwrMode = pm_full;

    //--- test phase
    qsrand(0x64892354);
    main_entry( NULL );
}

mainw::~mainw()
{
    delete ui;
}


void mainw::setup_graphic( graph_disp *_graph )
{
    graph   = _graph;
}


/////////////////////////////////////////////////////////////
//  Main app. simulation
/////////////////////////////////////////////////////////////
struct SEventStruct ev;

void mainw::Application_MainLoop( bool tick )
{
    main_loop();
}

void mainw::TimerTick()
{
    CPULoopSimulation( true );
}

void mainw::CPULoopSimulation( bool tick )
{
    int simuspeed[] = { TICKS_TO_SIMULATE / 10, TICKS_TO_SIMULATE / 8, TICKS_TO_SIMULATE / 6, TICKS_TO_SIMULATE / 4, TICKS_TO_SIMULATE / 2,
                                                            TICKS_TO_SIMULATE,
                        TICKS_TO_SIMULATE * 3 / 2, TICKS_TO_SIMULATE * 5 / 2, TICKS_TO_SIMULATE *3, TICKS_TO_SIMULATE * 5, TICKS_TO_SIMULATE * 10 };
    int i;
    int j;
    int tosim;

    if ( ui->cb_stop_time->isChecked() )
        return;

    // simulation of power modes:
    //   operations:     rnd mloop      1 mloop     timerISR    senspoll    rtc_Handle  CheckAllbtns   Continue
    // - pm_full            y               n           y           y           y            n            y
    // - pm_sleep           n               y           y           y           y            n            y
    // - pm_hold_btn        n               n           n           n           y            y            y
    // - pm_hold            n               n           n           n           y            n            y
    // - pm_down            n               n           n           n           n            n            n

    ui->num_pwr_mng->setValue( PwrMode );

    tosim = simuspeed[ui->sl_clock_simu->value()];

    for (i=0; i<(tick ? tosim : 1); i++)            // maintain this for loop for timing consistency
    {
        // process application loop
        if ( (PwrMode == pm_sleep) || (PwrMode == pm_full) )
        {
            // simulated code section ----
            bool    send_tick   = tick;
            int     loop;

            if ( PwrMode == pm_full )
                loop = (qrand() & 0x0f) + 1; // it can make 1 - 16 main loops
            else
                loop = 1;

            for ( j=0; j<loop; j++ )
            {
                Application_MainLoop( send_tick );
                send_tick = false;
            }
        }

        // process clocks
        if ( tick )
        {
            sec_ctr++;
            // increment RTC and check for alarms
            if ( sec_ctr == 500 )
            {
                sec_ctr = 0;
                RTCcounter++;
                if ( RTCcounter == RTCalarm )
                {
                    if ( PwrMode == pm_down )       // if power down - means that electonics is just waking up - start it all from beginning
                        main_entry( NULL );
                    else                            // otherwise interrupt will be generated from stopped/sleeping/full status
                        TimerRTCIntrHandler();
                    PwrMode = pm_full;
                }
            }
            // process system timer
            if ( (PwrMode == pm_sleep) || (PwrMode == pm_full) )
            {
                TimerSysIntrHandler();    // timer interval - 1ms
                Sensor_simu_poll();
                PwrMode = pm_full;
            }
        }
    }

    if ( PwrMode == pm_close )
    {
        this->close();
    }

    HW_wrapper_update_display();

}

void mainw::CPUWakeUpOnEvent( bool pwrbtn )
{
    if ( PwrMode == pm_down )               // if no power 
    {
        if ( pwrbtn )                       // and power button pressed - wake it up and start from reset
        {
            PwrMode = pm_full;
            main_entry( NULL );
            CPULoopSimulation( false );
        }
    }
    else if ( ((PwrMode == pm_hold) && pwrbtn) ||       // if stopped with UI turned off - user should wake it up by power button only
              (PwrMode == pm_hold_btn)           )      // if stopped with UI on ( no power_off action or power down timeout ) - any button will wake up
    {
        PwrMode = pm_full;
        CPULoopSimulation( false );
    }

    // Sleep and Full modes are not threated here - those are sysTimer powered
}

/////////////////////////////////////////////////////////////
// UI elements
/////////////////////////////////////////////////////////////

void mainw::on_btn_power_pressed()  { buttons[BTN_MODE] = true; CPUWakeUpOnEvent(true); }
void mainw::on_btn_power_released() { buttons[BTN_MODE] = false; }

void mainw::on_btn_up_pressed()  { buttons[BTN_UP] = true; CPUWakeUpOnEvent(false); }
void mainw::on_btn_up_released() { buttons[BTN_UP] = false; }

void mainw::on_btn_down_pressed()  { buttons[BTN_DOWN] = true; CPUWakeUpOnEvent(false);}
void mainw::on_btn_down_released() { buttons[BTN_DOWN] = false; }

void mainw::on_btn_left_pressed()  { buttons[BTN_LEFT] = true; CPUWakeUpOnEvent(false);}
void mainw::on_btn_left_released() { buttons[BTN_LEFT] = false; }

void mainw::on_btn_right_pressed()  { buttons[BTN_RIGHT] = true; CPUWakeUpOnEvent(false); }
void mainw::on_btn_right_released() { buttons[BTN_RIGHT] = false; }

void mainw::on_btn_ok_pressed()  { buttons[BTN_OK] = true; CPUWakeUpOnEvent(false); }
void mainw::on_btn_ok_released() { buttons[BTN_OK] = false; }

void mainw::on_btn_esc_pressed()  { buttons[BTN_ESC] = true; CPUWakeUpOnEvent(false); }
void mainw::on_btn_esc_released() { buttons[BTN_ESC] = false; }

void mainw::on_num_temperature_valueChanged(double arg1) { ms_temp = (arg1 * 100); }
void mainw::on_num_humidity_valueChanged(double arg1) { ms_hum = (arg1 * 100); }
void mainw::on_num_pressure_valueChanged(double arg1) { ms_press = (arg1 * 100); }
