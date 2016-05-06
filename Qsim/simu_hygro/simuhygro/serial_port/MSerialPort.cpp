#include "MSerialPort.hpp"
#include <QtDebug>

//////////////////////////////////////////
// Set header file for documentation    /
////////////////////////////////////////

//////////////////////////////////////////////////////
// Windows Version of the serial port driver Code
/////////////////////////////////////////////////////

#ifdef Q_OS_WIN32

#include <windows.h>

MSerialPort::MSerialPort()
{
    // make our data buffer
    dataBuffer = new QByteArray();
    hSerial = INVALID_HANDLE_VALUE;
    deviceName = NULL;
    state = 0;
    event_char = 0;
}

MSerialPort::~MSerialPort()
{
    if (hSerial != INVALID_HANDLE_VALUE)
        CloseHandle(hSerial);
    hSerial = INVALID_HANDLE_VALUE;
}


// setup what device we should use
int MSerialPort::openPort(QString *device_Name, int _buad, int _byteSize, int _stopBits, int _parity, uint8_t ev_char)
{
    if ( hSerial != INVALID_HANDLE_VALUE )
        return -1;      // port is opened allready

    deviceName = new QString(device_Name->toLatin1());
    // serial port settings
    Buad        = _buad;
    ByteSize    = _byteSize;
    StopBits    = _stopBits;
    Parity      = _parity;
    event_char  = ev_char;

    // open selected device
    hSerial = CreateFile( (WCHAR *) deviceName->constData() , GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if ( hSerial == INVALID_HANDLE_VALUE )
    {
        DWORD errorCode = GetLastError();
        qDebug() << "MSerialPort: Failed to open serial port " << deviceName->toLatin1() << " err " << errorCode;
        return -1;
    }

    // Yay we are able to open device as read/write
    qDebug() << "MSerialPort: Opened serial port " << deviceName->toLatin1() << " Sucessfully!";

    // now save current device/terminal settings
    dcbSerialParams.DCBlength=sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams))
    {
        qDebug() << "MSerialPort: Failed to get com port paramters";
        CloseHandle(hSerial);
        return -1;
    }

    if (_SERIALTHREAD_DEBUG) {
        qDebug() << "MSerialPort: Serial port setup and ready for use";
        qDebug() << "MSerialPort: Starting MSerialPort main loop";
    }

    dcbSerialParams.BaudRate    = Buad;
    dcbSerialParams.ByteSize    = ByteSize;
    dcbSerialParams.Parity      = Parity;
    dcbSerialParams.StopBits    = StopBits;
    dcbSerialParams.fOutxCtsFlow    = 0;
    dcbSerialParams.fOutxDsrFlow    = 0;
    dcbSerialParams.fDtrControl     = DTR_CONTROL_DISABLE;

    if(!SetCommState(hSerial, &dcbSerialParams))
    {
        qDebug() << "MSerialPort: Failed to set new com port paramters";
        CloseHandle(hSerial);
        return -2;
    }

    COMMTIMEOUTS lpCommTimeouts;
    GetCommTimeouts( hSerial, &lpCommTimeouts );
    lpCommTimeouts.ReadIntervalTimeout          = MAXDWORD;
    lpCommTimeouts.ReadTotalTimeoutMultiplier   = 0;
    lpCommTimeouts.ReadTotalTimeoutConstant     = 0;
    lpCommTimeouts.WriteTotalTimeoutConstant    = 0;
    lpCommTimeouts.WriteTotalTimeoutMultiplier  = 0;
    if (!SetCommTimeouts( hSerial, &lpCommTimeouts ))
    {
        qDebug() << "MSerialPort: Failed to set new com port paramters";
        CloseHandle(hSerial);
        return -2;
    }

    return 0;
}


//close the serial port
void MSerialPort::closePort()
{
    if ( hSerial != INVALID_HANDLE_VALUE )
        CloseHandle(hSerial);
    hSerial = INVALID_HANDLE_VALUE;
}



//write data to serial port
int MSerialPort::writeBuffer( uint8_t *buffer, int size )
{
    int dwBytesRead = 0;

    if (hSerial == INVALID_HANDLE_VALUE)
        return -1;

    WriteFile(hSerial, buffer, size, (DWORD *)&dwBytesRead, NULL);
    return dwBytesRead;
}


