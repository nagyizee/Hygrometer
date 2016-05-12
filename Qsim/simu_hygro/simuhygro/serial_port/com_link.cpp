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

    dataQ = 0;
    dataMax = 0;

    port_thread = new QThread();
    moveToThread( port_thread );

    // thread related connections
    connect(port_thread, SIGNAL(started()), this, SLOT(pslot_threadstartup()));
    connect(this, SIGNAL(sig_terminated()), port_thread, SLOT(quit()));
    connect(this, SIGNAL(sig_terminated()), this, SLOT(deleteLater()));
    connect(port_thread, SIGNAL(finished()), port_thread, SLOT(deleteLater()));

    connect(this, SIGNAL(sig_connect()), this, SLOT(pslot_connect()) );
    connect(this, SIGNAL(sig_disconnect()), this, SLOT(pslot_disconnect()) );

    connect(this, SIGNAL(sig_datadump_start()), this, SLOT(pslot_datadump_start()) );
    connect(this, SIGNAL(sig_datadump_stop()), this, SLOT(pslot_datadump_stop()) );

    port_thread->start();

    DBG_MESSAGE_CORE1("Core communicaition module started");   
}

com_link::~com_link()
{
    int tid = (int)QThread::currentThreadId();

    t_result = 1;
    emit sig_disconnect();      // disconnect serial port
    while (t_result == 1)
        QThread::yieldCurrentThread();

    port_thread->quit();        // exit worker thread
    port_thread->wait(5000);

    if ( port )
        delete port;
}


void com_link::pslot_threadstartup()
{
    // thread start-up

    // create the port on the worker thread
    port = new MSerialPort();
    connect(port, SIGNAL(hasEvent()), this, SLOT( Serial_GetData()) );
    connect(port, SIGNAL(bufferOverflow()), this, SLOT( Serial_Overflow()) );

}

void com_link::pslot_connect()
{
    if (port_opened)
    {
        t_result = 0;
        return;
    }
    t_result = OpenCommunication();
}

void com_link::pslot_disconnect()
{
    if (port_opened)
        CloseCommunication();
    t_result = 0;
}

//qqq void com_link::pslot_datadump_start(uint8 *pbuff, uint32 max_size)
void com_link::pslot_datadump_start()
{
    uint8* pbuff = m_dataBuff;

    dataMax = m_dataMax;
    dataQ = 0;

    port->flushInQ();

    while ( dataQ < dataMax )
    {
        int sz;
        sz = port->bytesAvailable();
        if ( sz )
        {
            port->getBytes( (pbuff + dataQ), sz );
            dataQ += sz;
        }
        else
        {
            QThread::usleep(1000);   // done for yield
        }
    }
}

void com_link::pslot_datadump_stop()
{
    dataMax = 0;

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
    t_result = 1;           // start with 1 - modified by worker thread
    emit sig_connect();
    while (t_result == 1)
        QThread::yieldCurrentThread();

    if ( t_result )
    {
////        connect(this, SIGNAL(sig_datadump_start(uint8*,uint32)), this, SLOT(pslot_datadump_start(uint8*,uint32)) );
//        connect(this, SIGNAL(sig_datadump_start()), this, SLOT(pslot_datadump_start()) );
//        connect(this, SIGNAL(sig_datadump_stop()), this, SLOT(pslot_datadump_stop()) );
    }
    return t_result;
}

int com_link::cmd_disconnect()
{
    t_result = 1;
    dataMax = 0;        // stop the data receiving - if any
    emit sig_disconnect();
    while (t_result == 1)
        QThread::yieldCurrentThread();

    return t_result;
}

int com_link::cmd_read_data_dump_start(uint8 *pbuff, uint32 max_size )
{
    if (port_opened == conn_disconnected )
        return -1;
    if ( dataMax || (max_size==0) )
        return -1;

//qqq    emit sig_datadump_start(pbuff, max_size);
    m_dataMax = max_size;
    m_dataBuff = pbuff;
    emit sig_datadump_start();
    while (dataMax == 0)
        QThread::yieldCurrentThread();

    return 0;
}

int com_link::cmd_read_data_check_inbuffer()
{
    return dataQ;   // should be atomic
}

int com_link::cmd_read_data_stop()
{
//    emit sig_datadump_stop();
//    while (dataMax)
//        QThread::yieldCurrentThread();
    dataMax = 0;        // stop the data receiving
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

        res = port->openPort( &comport, 115200 , CS8, SB1, ParityNone, 0);      // 614400
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

