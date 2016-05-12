#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hw_stuff.h"
#include "typedefs.h"
#include "core.h"

#include "hw_stuff.h"
#include "utilities.h"
#include "events_ui.h"

#define UART_CKSUM_MAGICNR  0xAABBCCDD;

uint8 in_buffer[ 1024*1024 ];
int   in_size;
FILE *file;
int ptr = 0; 
int buff_len = 0;


int internal_print_readout( struct SCoreNVreadout *readout )
{
    fprintf( file, "\t\ttask_elem[0x%X]  task_elsizeX2[%d]  shifted[%d]  flipbuff[%d]\n", 
             (unsigned int)readout->taks_elem, readout->task_elsizeX2, readout->shifted, readout->flipbuff );
    fprintf( file, "\t\ttask_offs[0x%08lX]  wrap[%d]  total_read[%d]  last_timestamp[0x%08lX]\n",
             readout->task_offs, readout->wrap, readout->total_read, readout->last_timestamp );
    fprintf( file, "\t\tto_read[%d]  to_ptr[%d]  to_process[%d]\n\n",
             readout->to_read, readout->to_ptr, readout->to_process );

    fprintf( file, "\t\tv1ctr[0x%08lX]  v2ctr[0x%08lX]  v3ctr[0x%08lX]\n", readout->v1ctr, readout->v2ctr, readout->v3ctr );
    fprintf( file, "\t\tv1max[0x%08lX]  v2max[0x%08lX]  v3max[0x%08lX]\n", readout->v1max, readout->v2max, readout->v3max );
    fprintf( file, "\t\tv1min[0x%08lX]  v2min[0x%08lX]  v3min[0x%08lX]\n", readout->v1min, readout->v2min, readout->v3min );
    fprintf( file, "\t\tv1max_total[0x%08lX]  v2max_total[0x%08lX]  v3max_total[0x%08lX]\n", readout->v1max_total, readout->v2max_total, readout->v3max_total );
    fprintf( file, "\t\tv1min_total[0x%08lX]  v2min_total[0x%08lX]  v3min_total[0x%08lX]\n", readout->v1min_total, readout->v2min_total, readout->v3min_total );
    fprintf( file, "\t\tv1sum[0x%08lX]  v2sum[0x%08lX]  v3sum[0x%08lX]\n\n", readout->v1sum, readout->v2sum, readout->v3sum );
             
    fprintf( file, "\t\tdispctr[0x%08lX]  dispstep[0x%08lX]  dispprev[0x%08lX] raw_ptr[0x%08lX]\n\n", readout->dispctr, readout->dispstep, readout->dispprev, readout->raw_ptr );

    return 0;
}

int internal_print_taskInstance( struct SRecTaskInstance *taskInst )
{
    fprintf( file, "\t\tmempage[%d]  size[%d]  task_elems[0x%X]  sample_rate[%d]\n\n",
             taskInst->mempage, taskInst->size, taskInst->task_elems, taskInst->sample_rate );
    return 0;
}

int internal_print_taskFunc( struct SRecTaskInternals *taskFunc )
{
    // print just a subset
    int i;
    fprintf( file, "\t\tr[%d]  w[%d]  c[%d]  wrap[%d]\n", taskFunc->r, taskFunc->w, taskFunc->c, taskFunc->wrap );

    fprintf( file, "\t\tavg_cnt[ " );
    for (i=0; i<3; i++)
        fprintf(file, "0x%08lX ", taskFunc->avg_cnt[i] );
    fprintf( file, "]\n" );

    fprintf( file, "\t\tavg_sum[ " );
    for (i=0; i<3; i++)
        fprintf(file, "0x%08lX ", taskFunc->avg_sum[i] );
    fprintf( file, "]\n" );

    fprintf( file, "\t\tlast_timestamp[0x%08lX] \n\n", taskFunc->last_timestamp);
    return 0;
}

int internal_print_NVRAM_readinfo( uint8 *inbuff )
{
    uint32 eeaddr;
    uint32 eesize;
    uint32 buffaddr;

    eeaddr = *(uint32*)(inbuff);
    eesize = *(uint32*)(inbuff+4);
    buffaddr = *(uint32*)(inbuff+8);

    fprintf( file, "\tNVRAM read parameters:\n"
                   "\t\taddress[0x%08lX]  size[%d]  -> buffer[0x%08lX]\n\n",
             eeaddr, eesize, buffaddr );

    buff_len = eesize;
    return 0;
}


