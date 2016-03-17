#include "mainw.h"
#include "ui_mainw.h"
#include "hw_stuff.h"
#include "core.h"
#include "eeprom_spi.h"
#include "graphic_lib.h"
#include "dispHAL.h"
#include "utilities.h"


#define MAX_AXIS_CHANNELS 4


mainw *pClass;
int tiv;

/// display stuff /////////////
#define MAX_LINES       2
#define MAX_COLOUMNS    16
#define EEPROM_SIZE     0x200

char line[MAX_LINES*MAX_COLOUMNS];
static bool disp_changed = false;
/////////////////////////////////

uint8 eeprom_cont[ EEPROM_SIZE ];

uint8 *dispmem = NULL;

void InitHW(void)
{
    pClass->RTCalarm = pClass->RTCcounter+1;
}


void HW_ASSERT(const char *reason)
{
    pClass->HW_assertion(reason);
}

bool BtnGet_OK()
{
    return pClass->HW_wrapper_getButton(BTN_OK);
}

bool BtnGet_Esc()
{
    return pClass->HW_wrapper_getButton(BTN_ESC);
}

bool BtnGet_Mode()
{
    return pClass->HW_wrapper_getButton(BTN_MODE);
}

bool BtnGet_Up()
{
    return pClass->HW_wrapper_getButton(BTN_UP);
}

bool BtnGet_Down()
{
    return pClass->HW_wrapper_getButton(BTN_DOWN);
}

bool BtnGet_Left()
{
    return pClass->HW_wrapper_getButton(BTN_LEFT);
}

bool BtnGet_Right()
{
    return pClass->HW_wrapper_getButton(BTN_RIGHT);
}

bool BtnPollStick()
{

}

void HW_LED_On()
{

}


int encoder_dir = 0;



uint32 DispHAL_Init(uint8 *Gmem)
{
    dispmem = Gmem;
}


uint32 DispHAL_UpdateLUT( LUTelem *LUT )
{
    return 0;
}


void DispHAL_NeedUpdate(  uint32 X1, uint32 Y1, uint32 X2, uint32 Y2  )
{
}


int saved_secCtr;

void HW_Seconds_Start(void)
{
    saved_secCtr = pClass->sec_ctr;
    pClass->sec_ctr = 0;
}

void HW_Seconds_Restore(void)
{
    pClass->sec_ctr = saved_secCtr;
}


void HW_Buzzer_On(int pulse)
{
    if ( pulse < 800 )
        pClass->HW_wrapper_Beep(2);
    else
        pClass->HW_wrapper_Beep(1);
}

void HW_Buzzer_Off(void)
{
    pClass->HW_wrapper_Beep(0);
}

bool HW_Sleep( enum EPowerMode mode)
{
    pClass->PwrMode = mode;
    pClass->PwrModeToDisp = mode;
    return false;
}

uint32 RTC_GetCounter(void)
{
    return pClass->RTCcounter;
}

void RTC_SetAlarm(uint32 value)
{
    pClass->RTCalarm = value;
}

void HW_SetRTC(uint32 RTCctr)
{
    pClass->RTCcounter = RTCctr;
}

uint32 HW_ADC_GetBattery(void)
{
    return pClass->HW_wrapper_ADC_battery();
}


void HW_PWR_An_On( void )
{

}

void HW_PWR_An_Off( void )
{

}

void HWDBG( int val )
{
    pClass->HW_wrapper_DBG( val );
}

int HWDBG_Get_Temp()
{
    return pClass->HW_wrapper_get_temperature();
}

int HWDBG_Get_Humidity()
{
    return pClass->HW_wrapper_get_humidity();
}

int HWDBG_Get_Pressure()
{
    return pClass->HW_wrapper_get_pressure();
}



///////////////////////////////////////////////////

void mainw::HW_assertion(const char *reason)
{
    // ui->cb_syserror->setChecked(true);
    // TODO
}

bool mainw::HW_wrapper_getButton(int index)
{
    return buttons[index];
}


