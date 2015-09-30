#include "mainw.h"
#include "ui_mainw.h"
#include "hw_stuff.h"
#include "core.h"
#include "eeprom_spi.h"
#include "graphic_lib.h"
#include "dispHAL.h"


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
}


void HW_ASSERT(const char *reason)
{
    pClass->HW_assertion(reason);
}

bool BtnGet_OK()
{
    return pClass->HW_wrapper_getButton(0);
}

bool BtnGet_Cancel()
{
    return pClass->HW_wrapper_getButton(1);
}

bool BtnGet_Power()
{
    return pClass->HW_wrapper_getButton(2);
}

bool BtnGet_StartStop()
{
    return pClass->HW_wrapper_getButton(3);
}

bool BtnGet_Menu()
{
    return pClass->HW_wrapper_getButton(4);
}

void HW_Shutter_Prefocus_ON()
{
    pClass->HW_wrapper_shutter_pref(true);
}

void HW_Shutter_Prefocus_OFF()
{
    pClass->HW_wrapper_shutter_pref(false);
}

void HW_Shutter_Release_ON()
{
    pClass->HW_wrapper_shutter_release(true);
}

void HW_Shutter_Release_OFF()
{
    pClass->HW_wrapper_shutter_release(false);
}


int encoder_dir = 0;

void HW_Encoder_Poll(void)
{
    static uint32 enc_old = 0;
    int enc;

    enc = pClass->HW_wrapper_getEncoder();

    if ( enc != enc_old )
    {
        if ( ((enc > enc_old) && ((enc - enc_old) < (ENCODER_MAX / 2) )) ||
             ((enc < enc_old) && ((enc_old - enc) > (ENCODER_MAX / 2) ))    )
             encoder_dir = 2;
        if ( ((enc > enc_old) && ((enc - enc_old) > (ENCODER_MAX / 2) )) ||
             ((enc < enc_old) && ((enc_old - enc) < (ENCODER_MAX / 2) ))    )
             encoder_dir = 1;
        enc_old = enc;  //Currently it detects the direction only. TBD: if we need a step up/down count to not to lose steps
    }
}


uint32 BtnGet_Encoder()
{
    int tmp;
    tmp = encoder_dir;
    encoder_dir = 0;
}


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

bool HW_Sleep(int mode)
{
    pClass->hw_power_mode = mode;
    disp_changed = true;
    return false;
}


static int adc_run = 0;
static int adc_group = 0;
static uint32 *adcBuff = NULL;

void ADC_ISR_simulation(void)
{
    int i;
    if (adc_run == 0)
        return;

    i = 5;
    while (i > 0)
    {
        uint32 value;
        if ( adc_group )
            value = pClass->HW_wrapper_ADC_sound();
        else
            value = pClass->HW_wrapper_ADC_light();

        adcBuff[0] = pClass->HW_wrapper_ADC_battery();
        adcBuff[1] = value;
        adcBuff[2] = value;
        CoreADC_ISR_Complete();
        i--;
    }

}



uint32 HW_ADC_GetBattery(void)
{
    return pClass->HW_wrapper_ADC_battery();
}

void HW_ADC_StartAcquisition( uint32 buffer_addr, int group )
{
    adc_run = 1;
    adc_group = group;
    adcBuff = (uint32*)buffer_addr;
    pClass->HW_wrapper_ADC_On_indicator( true );
}

void HW_ADC_StopAcquisition( void )
{
    adc_run = 0;
    pClass->HW_wrapper_ADC_On_indicator( false );
}

void HW_ADC_SetupPretrigger( uint32 value )
{
    Core_ISR_PretriggerCompleted();     // mark directly the completion in simu
}

void HW_ADC_ResetPretrigger( void )
{
    // do nothing in simu
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

int mainw::HW_wrapper_getEncoder(void)
{
    return ui->dial->value();
}

int mainw::HW_wrapper_shutter_pref( bool state )
{
    ui->cb_prefocus->setChecked(state);
}

int mainw::HW_wrapper_shutter_release( bool state )
{
    ui->cb_expo->setChecked(state);
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

int mainw::HW_wrapper_ADC_light(void)
{
    return ui->sld_light->value();
}

int mainw::HW_wrapper_ADC_sound(void)
{
    return ui->sld_sound->value();
}

void mainw::HW_wrapper_ADC_On_indicator(bool value)
{
    ui->cb_adc_on->setChecked( value );
}

////////////////////////////////////////////////////

void mainw::HW_wrapper_setup( int interval )
{
    pClass = this;
    tiv    = interval;

}


void mainw::HW_wrapper_update_display()
{
    if ( disp_changed )
    {
        disp_changed = false;
        ui->num_pwr_mng->setValue( hw_power_mode );
    }
}


// custom debug interface
void mainw::HW_wrapper_DBG( int val )
{
    ui->num_dbg->setValue( val );

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
uint32 eeprom_write( uint32 address, uint8 *buff, int count )
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