int main(int argc, char *argv[])
{
    char *filename = argv[1];
    char outfname[64];

    struct SCoreNVreadout readout;
    struct SRecTaskInstance taskInst;
    struct SRecTaskInternals taskFunc;

    // open and read the raw dump data
    file = fopen( filename, "rb" );
    if ( file == NULL )
    {
        printf("Failed to open file %s", filename);
        return 1;
    }

    fseek(file, 0L, SEEK_END);
    in_size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    fread( in_buffer, 1, in_size, file );
    fclose(file);

    // open the output file
    strcpy( outfname, argv[1] );
    outfname[ strlen(outfname) - 4 ] = 0;
    strcat( outfname, ".txt");

    file = fopen( outfname, "w" );
    if ( file == NULL )
    {
        printf("Failed to open output file %s", outfname);
        return 1;
    }


    //--- parse the raw data and output result

    // check header - parse readout / task / taskfunc
    {
        uint8 hdr_start[] = { 0xff, 0xff, 0xff, 0xff };
        if ( strncmp( (char*)in_buffer, (char*)hdr_start, 4) )
        {
            fprintf( file, "Invalid header at start\n" );
            goto _error_exit;
        }
        fprintf( file, "{FF,FF,FF,FF} start readback\n"
                       "\tReadOut:\n" );

        ptr+= 4;

        // print readout
        memcpy( (uint8*)&readout, in_buffer+ptr, sizeof(readout) );
        ptr+=sizeof(readout);
        if ( internal_print_readout( &readout ) )
            goto _error_exit;

        // print task instance
        fprintf( file, "\tTask instance:\n");
        memcpy( (uint8*)&taskInst, in_buffer+ptr, sizeof(taskInst) );
        ptr+=sizeof(taskInst);
        if ( internal_print_taskInstance( &taskInst ) )
            goto _error_exit;

        // print task internals
        fprintf( file, "\tTask internals:\n");
        memcpy( (uint8*)&taskFunc, in_buffer+ptr, sizeof(taskFunc) );
        ptr+=sizeof(taskFunc);
        if ( internal_print_taskFunc( &taskFunc ) )
            goto _error_exit;

        // print NVRAM data
        internal_print_NVRAM_readinfo( in_buffer+ptr );
        ptr+= 3 * sizeof(uint32);

        if ( ptr > in_size )
            goto _error_exit;
    }


    // print read data
    do
    {
        uint8 hdr_data[] = { 0xaa, 0xaa, 0xaa };
        uint8 hdr_new[] = { 0x55, 0x55, 0x55, 0x55 };
        uint8 hdr_end[] = { 0x66, 0x66, 0x66, 0x66 };
        int i;
        uint32 temp;

        if ( strncmp( (char*)(in_buffer+ptr), (char*)hdr_data, 3) )
        {
            fprintf( file, "Invalid data header\n" );
            goto _error_exit;
        }
        temp = *(uint32*)(in_buffer+ptr+3);
        fprintf( file, "{AA,AA,AA} data from NVRAM - from flipped buffer [%d], data is shifted [%d]\n",
                 ( temp >> 4) & 0x0f, temp & 0x0f  );
        fprintf( file, "\tBuffer used [0x%08lX]:\n", *(uint32*)(in_buffer+ptr+4) );
        ptr+= 8;

        for ( i=0; i<buff_len; i++ )
        {
            if ( ((i%32) == 0) && (i>0))
                fprintf( file, "\n");
            if ( (i%32) == 0 )
                fprintf( file, "\t\t");

            if ( ((i%32) == 7) || ((i%32) == 15) || ((i%32) == 23) )
                fprintf( file, "%02X | ", *(in_buffer+ptr) );
            else
                fprintf( file, "%02X ", *(in_buffer+ptr) );

            ptr++;
        }
        fprintf( file, "\n\n");
        
        // check new read was initiated
        if ( strncmp( (char*)(in_buffer+ptr), (char*)hdr_new, 4) == 0 )
        {
            fprintf( file, "{55,55,55,55} new readback initiated\n"
                           "\tReadOut:\n" );
            ptr+= 4;

            // print readout
            memcpy( (uint8*)&readout, in_buffer+ptr, sizeof(readout) );
            ptr+=sizeof(readout);
            if ( internal_print_readout( &readout ) )
                goto _error_exit;

            // print NVRAM data
            internal_print_NVRAM_readinfo( in_buffer+ptr );
            ptr+= 3 * sizeof(uint32);

            if ( ptr > in_size )
                goto _error_exit;
        }
        else if ( strncmp( (char*)(in_buffer+ptr), (char*)hdr_end, 4) == 0 )
        {
            // finished readback
            fprintf( file, "{66,66,66,66} readback finished\n"
                           "\tReadOut:\n" );

            ptr+= 4;

            // print readout
            memcpy( (uint8*)&readout, in_buffer+ptr, sizeof(readout) );
            ptr+=sizeof(readout);
            if ( internal_print_readout( &readout ) )
                goto _error_exit;

            fprintf( file, "End sequence : %02X, %02X, %02X, %02X\n",
                     *(in_buffer+ptr), *(in_buffer+ptr+1), *(in_buffer+ptr+2), *(in_buffer+ptr+3) );
            ptr+= 4;

            // checksum processing
            {
                int i;
                uint32 cksum = UART_CKSUM_MAGICNR;
                uint32 in_cksum = *(uint32*)(in_buffer + ptr);

                for (i=0; i<ptr; i++)
                {
                    uint8 data;
                    data = *(in_buffer+i);
                    cksum = cksum + data + ( ((1 - data)&0xff) << 16 );
                }

                fprintf( file, "\nCKSUM:  InCksum[0x%08lX]  CalcCksum[0x%08lX]\n", in_cksum, cksum );
                if ( cksum == in_cksum )
                    fprintf( file, "\t checksum OK\n");
                else
                    fprintf( file, "\t checksum ERROR\n");
            }

            break;
        }
        else
        {
            fprintf( file, "Inconsistency detected\n");
            goto _error_exit;
        }

    } while (1);
    





_error_exit:
    fclose(file);
}
