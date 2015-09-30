
#include <QTextBlock>
#include "string.h"
#include "mainw.h"
#include "ui_mainw.h"
#include "hw_stuff.h"
#include "events_ui.h"

#define TIMER_INTERVAL          20       // 20ms
#define TICKS_TO_SIMULATE       60      // timer tick is on 500us -> 2000 timer ticks in 1 sec.  => 40 ticks in 20ms


mainw::mainw(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::mainw)
{
    ui->setupUi(this);

    // init stuff
    btn_pressed = false;
    hw_power_mode = HWSLEEP_FULL;
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
    connect( this, SIGNAL(send_coords_to_graphic(int,int,int,int)), graph, SLOT(insert_tick(int,int,int,int)) );
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
    int i;
    int j;

    for (i=0; i<TICKS_TO_SIMULATE; i++)
    {

        // simulated code section ----
        bool    send_tick   = true;
        int     loop;

        if ( hw_power_mode == HWSLEEP_FULL )
            loop = (qrand() & 0x0f) + 1; // it can make 1 - 16 main loops
        else
            loop = 1;

        for ( j=0; j<loop; j++ )
        {
            Application_MainLoop( send_tick );
            send_tick = false;
        }


        ADC_ISR_simulation();

        TimerSysIntrHandler();    // timer interval - 500us
        sec_ctr++;
        if ( sec_ctr == 2000 )
        {
            sec_ctr = 0;
            TimerRTCIntrHandler();
        }
    }

    if ( hw_power_mode == HWSLEEP_OFF )
    {
        this->close();
    }

    HW_wrapper_update_display();
}


/////////////////////////////////////////////////////////////
// UI elements
/////////////////////////////////////////////////////////////
void mainw::on_btn_OK_pressed()
{
    buttons[0] = true;
}

void mainw::on_btn_OK_released()
{
    buttons[0] = false;
}

void mainw::on_btn_esc_pressed()
{
    buttons[1] = true;
}

void mainw::on_btn_esc_released()
{
    buttons[1] = false;
}

void mainw::on_btn_power_pressed()
{
    buttons[2] = true;
}

void mainw::on_btn_power_released()
{
    buttons[2] = false;
}

void mainw::on_btn_startstop_pressed()
{
    buttons[3] = true;
}

void mainw::on_btn_startstop_released()
{
    buttons[3] = false;
}

void mainw::on_btn_menu_pressed()
{
    buttons[4] = true;
}

void mainw::on_btn_menu_released()
{
    buttons[4] = false;
}