//flush output queue
void MSerialPort::flushInQ()
{
    if (hSerial == INVALID_HANDLE_VALUE)
        return;

    PurgeComm( hSerial, PURGE_RXCLEAR );
    dataBuffer->clear();
}


//flush output queue
void MSerialPort::flushOutQ()
{
    if (hSerial == INVALID_HANDLE_VALUE)
        return;
    PurgeComm( hSerial, PURGE_TXCLEAR );
}


// return number of bytes in receive buffer
uint32_t MSerialPort::bytesAvailable() {
    // this is thread safe, read only operation
    poll();
    return (uint32_t)dataBuffer->size();
}


// data fetcher, get next byte from buffer
uint8_t MSerialPort::getNextByte()
{
    // mutex needed to make thread safe
    uint8_t byte = (uint8_t)dataBuffer->at(0); // get the top most byte
    dataBuffer->remove(0, 1); // remove top most byte
    return byte; // return top most byte
}


// get size amount of data bytes
int MSerialPort::getBytes( uint8_t *buffer, int size )
{
    int read_nr;
    int size_in_buff;
    if (hSerial == INVALID_HANDLE_VALUE)
        return -1;

    size_in_buff = dataBuffer->size();
    for (read_nr = 0; read_nr < size; read_nr++)
    {
        if ( read_nr == size_in_buff )
            break;
        buffer[read_nr] = (uint8_t)dataBuffer->at(0); // get the top most byte
        dataBuffer->remove(0, 1); // remove top most byte
    }
    return read_nr;
}


// get size amount of data bytes but exits on "until" character
int MSerialPort::getBytes( uint8_t *buffer, uint8_t until_chr, int size )
{
    int read_nr;
    int byte_nr = 0;
    int size_in_buff;
    int no_event_char = 0x80000000;
    if (hSerial == INVALID_HANDLE_VALUE)
        return -1;

    size_in_buff = dataBuffer->size();
    for (read_nr = 0; byte_nr < size; read_nr++)        // use bytenr for comparison and readnr for cursor
    {
        uint8_t chr;
        if ( read_nr == size_in_buff )      // if no more data in the input buffer
            break;

        chr = (uint8_t)dataBuffer->at(0);
        if ( chr == until_chr )
        {
            dataBuffer->remove(0, 1); // remove top most byte
            no_event_char = 0;
            break;
        }

        if ( ((until_chr == '\r') || (until_chr == '\n')) && ((chr == '\r') || (chr == '\n')) )
        {
            // do not insert end of line
        }
        else
            buffer[byte_nr++] = chr;

        dataBuffer->remove(0, 1); // remove top most byte
    }
    return byte_nr | no_event_char;
}


bool MSerialPort::checkForEventChar()
{
    bool result = 0;
    poll();
    result = dataBuffer->contains( (char)event_char );
    return result;
}



// poll the receive buffer for new data
int MSerialPort::poll()
{
    uint8_t byte[512];  // temp storage byte
    int res = 0;
    int dwBytesRead;
    int state=0;        // state machine state
    int iterations = 3;  // leave only 3 iterations in this loop to prevent in-loop overflow

    do
    {
        int to_read = 512;
        if ( (dataBuffer->size() + to_read) > 2048 )
            to_read =2048 - dataBuffer->size();

        if (to_read == 0)
        {
            res = -1;
            break;
        }

        // poll the input serial buffer
        ReadFile(hSerial, (void *)&byte, to_read, (DWORD *)&dwBytesRead, NULL);
        if (dwBytesRead == 0)
            break;                  // if there was no data, return immediately

        // print what we received
        if (_SERIALTHREAD_DEBUG )
            qDebug() << "MSerialPort: Received amount " << dwBytesRead;
        if ( state == 1 )
        {
            qDebug() << "Local buffer no-longer overflowing, back to normal";
            state = 0;
        }

        // stick byte read from device into buffer
        dataBuffer->append( (char*)byte, dwBytesRead );
        if ( event_char && dataBuffer->contains( (char)event_char ) )
        {
            if (_SERIALTHREAD_DEBUG)
                qDebug() << "MSerialPort: event character reached";
            emit hasEvent(); // signal our user that there is data to receive till event character
            res = 1;
        }

        iterations--;
    } while( iterations );


    if ( (res == -1) && (state == 0) )
    {
        qDebug() << "Local buffer overflow, dropping input serial port data";
        state = 1;  // over-flowed state
        emit bufferOverflow();
    }

    return res;
}

