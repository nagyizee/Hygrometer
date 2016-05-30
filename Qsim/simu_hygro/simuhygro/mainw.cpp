
#include <QTextBlock>
#include "string.h"
#include "mainw.h"
#include "ui_mainw.h"
#include "hw_stuff.h"
#include "utilities.h"
#include "events_ui.h"
#include "com_link.h"


#define TIMER_INTERVAL          20       // 20ms
#define TICKS_TO_SIMULATE       30       // to obtain 0.5sec simulated time ( 500cycles of 1ms timer )


static bool internal_is_mounth_over( datestruct date )
{
    uint32 last_day = 0;
    switch ( date.mounth )
    {
        case 1: last_day = 31; break;
        case 2:
            if ( (date.year % 4) == 0 ) // leap year
                last_day = 29;
            else
                last_day = 28;
            break;
        case 3: last_day = 31; break;
        case 4: last_day = 30; break;
        case 5: last_day = 31; break;
        case 6: last_day = 30; break;
        case 7: last_day = 31; break;
        case 8: last_day = 31; break;
        case 9: last_day = 30; break;
        case 10: last_day = 31; break;
        case 11: last_day = 30; break;
        case 12: last_day = 31; break;
    }

    if ( date.day > last_day )
        return true;
    return false;
}

void test_date(void)
{
    // unit test for date converter
    uint32 date_ctr;
    uint8 mounth, day, hour, minute, second;
    uint16 year;
    datestruct date = {0, };
    timestruct time = {0, };
    uint32 dayofweek = WEEK_START;
    uint32 weekofyear = 0;

    datestruct c_date = {0, };
    timestruct c_time = {0, };

    c_date.day = 1;
    c_date.mounth = 1;
    c_date.year = 2013;

    for ( date_ctr=0; date_ctr<=0xffffffff; date_ctr++ )
    {
        if ( date_ctr == 0x0f099c00 )       // value which creates problems
        {
            printf("breakpoint here\n");
        }

        utils_convert_counter_2_hms( date_ctr, &hour, &minute, &second );
        utils_convert_counter_2_ymd( date_ctr, &year, &mounth, &day );
        
        date.day = day;
        date.mounth = mounth;
        date.year = year;
        time.hour = hour;
        time.minute = minute;
        time.second = second;

        // calculate the local
        if ( ((date_ctr & 0x01) == 0) && date_ctr )
        {
            c_time.second++;
            if ( c_time.second == 60 )
            {
                c_time.second = 0;
                c_time.minute++;
                if ( c_time.minute == 60 )
                {
                    c_time.minute = 0;
                    c_time.hour++;
                    if ( c_time.hour == 24 )
                    {
                        c_time.hour = 0;
                        dayofweek++;
                        if ( (dayofweek % 7) == 0 )
                        {
                            weekofyear++;
                        }
                        dayofweek %= 7;

                        c_date.day++;
                        if ( internal_is_mounth_over( c_date ) )
                        {
                            c_date.day = 1;
                            c_date.mounth++;
                            if ( c_date.mounth == 13 )
                            {
                                c_date.mounth = 1;
                                weekofyear = 0;
                                c_date.year++;
                            }
                        }
                    }
                }
            }
        }

        if ( (c_time.hour != time.hour) ||
             (c_time.minute != time.minute) ||
             (c_time.second != time.second) ||
             (c_date.day != date.day) ||
             (c_date.mounth != date.mounth) ||
             (c_date.year != date.year)   )
        {
            printf("Consistency fail at %d", date_ctr );
        }

        if ( (date_ctr & 0xfffffffe) != utils_convert_date_2_counter( &date, &time ) )
        {
            printf("Error at %d", date_ctr );
        }

        if ( dayofweek != utils_day_of_week(date_ctr) )
        {
            printf("Error at %d", date_ctr );
        }

        if ( weekofyear != utils_week_of_year(date_ctr) )
        {
            printf("Error at %d", date_ctr );
        }
    }
}



