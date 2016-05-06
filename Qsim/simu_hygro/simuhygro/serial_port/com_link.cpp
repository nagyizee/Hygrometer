#include "com_link.h"

#include "MSerialPort.hpp"
#include <QString>
#include <QtDebug>


/////////////////////// DEBUG MESSAGES ////////////////////////
// switches
#define DEBUG_CORE
#define DEBUG_CORE_TRACE
#define DEBUG_API

// for terminal testing - do not use on the machine
// #define DBG_SEND_NEW_LINE_ALSO



// defines
// do not edit
#ifdef DEBUG_CORE
#define DBG_MESSAGE_CORE1(a)         do {  \
                                            qDebug() << "ComLink Core: " << a; \
                                       } while (0)

#define DBG_MESSAGE_CORE2(a, b)     do {  \
                                            qDebug() << "ComLink Core: " << a << b; \
                                       } while (0)

#define DBG_MESSAGE_CORE3(a, b, c)  do {  \
                                            qDebug() << "ComLink Core: " << a << b << c; \
                                       } while (0)

#else
#define DBG_MESSAGE_CORE1(a)         do {  } while (0)

#define DBG_MESSAGE_CORE2(a, b)      do {  } while (0)

#define DBG_MESSAGE_CORE3(a, b, c)   do {  } while (0)
#endif


#ifdef DEBUG_API
#define DBG_MESSAGE_API1(a)         do {  \
                                            qDebug() << "ComLink API: " << a; \
                                       } while (0)

#define DBG_MESSAGE_API2(a, b)     do {  \
                                            qDebug() << "ComLink API: " << a << b; \
                                       } while (0)

#define DBG_MESSAGE_API3(a, b, c)  do {  \
                                            qDebug() << "ComLink API: " << a << b << c; \
                                       } while (0)

#else
#define DBG_MESSAGE_API1(a)         do {  } while (0)

#define DBG_MESSAGE_API2(a, b)      do {  } while (0)

#define DBG_MESSAGE_API3(a, b, c)   do {  } while (0)
#endif


#ifdef DEBUG_CORE_TRACE
#define DBG_MESSAGE_CORE_TRACE1(a)         do {  \
                                            qDebug() << "ComLink Core: " << a; \
                                       } while (0)

#define DBG_MESSAGE_CORE_TRACE2(a, b)     do {  \
                                            qDebug() << "ComLink Core: " << a << b; \
                                       } while (0)

#define DBG_MESSAGE_CORE_TRACE3(a, b, c)  do {  \
                                            qDebug() << "ComLink Core: " << a << b << c; \
                                       } while (0)

#else
#define DBG_MESSAGE_CORE_TRACE1(a)         do {  } while (0)

#define DBG_MESSAGE_CORE_TRACE2(a, b)      do {  } while (0)

#define DBG_MESSAGE_CORE_TRACE3(a, b, c)   do {  } while (0)
#endif
///////////////////////////////////////////////////////////////



com_link::com_link()
{
    // make everything in this thread, run in this thread. (Including signals/slots)
    //QObject::moveToThread(this);
    port_opened     = conn_disconnected;
    sig_data_avail  = false;
    sig_overflow    = false;

    DBG_MESSAGE_CORE1("Core communicaition module started");

    port = new MSerialPort();
    connect(port, SIGNAL(hasEvent()), this, SLOT( Serial_GetData()) );
    connect(port, SIGNAL(bufferOverflow()), this, SLOT( Serial_Overflow()) );
}

com_link::~com_link()
{
    delete port;
    DBG_MESSAGE_CORE1("Core communicaition module exit");
}




// Events from the underlying serial port driver
void com_link::Serial_GetData()
{
    sig_data_avail = true;
}

void com_link::Serial_Overflow()
{
    sig_overflow = true;
}




///////////////////////////////////////////////////////////////////////
//
//     Interface routines
//
///////////////////////////////////////////////////////////////////////

int com_link::cmd_connect()
{
    if (port_opened)
        return 0;
    return OpenCommunication();
}

int com_link::cmd_disconnect()
{
    if (port_opened)
    {
        CloseCommunication();
    }
    return 0;
}

int com_link::cmd_read_data_dump(uint8 *pbuff, uint32 *psize )
{
    if (port_opened )
    {
        return ReadDumpData(pbuff, psize);
    }
    else
        return -1;
}

enum EconnectStatus com_link::is_opened()
{
    return port_opened;
}


///////////////////////////////////////////////////////////////////////
//
//     Functional stuff
//
///////////////////////////////////////////////////////////////////////


int com_link::OpenCommunication()
{
    // searches an available comm. port and checks the existence of the CNC device on it
    if ( port_opened )
        return 0;

    int i = 0;
    int res = -1;

    QString comport = "COM1";

    do
    {
        i++;
        if ( i == 33 )
        {
            res = -1;
            break;
        }

        comport = "COM" + QString::number(i);

        res = port->openPort( &comport, Baud115200, CS8, SB1, ParityNone, '\r');
        if ( res != 0 )
        {
            continue;
        }
        else
        {
            char buff[30];
            DBG_MESSAGE_CORE2("Found serial port", comport.toLatin1() );

            // check the existence of the CNC controller
            port->flushOutQ();
            port->flushInQ();

            // return success
            res = 0;
            break;
        }
    } while (1);

    DBG_MESSAGE_CORE2("Connection result: ", res);
    if ( res != -1 )
    {
        port_opened = conn_connected;
    }

    return res;
}//END: OpenCommunication



void com_link::CloseCommunication()
{
    if ( port_opened == conn_disconnected )
        return;

    DBG_MESSAGE_CORE1("Closing comm port");
    port->closePort();
    port_opened = conn_disconnected;
}//END: CloseCommunication



int com_link::ReadDumpData( uint8 *pbuff, uint32 *psize )
{
    int res;
    int size = *psize;
    int rsize = 0;

    if ( port_opened == conn_disconnected)
        return -1;

    port->flushInQ();

    // wait for event character (end of line)
    int timeout = 1000;

    while ( rsize < size )
    {
        int sz;
        sz = port->bytesAvailable();
        if ( sz )
        {
            port->getBytes( pbuff, sz );
            rsize += sz;
            pbuff += sz;
            timeout = 1000;
        }
        else
        {
            QThread::usleep(1000);   // done for yield
            timeout--;
            if (timeout == 0)
                break;
        }
    }

    if ( timeout == 0)      // no time out - counter still > 0
    {
        *psize = rsize;
        return -1;
    }

    return 0;
}

