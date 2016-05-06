#ifndef COM_LINK_H
#define COM_LINK_H

#include <QThread>
#include <QString>
#include "MSerialPort.hpp"


enum EconnectStatus
{
    conn_disconnected = 0,
    conn_connected
};

#ifndef uint8
typedef unsigned char uint8;
#endif
#ifndef uint32
typedef unsigned int uint32;
#endif

class com_link : public QObject
{
    Q_OBJECT
public:
    explicit com_link();
    ~com_link();

    enum EconnectStatus is_opened();                                    

    int cmd_connect();                                                  // make the connection to the CNC machine
    int cmd_disconnect();                                               // disconnect from the CNC
    int cmd_read_data_dump(uint8 *pbuff, uint32 *psize );               // when coordinate setup is done


public slots:
    // Events from the underlying serial port driver
    void Serial_GetData();
    void Serial_Overflow();


private:

    MSerialPort *port;                          // serial port instance
    enum EconnectStatus port_opened;            // if connection is done and which type

    bool sig_overflow;
    bool sig_data_avail;

    int OpenCommunication();                        // Open the communication channel
    void CloseCommunication();                      // Closes the communication channel
    int ReadDumpData( uint8 *pbuff, uint32 *psize );// Finish the setup procedure
};

#endif // COM_LINK_H