#else

//////////////////////////////////////////////////////////
// Linux/Mac/BSD Version of the serial port driver Code
////////////////////////////////////////////////////////

// POSIX C stuff for accessing the serial port
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

// Constructor
MSerialPort::MSerialPort(QObject *parent) :
    QThread(parent)
{
    // make everything in this thread, run in this thread. (Including signals/slots)
    QObject::moveToThread(this);
    // make our data buffer
    dataBuffer = new QByteArray();
    running = true;
    deviceName=NULL;
    bufferMutex = new QMutex(); // control access to buffer
    bufferMutex->unlock();  // not in a locked state
}

MSerialPort::~MSerialPort() {
                running = false;
                close(sfd);
}

//write data to serial port
int MSerialPort::writeBuffer(QByteArray *buffer) {
    if (sfd != 0) {
        // have a valid file discriptor
        return write(sfd, buffer->constData(), buffer->size());
    } else {
        return -1;
    }
}

// setup what device we should use
void MSerialPort::usePort(QString *device_Name) {
    deviceName = new QString(device_Name->toLatin1());
    if (_SERIALTHREAD_DEBUG) {
        qDebug() << "MSerialPort: Using device: " << deviceName->toLatin1();
    }
}

// data fetcher, get next byte from buffer
uint8_t MSerialPort::getNextByte() {
    // mutex needed to make thread safe
    bufferMutex->lock(); // lock access to resource, or wait untill lock is avaliable
    uint8_t byte = (uint8_t)dataBuffer->at(0); // get the top most byte
    dataBuffer->remove(0, 1); // remove top most byte
    bufferMutex->unlock();
    return byte; // return top most byte
}

// return number of bytes in receive buffer
uint32_t MSerialPort::bytesAvailable() {
    // this is thread safe, read only operation
    return (uint32_t)dataBuffer->size();
}

// our main code thread
void MSerialPort::run() {
    // thread procedure
    if (_SERIALTHREAD_DEBUG) {
        qDebug() << "MSerialPort: MSerialPort Started..";
        qDebug() << "MSerialPort: Openning serial port " << deviceName->toLatin1();
    }

    // open selected device
    sfd = open( deviceName->toLatin1(), O_RDWR | O_NOCTTY );
    if ( sfd < 0 ) {
        qDebug() << "MSerialPort: Failed to open serial port " << deviceName->toLatin1();
        emit openPortFailed();
        return;  // exit thread
    }

    // Yay we are able to open device as read/write
    qDebug() << "MSerialPort: Opened serial port " << deviceName->toLatin1() << " Sucessfully!";
    // now save current device/terminal settings
    tcgetattr(sfd,&oldtio);
    // setup new terminal settings
    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = Buad | ByteSize | StopBits | Parity | CREAD | CLOCAL; // enable rx, ignore flowcontrol
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until atleast 1 charactors received */
    // flush device buffer
    tcflush(sfd, TCIFLUSH);
    // set new terminal settings to the device
    tcsetattr(sfd,TCSANOW,&newtio);
    // ok serial port setup and ready for use

    if (_SERIALTHREAD_DEBUG) {
        qDebug() << "MSerialPort: Serial port setup and ready for use";
        qDebug() << "MSerialPort: Starting MSerialPort main loop";
    }

    // signal we are opened and running
    emit openPortSuccess();

    uint8_t byte; // temp storage byte
    int state=0;    // state machine state

    // start polling loop
    while(running) {
        read(sfd, (void *)&byte, 1); // reading 1 byte at a time..  only 2400 baud.
        // print what we received
        if (_SERIALTHREAD_DEBUG) {
            qDebug() << "MSerialPort: Received byte with value: " << byte;
        }
        if (dataBuffer->size() > 1023) {
            if ( state == 0 ) {
                qDebug() << "Local buffer overflow, dropping input serial port data";
                state = 1;  // over-flowed state
                emit bufferOverflow();
            }
        } else {
            if ( state == 1 ) {
                qDebug() << "Local buffer no-longer overflowing, back to normal";
                state = 0;;
            }
            // stick byte read from device into buffer
            // Mutex needed to make thread safe from buffer read operation
            bufferMutex->lock();
            dataBuffer->append(byte);
            bufferMutex->unlock();
            emit hasData(); // signal our user that there is data to receive
        }
    }
    close(sfd);
}

#endif // OS Selection