void mainw::HW_wrapper_set_disp_brt(int brt)
{
    ui->num_disp_brightness->setValue( brt );
    if ( brt == 255 )
        hw_disp_brt = 0;
    else if ( brt > 0x40 )
        hw_disp_brt = 0x40;
    else if ( brt == 0 )
        hw_disp_brt = 0x01;
    else
        hw_disp_brt = brt;
    dispsim_redraw_content();
}

int mainw::HW_wrapper_ADC_battery(void)
{
    return ui->sld_battery->value();
}


//////////////////////////////////////////////////

void mainw::HW_wrapper_setup( int interval )
{
    pClass = this;
    tiv    = interval;

}


void mainw::HW_wrapper_update_display()
{
    static uint32 OldRTC = 0;
    static uint32 OldAlarm = 0;

    if ( disp_changed )
    {
        disp_changed = false;
    }

    if ( RTCcounter != OldRTC )
    {
        char timedisp[256];
        OldRTC = RTCcounter;
        uint8 mounth, day, hour, minute, second;
        uint16 year;

        utils_convert_counter_2_hms( RTCcounter, &hour, &minute, &second );
        utils_convert_counter_2_ymd( RTCcounter, &year, &mounth, &day );

        sprintf( timedisp, "%04d-%02d-%02d %02d:%02d:%02d.%c [0x%08X]",
                 year, mounth, day,
                 hour, minute, second,
                 (RTCcounter & 0x01) ? '5' : '0',               // 1/2 second
                 RTCcounter                      );
        ui->tb_time->setText(tr(timedisp));

    }
    if ( RTCalarm != OldAlarm )
    {
        char timedisp[256];
        OldAlarm= RTCalarm;
        uint8 mounth, day, hour, minute, second;
        uint16 year;

        utils_convert_counter_2_hms( RTCalarm, &hour, &minute, &second );
        utils_convert_counter_2_ymd( RTCalarm, &year, &mounth, &day );

        sprintf( timedisp, "%04d-%02d-%02d %02d:%02d:%02d.%c [0x%08X]",
                 year, mounth, day,
                 hour, minute, second,
                 (RTCalarm & 0x01) ? '5' : '0',               // 1/2 second
                 RTCalarm                      );
        ui->tb_alarm->setText(tr(timedisp));
    }
}


// custom debug interface
void mainw::HW_wrapper_DBG( int val )
{
    ui->num_dbg->setValue( val );

}

int mainw::HW_wrapper_get_temperature()
{
    double val =ui->num_temperature->value();

    if ( val > 87.99 )
        return 0xffff;
    if ( val < -39.99 )
        return 0;

    return (int)( (val + 40) * (1 << TEMP_FP) );
}

int mainw::HW_wrapper_get_humidity()
{
    double val =ui->num_humidity->value();
    return (int)(val * (1<<RH_FP));
}

int mainw::HW_wrapper_get_pressure()
{
    return ms_press;
}

void mainw::HW_wrapper_Beep( int op )
{
    switch (op)
    {
        case 0:
            ui->cb_beepHi->setChecked(false);
            ui->cb_beepLo->setChecked(false);
            break;
        case 1:
            ui->cb_beepLo->setChecked(true);
            break;
        case 2:
            ui->cb_beepHi->setChecked(true);
            break;
    }
}