mainw::mainw(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::mainw)
{
    ui->setupUi(this);

    // init stuff
    btn_pressed = false;
    hw_disp_brt = 0x40;
    sec_ctr = 0;
    dump_thread = false;
    comlink  = new com_link();

    memset( buttons, 0, sizeof(bool)*8 );

    // set up the graphic display simulator
    gmem    = (uchar*)malloc( DISPSIM_MAX_W * DISPSIM_MAX_H * 3 );
    scene   = new QGraphicsScene( this );
    QImage image( gmem, DISPSIM_MAX_W, DISPSIM_MAX_H, DISPSIM_MAX_W * 3, QImage::Format_RGB888 );
    image.fill( Qt::black );
    G_item  = new QGraphicsPixmapItem( QPixmap::fromImage( image ));
    scene->addItem(G_item);
    ui->grf_display->setScene(scene);

    // set up power management display
    //pwr_colors = new QVector<QRgb>();
    pwr_colors.append( 0xff000000 );       // cm_backgnd
    pwr_colors.append( 0xff201812 );       // cm_grid_dark
    pwr_colors.append( 0xff403025 );       // cm_grid_bright
    pwr_colors.append( 0xfffce000 );       // cm_pwr_full
    pwr_colors.append( 0xff807010 );       // cm_pwr_sleep
    pwr_colors.append( 0xffA09010 );       // cm_pwr_slpdisp
    pwr_colors.append( 0xffA01010 );       // cm_pwr_hold_btn
    pwr_colors.append( 0xffA010f0 );       // cm_pwr_hold
    pwr_colors.append( 0xff000000 );       // cm_pwr_down
    pwr_colors.append( 0xffffffff );       // cm_pwr_exti

    pwr_colors.append( 0xf0fce000 );       // cm_pwr_br_full
    pwr_colors.append( 0xf0807010 );       // cm_pwr_br_sleep
    pwr_colors.append( 0xf0A09010 );       // cm_pwr_br_slpdisp
    pwr_colors.append( 0xf0A01010 );       // cm_pwr_br_hold_btn
    pwr_colors.append( 0xf0A010f0 );       // cm_pwr_br_hold

    pwr_colors.append( 0xe0fce000 );       // cm_pwr_drk_full
    pwr_colors.append( 0xe0807010 );       // cm_pwr_drk_sleep
    pwr_colors.append( 0xe0A09010 );       // cm_pwr_drk_slpdisp
    pwr_colors.append( 0xe0A01010 );       // cm_pwr_drk_hold_btn
    pwr_colors.append( 0xe0A010f0 );       // cm_pwr_drk_hold


    pwr_gmem = (uchar*)malloc( DISPPWR_MAX_W * DISPPWR_MAX_H);
    pwr_scene   = new QGraphicsScene( this );
    QImage image2( pwr_gmem, DISPPWR_MAX_W, DISPPWR_MAX_H, DISPPWR_MAX_W, QImage::Format_Indexed8 );
    image2.setColorTable(pwr_colors);
    image2.fill( Qt::black );
    pwr_G_item  = new QGraphicsPixmapItem( QPixmap::fromImage( image2 ));
    pwr_scene->addItem(pwr_G_item);
    ui->grf_power->setScene(pwr_scene);
    pwr_ptr = 0;
    pwr_xptr = 0;

    dispsim_mem_clean();
    dispsim_redraw_content();

    disppwr_mem_clean();
    disppwr_redraw_content();

    ticktimer = new QTimer( this );
    connect(ticktimer, SIGNAL(timeout()), this, SLOT(TimerTick()));
    ticktimer->start( TIMER_INTERVAL );
    HW_wrapper_setup( TIMER_INTERVAL );

    ms_temp = 23.54;
    ms_hum = 56.7;
    ms_press = 1013.25;

    dbg_shed_sens_temp = 1;
    dbg_shed_sens_rh = 1;
    dbg_shed_sens_press = 1;

    ui->num_temperature->setValue( ms_temp );
    ui->num_humidity->setValue( ms_hum );
    ui->num_pressure->setValue( ms_press );

/*    datestruct date;
    timestruct time;
    date.day = 15;
    date.mounth = 3;
    date.year = 2015;
    time.hour = 23;
    time.minute = 18;
    time.second = 22;
    RTCcounter = utils_convert_date_2_counter( &date, &time );
    */
    RTCcounter = 0x00;
    RTCalarm = 0xffffffff;
    PwrMode = pm_down;
    PwrModeToDisp = pm_down;
    PwrWUR = WUR_FIRST;
    PwrDispUd = 0;

    //--- test phase
    qsrand(0x64892354);


//    test_date();
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

double SIMUVAL_DIFF[]  = { 20.0, 60.0, 90.0  };     // differences over the floor values
double SIMUVAL_FLOOR[] = { 12.0, 30.0, 950.0 };     // floor values

int    SIMUIV_MIN[]     = { 1000, 1000, 5000 };         // minimum intervals for value change
int    SIMUIV_STRETCH[] = { 600000, 600000, 600000, };  // maximum strech bw. value changes

void mainw::InputValSimulation(void)
{
    // executed at system tick (1ms)
    int i;
    double diff;
    if ( inval.initted == false )
    {
        inval.sens[0] = ui->num_temperature->value();
        inval.sens[1] = ui->num_humidity->value();
        inval.sens[2] = ui->num_pressure->value();

        for (i=0; i<3; i++)
        {
            inval.steps_todo[i] = 0;
        }

        inval.initted = true;
    }

    for (i=0; i<3; i++)
    {
        if ( inval.steps_todo[i] == 0 )
        {
            // start of a new value set
            int steps;

            diff = ( qrand() * SIMUVAL_DIFF[i] ) / RAND_MAX + SIMUVAL_FLOOR[i];
            steps = ( (uint64)qrand() * SIMUIV_STRETCH[i] ) / RAND_MAX + SIMUIV_MIN[i];

            inval.steps_todo[i] = steps;
            inval.sdiff[i] = (diff - inval.sens[i]) / steps;
        }
        else
        {
            inval.sens[i] += inval.sdiff[i];
            inval.steps_todo[i]--;
        }
    }
}


struct SEventStruct ev;

void mainw::Application_MainLoop( bool tick )
{
    main_loop();
}

void mainw::TimerTick()
{
    CPULoopSimulation( true );
}


bool pwr_main_executed = false;
bool pwr_exti = false;

void mainw::CPULoopSimulation( bool tick )
{
    int simuspeed[] = { TICKS_TO_SIMULATE / 10, TICKS_TO_SIMULATE / 8, TICKS_TO_SIMULATE / 6, TICKS_TO_SIMULATE / 4, TICKS_TO_SIMULATE / 2,
                                                            TICKS_TO_SIMULATE,
                        TICKS_TO_SIMULATE * 3 / 2, TICKS_TO_SIMULATE * 5 / 2, TICKS_TO_SIMULATE *5, TICKS_TO_SIMULATE * 10, TICKS_TO_SIMULATE * 120 };
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

    ui->num_pwr_mng->setValue( PwrModeToDisp );

    tosim = simuspeed[ui->sl_clock_simu->value()];

    for (i=0; i<(tick ? tosim : 1); i++)            // maintain this for loop for timing consistency
    {
        if ( ui->cb_simu_val->isChecked() )
            InputValSimulation();
        else
            inval.initted = false;

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
                pwr_main_executed = true;

                if ( simu_1cycle )
                    break;

                if ( PwrMode >= pm_sleep )
                    break;                   // break the multi-main-loop if power mode changed to anything but spleep or full

                send_tick = false;
            }
        }

        // process clocks
        if ( tick && (PwrWUR != WUR_FIRST) )
        {
            sec_ctr++;

            if ( PwrDispUd )
                PwrDispUd--;

            // increment RTC and check for alarms
            if ( sec_ctr == 500 )
            {
                sec_ctr = 0;
                RTCcounter++;
                if ( RTCcounter == RTCalarm )
                {
                    if ( PwrMode == pm_down )       // if power down - means that electonics is just waking up - start it all from beginning
                    {
                        PwrWUR = WUR_RTC;
                        pwr_exti = true;
                        main_entry( NULL );
                    }
                    else                            // otherwise interrupt will be generated from stopped/sleeping/full status
                        TimerRTCIntrHandler();

                    if ( (PwrMode == pm_down) || (PwrMode == pm_hold_btn) || (PwrMode == pm_hold) )
                    {
                        pwr_exti = true;
                        PwrWUR = WUR_RTC;
                    }
                    PwrMode = pm_full;
                }
            }
            // process system timer
            if ( (PwrMode == pm_sleep) || (PwrMode == pm_full) )
            {
                TimerSysIntrHandler();    // timer interval - 1ms
                PwrMode = pm_full;
            }

            if ( PwrMode != pm_down )
            {
                if ( Sensor_simu_poll() )
                {
                    PwrWUR |= WUR_SENS_IRQ;
                    PwrMode = pm_full;
                    pwr_exti = true;
                }
            }

            if ( pwr_exti )
                pwrdisp_add_pwr_state( pm_exti );
            else
            {
                if ( (PwrModeToDisp == pm_sleep) && (PwrDispUd) )
                {
                    pwrdisp_add_pwr_state( pm_disp_update );
                }
                else if ( (PwrModeToDisp == pm_sleep) && (simu_1cycle) )
                {
                    pwrdisp_add_pwr_state( pm_full );
                }
                else
                    pwrdisp_add_pwr_state( PwrModeToDisp );
            }

            simu_1cycle = false;
            pwr_main_executed = false;
            pwr_exti = false;
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
            if ( PwrWUR != WUR_FIRST )
            {
                pwr_exti = true;
                PwrWUR = WUR_USR;
            }
            main_entry( NULL );
            CPULoopSimulation( false );
        }
    }
    else if ( ((PwrMode == pm_hold) && pwrbtn) ||       // if stopped with UI turned off - user should wake it up by power button only
              (PwrMode == pm_hold_btn)           )      // if stopped with UI on ( no power_off action or power down timeout ) - any button will wake up
    {
        PwrMode = pm_full;
        PwrWUR = WUR_USR;
        pwr_exti = true;
        CPULoopSimulation( false );
    }

    // Sleep and Full modes are not threated here - those are sysTimer powered
}


