/*
 *
 *
 **/

#include <stdio.h>
#include <string.h>
#include "core.h"
#include "hw_stuff.h"
#include "eeprom_spi.h"

#ifdef ON_QT_PLATFORM
struct STIM1 stim;
struct STIM1 *TIM1 = &stim;
struct STIM1 *TIMER_ADC;
#endif

static struct SCore         core;


void core_update_battery()
{
    uint32 battery;
    battery = HW_ADC_GetBattery();

    // limit battery value and remove offset
    if (battery > VBAT_MIN )
        battery -= VBAT_MIN;
    else
        battery = 0;
    if ( battery > VBAT_DIFF )
        battery = VBAT_DIFF;
    // scale to 0 - 100%
    core.measure.battery = (uint8)((battery * 100) / VBAT_DIFF);     
}


/////////////////////////////////////////////////////
//
//   main routines
//
/////////////////////////////////////////////////////


//-------------------------------------------------
//     Setup save / load
//-------------------------------------------------

int core_setup_save( void )
{
    int i;
    uint16  cksum       = 0xABCD;
    uint8   *SetParams  = (uint8*)&core.setup;

    eeprom_init();

    if ( eeprom_enable(true) )
        return -1;

    for ( i=0; i<sizeof(struct SCoreSetup); i++ )
        cksum = cksum + ( (uint16)(SetParams[i] << 8) - (uint16)(~SetParams[i]) ) + 1;

    if ( eeprom_write( 0x10, SetParams, sizeof(struct SCoreSetup) ) != sizeof(struct SCoreSetup) )
        return -1;
    if ( eeprom_write( 0x10 + sizeof(struct SCoreSetup), (uint8*)&cksum, 2 ) != 2 )
        return -1;
    eeprom_disable();
    return 0;
}


int core_setup_reset( bool save )
{
    struct SCoreSetup *setup = &core.setup;

    setup->disp_brt_on  = 0x30;
    setup->disp_brt_dim = 12;
    setup->disp_brt_auto = 0;
    setup->pwr_stdby = 30;       // 30sec standby
    setup->pwr_disp_off = 60;    // 1min standby
    setup->pwr_off = 300;        // 5min pwr off

    setup->beep_on = 1;
    setup->beep_hi = 1554;
    setup->beep_low = 2152;

    if ( save )
    {
        return core_setup_save();
    }
    return 0;
}


int core_setup_load( void )
{
    uint8   i;
    uint16  cksum       = 0xABCD;
    uint16  cksumsaved  = 0;
    uint8   *SetParams  = (uint8*)&core.setup;

    if ( eeprom_enable(false) )
        return -1;

    if ( eeprom_read( 0x10, sizeof(struct SCoreSetup), SetParams ) != sizeof(struct SCoreSetup) )
        return -1;
    if ( eeprom_read( 0x10 + sizeof(struct SCoreSetup), 2, (uint8*)&cksumsaved ) != 2 )
        return -1;

    for ( i=0; i<sizeof(struct SCoreSetup); i++ )
    {
        cksum = cksum + ( (uint16)(SetParams[i] << 8) - (uint16)(~SetParams[i]) ) + 1;
    }

    if ( cksum != cksumsaved )
    {
        // eeprom data corrupted or not initialized
        if ( core_setup_reset( true ) )
            return -1;
    }

    eeprom_disable();
    return 0;
}


void core_beep( enum EBeepSequence beep )
{
    if ( core.setup.beep_on )
    {
        BeepSequence( (uint32)beep );
    }
}



//-------------------------------------------------
//     main core interface
//-------------------------------------------------

int core_init( struct SCore **instance )
{
    memset(&core, 0, sizeof(core));
    *instance = &core;

    if ( eeprom_init() )
        goto _err_exit;
    if ( core_setup_load() )
        goto _err_exit;

    core_update_battery();
    BeepSetFreq( core.setup.beep_low, core.setup.beep_hi );
    return 0;

_err_exit:
    return -1;
}


void core_poll( struct SEventStruct *evmask )
{
    // poll the started state
    if ( core.op.op_state )
    {

    }
    return;
}


int  core_get_pwrstate()
{
    if ( core.op.op_state == 0 )                // if core is stopped we don't care about adc - ui should take care
        return SYSSTAT_CORE_STOPPED;
}