void mainw::on_tb_time_editingFinished()
{

    char dateline[256];
    char *pchr;
    datestruct sdate;
    timestruct stime;
    uint32 counter;

    int y, m, d, h, mn, s;

    strcpy( dateline, ui->tb_time->text().toLatin1() );

    pchr = strtok( dateline, "- :" );
    sscanf( pchr, "%d", &y );

    pchr = strtok( NULL, "- :" );
    if ( pchr == NULL )         return;
    sscanf( pchr, "%d", &m );

    pchr = strtok( NULL, "- :" );
    if ( pchr == NULL )         return;
    sscanf( pchr, "%d", &d );

    pchr = strtok( NULL, "- :" );
    if ( pchr == NULL )         return;
    sscanf( pchr, "%d", &h );

    pchr = strtok( NULL, "- :" );
    if ( pchr == NULL )         return;
    sscanf( pchr, "%d", &mn );

    pchr = strtok( NULL, "- :" );
    if ( pchr == NULL )         return;
    sscanf( pchr, "%d", &s );

    sdate.year = (uint16)y;
    sdate.mounth = (uint8)m;
    sdate.day = (uint8)d;
    stime.hour = (uint8)h;
    stime.minute = (uint8)mn;
    stime.second = (uint8)s;

    counter = utils_convert_date_2_counter( &sdate, &stime );
    core_set_clock_counter( counter );
    HW_wrapper_update_display();
}


/////////////////////////////////////////////////////
// Sensor emulation
/////////////////////////////////////////////////////

struct 
{
    int time_ctr_RH;            // result lag simulation for the Temp/RH sensor
    int time_ctr_Press;         // result lag for Pressure sensor
    bool RH_up;                 // if sensor is shut down - additional time is considered
    bool Press_up;              //

    uint32 in_progress;         // measurement in progress
    uint32 ready;               // measurement is ready

} sens;



void Sensor_Init()
{
    memset( &sens, 0, sizeof(sens));
}

void Sensor_Shutdown( uint32 mask )
{
    if ( (mask & ( SENSOR_TEMP | SENSOR_RH )) == ( SENSOR_TEMP | SENSOR_RH ) )
    {
        sens.ready &= ~SENSOR_TEMP;
        sens.ready &= ~SENSOR_RH;
        sens.RH_up = false;
    }
    if ( mask & SENSOR_PRESS )
    {
        sens.ready &= ~SENSOR_PRESS;
        sens.Press_up = false;
    }
}

void Sensor_Acquire( uint32 mask )
{
    if ( (mask & SENSOR_TEMP) && ((sens.in_progress & SENSOR_TEMP) == 0) )
    {
        if ( sens.RH_up == false )
            sens.time_ctr_RH = 15 + 70;      // 15ms start-up + 70ms read time
        else
            sens.time_ctr_RH = 70;           // 70ms read time
        sens.RH_up = true;
        sens.in_progress |= SENSOR_TEMP;
        sens.ready &= ~SENSOR_TEMP;
    }
    if ( (mask & SENSOR_RH) && ((sens.in_progress & SENSOR_RH) == 0) )
    {
        if ( sens.RH_up == false )
            sens.time_ctr_RH = 15 + 25;      // 15ms start-up + 25ms read time
        else
            sens.time_ctr_RH = 25;           // 70ms read time
        sens.RH_up = true;
        sens.in_progress |= SENSOR_RH;
        sens.ready &= ~SENSOR_RH;
    }
    if ( (mask & SENSOR_PRESS) && ((sens.in_progress & SENSOR_PRESS) == 0) )
    {
        if ( sens.Press_up == false )
            sens.time_ctr_Press = 50 + 80;       // 50ms start-up + 80ms read time
        else
            sens.time_ctr_Press = 80;            // 40ms read time for 8x oversample
        sens.Press_up = true;
        sens.in_progress |= SENSOR_PRESS;
        sens.ready &= ~SENSOR_PRESS;
    }
}

uint32 Sensor_Is_Ready(void)
{
    return sens.ready;
}

uint32 Sensor_Is_Busy(void)
{
    return sens.in_progress;
}

uint32 Sensor_Get_Value( uint32 sensor )
{
    switch ( sensor )
    {
        case SENSOR_TEMP:
            if ( (sens.ready & SENSOR_TEMP) == 0 )
                return SENSOR_VALUE_FAIL;
            sens.ready &= ~SENSOR_TEMP;
            return pClass->HW_wrapper_get_temperature();
        case SENSOR_PRESS:
            if ( (sens.ready & SENSOR_PRESS) == 0 )
                return SENSOR_VALUE_FAIL;
            sens.ready &= ~SENSOR_PRESS;
            return pClass->HW_wrapper_get_pressure();
        case SENSOR_RH:
            if ( (sens.ready & SENSOR_RH) == 0 )
                return SENSOR_VALUE_FAIL;
            sens.ready &= ~SENSOR_RH;
            return pClass->HW_wrapper_get_humidity();
        default:
            return SENSOR_VALUE_FAIL;
    }
    return SENSOR_VALUE_FAIL;
}