void mainw::pwrdisp_add_pwr_state( enum EPowerMode mode )
{
    int grid = 0;
    int val = 0;

    if ( pwr_ptr == DISPPWR_MAX_H )
    {
        // shift everything <----
        int y,x;
        for ( y=0; y<DISPPWR_MAX_H; y++ )
        {
            for (x=0; x<DISPPWR_MAX_W-1; x++ )
            {
                pwr_gmem[ x + y*DISPPWR_MAX_W ] = pwr_gmem[ x+1 + y*DISPPWR_MAX_W ];
            }
            pwr_gmem[ DISPPWR_MAX_W-1 + y*DISPPWR_MAX_W ] = cm_pwr_down;
        }
        pwr_xptr++;
        pwr_ptr = 0;
    }

    if ( ((pwr_xptr % 10 ) == 0) || ((pwr_ptr % 10 ) == 0) )    // if 0.5sec intervals on horizontal or 10ms on vertical
    {
        grid = 1;
        if ((pwr_xptr % 100 ) == 0)
            grid = 2;
    }

    switch (mode)
    {
        case pm_exti:
            val = cm_pwr_exti;
            break;
        case pm_full:
            if (grid == 0)
                val = cm_pwr_full;
            else if (grid == 1)
                val = cm_pwr_br_full;
            else
                val = cm_pwr_drk_full;
            break;
        case pm_sleep:
            if (grid == 0)
                val = cm_pwr_sleep;
            else if (grid == 1)
                val = cm_pwr_br_sleep;
            else
                val = cm_pwr_drk_sleep;
            break;
        case pm_disp_update:
            if (grid == 0)
                val = cm_pwr_slpdisp;
            else if (grid == 1)
                val = cm_pwr_br_slpdisp;
            else
                val = cm_pwr_drk_slpdisp;
            break;
        case pm_hold_btn:
            if (grid == 0)
                val = cm_pwr_hold_btn;
            else if (grid == 1)
                val = cm_pwr_br_hold_btn;
            else
                val = cm_pwr_drk_hold_btn;
            break;
        case pm_hold:
            if (grid == 0)
                val = cm_pwr_hold;
            else if (grid == 1)
                val = cm_pwr_br_hold;
            else
                val = cm_pwr_drk_hold;
            break;
        case pm_down:
            if ( grid )    // if 0.5sec intervals on horizontal or 10ms on vertical
            {
                if (grid == 2)
                    val = cm_grid_bright;     // 5sec intervals
                else
                    val = cm_grid_dark;       // 0.5sec intervals
            }
            else
            {
                val = cm_pwr_down;
            }
            break;
    }

    pwr_gmem[ DISPPWR_MAX_W - 1 + (DISPPWR_MAX_W*(DISPPWR_MAX_H - pwr_ptr - 1)) ] = val;

    pwr_ptr++;
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

#define MAX_BUFF_SIZE   ( 1024 * 1024 )
uint8 dumpbuffer[ MAX_BUFF_SIZE ];

void mainw::on_pb_dump_clicked()
{
    if ( ui->pb_dump->isChecked() )
    {
        if ( comlink->cmd_connect() )
            goto _error;

        if ( comlink->cmd_read_data_dump_start( dumpbuffer, MAX_BUFF_SIZE ) )
            goto _error;

        dump_thread = true;
    }
    else if (dump_thread == true)
    {
        FILE *eefile;
        int size;
        char filename[64] = "";

        comlink->cmd_read_data_stop();
        comlink->cmd_disconnect();
        dump_thread = false;

        size =comlink->cmd_read_data_check_inbuffer();

        strcpy( filename, ui->tb_dump_filename->text().toLatin1() );
        strcat( filename, ".dmp" );

        eefile = fopen( filename, "wb" );
        if ( eefile == NULL )
            goto _error;
        fwrite( dumpbuffer, 1, size, eefile );
        fclose(eefile);

        comlink->cmd_disconnect();
    }
    return;
_error:
    comlink->cmd_disconnect();
    ui->pb_dump->setChecked(false);
}



