void Sensor_simu_poll()
{
    if ( sens.in_progress & SENSOR_TEMP )
    {
        if ( sens.time_ctr_RH == 0 )
        {
            sens.in_progress &= ~SENSOR_TEMP;
            sens.ready |= SENSOR_TEMP;
        }
        else
            sens.time_ctr_RH--;
    }
    if ( sens.in_progress & SENSOR_RH )
    {
        if ( sens.time_ctr_RH == 0 )
        {
            sens.in_progress &= ~SENSOR_RH;
            sens.ready |= SENSOR_RH;
        }
        else
            sens.time_ctr_RH--;
    }
    if ( sens.in_progress & SENSOR_PRESS )
    {
        if ( sens.time_ctr_Press == 0 )
        {
            sens.in_progress &= ~SENSOR_PRESS;
            sens.ready |= SENSOR_PRESS;
        }
        else
            sens.time_ctr_Press--;
    }
}


/////////////////////////////////////////////////////
// EEPROM emulation
/////////////////////////////////////////////////////

// init the eeprom module
bool ee_enabled = false;
bool ee_wren = false;

uint32 eeprom_init()
{
    FILE *eefile;

    eefile = fopen( "eeprom.dat", "rb" );
    if ( eefile == NULL )
    {
        eefile = fopen( "eeprom.dat", "wb" );
        if ( eefile == NULL )
            return (uint32)-1;
        memset( eeprom_cont, 0xff, EEPROM_SIZE );
        fwrite( eeprom_cont, 1, EEPROM_SIZE, eefile );
    }
    else
    {
        fread( eeprom_cont, 1, EEPROM_SIZE, eefile );
    }
    fclose( eefile );
    return 0;
}

// gets eeprom dimension
uint32 eeprom_get_size()
{
    return EEPROM_SIZE;
}

// enable eeprom in read only mode or in read/write mode
uint32 eeprom_enable( bool write )
{
    ee_enabled = true;
    ee_wren = write;
    return 0;
}

// disable eeprom module
uint32 eeprom_disable()
{
    ee_enabled = true;
    return 0;
}

// read count quantity of data from address in buff, returns the nr. of successfull read bytes
uint32 eeprom_read( uint32 address, int count, uint8 *buff )
{
    if (ee_enabled == false)
        return (uint32)-1;
    if ( count > (EEPROM_SIZE - address) )
        count = (EEPROM_SIZE - address);

    memcpy( buff, eeprom_cont + address, count );

    return count;
}

// write count quantity of data to address from buff, returns the nr. of successfull written bytes
uint32 eeprom_write( uint32 address, const uint8 *buff, int count )
{
    FILE *eefile;
    if ( (ee_enabled == false) || (ee_wren == false) )
        return (uint32)-1;

    if ( count > (EEPROM_SIZE - address) )
        count = (EEPROM_SIZE - address);

    memcpy( eeprom_cont + address, buff, count );

    eefile = fopen( "eeprom.dat", "wb" );
    if ( eefile == NULL )
        return (uint32)-1;
    fwrite( eeprom_cont, 1, EEPROM_SIZE, eefile );
    fclose(eefile);
    return count;
}




/////////////////////////////////////////////////////
// Display simulation
/////////////////////////////////////////////////////
int brt = 0x80;
bool dispon = false;

bool DispHAL_App_Poll(void)
{
    return false;
}

void DispHAL_ISR_Poll()
{
}

int  DispHAL_Display_On( void )
{
    dispon = true;
    pClass->HW_wrapper_set_disp_brt(brt);
    return 0;
}

int  DispHAL_Display_Off( void )
{
    dispon = false;
    pClass->HW_wrapper_set_disp_brt(255);
    return 0;
}

void DispHAL_SetContrast( int cval )
{
    brt = cval;
    if ( dispon )
        pClass->HW_wrapper_set_disp_brt(brt);
    else
        pClass->HW_wrapper_set_disp_brt(255);
}

bool DispHAL_DisplayBusy( void )
{
    return false;
}

bool DispHAL_ReleaseSPI( void )
{
    return true;
}


void DispHAL_UpdateScreen()
{
    pClass->dispsim_redraw_content();
}


void mainw::dispsim_mem_clean()
{
    if ( dispmem == NULL )
        return;
    memset( dispmem, 0, 1024 );
}

int pixh_r;
int pixh_g;
int pixh_b;
int pixl_r;
int pixl_g;
int pixl_b;


static void internal_dispsim_putpixel( uchar *gmem, int x, int y, bool color )
{
    int poz;

    uchar r = pixh_r;
    uchar g = pixh_g;
    uchar b = pixh_b;

    if (y>=16)
    {
        r = pixl_r;
        g = pixl_g;
        b = pixl_b;
        y+=2;
    }

    poz = x*2*3 + y*DISPSIM_MAX_W*3*2;

    if (color)
    {
        gmem[poz]   = r;
        gmem[poz+1] = g;
        gmem[poz+2] = b;
        gmem[poz+3] = r;
        gmem[poz+4] = g;
        gmem[poz+5] = b;
        poz += DISPSIM_MAX_W*3;
        gmem[poz]   = r;
        gmem[poz+1] = g;
        gmem[poz+2] = b;
        gmem[poz+3] = r;
        gmem[poz+4] = g;
        gmem[poz+5] = b;
    }
}


void mainw::dispsim_redraw_content()
{
    int x,y;

    if ( dispmem == NULL )
        return;

    memset( gmem, 0, DISPSIM_MAX_W * DISPSIM_MAX_H *3 );


    pixh_r = 0xff * hw_disp_brt / 0x40;
    pixh_g = 0xe0 * hw_disp_brt / 0x40;
    pixh_b = 0x10 * hw_disp_brt / 0x40;
    pixl_r = 0x10 * hw_disp_brt / 0x40;
    pixl_g = 0x80 * hw_disp_brt / 0x40;
    pixl_b = 0xff * hw_disp_brt / 0x40;

    for (x=0; x<GDISP_WIDTH; x++)
    {
        for (y=0; y<GDISP_MAX_MEM_H; y++)
        {
            internal_dispsim_putpixel( gmem, x, y*8+0, dispmem[x+y*GDISP_WIDTH] & 0x01 );
            internal_dispsim_putpixel( gmem, x, y*8+1, dispmem[x+y*GDISP_WIDTH] & 0x02 );
            internal_dispsim_putpixel( gmem, x, y*8+2, dispmem[x+y*GDISP_WIDTH] & 0x04 );
            internal_dispsim_putpixel( gmem, x, y*8+3, dispmem[x+y*GDISP_WIDTH] & 0x08 );
            internal_dispsim_putpixel( gmem, x, y*8+4, dispmem[x+y*GDISP_WIDTH] & 0x10 );
            internal_dispsim_putpixel( gmem, x, y*8+5, dispmem[x+y*GDISP_WIDTH] & 0x20 );
            internal_dispsim_putpixel( gmem, x, y*8+6, dispmem[x+y*GDISP_WIDTH] & 0x40 );
            internal_dispsim_putpixel( gmem, x, y*8+7, dispmem[x+y*GDISP_WIDTH] & 0x80 );
        }
    }

    QImage image( gmem, DISPSIM_MAX_W, DISPSIM_MAX_H, DISPSIM_MAX_W * 3, QImage::Format_RGB888 );

    scene->removeItem(G_item);
    delete G_item;
    G_item  = new QGraphicsPixmapItem( QPixmap::fromImage( image ));
    scene->addItem(G_item);
}

