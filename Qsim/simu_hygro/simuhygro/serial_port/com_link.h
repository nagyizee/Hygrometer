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

    enum EconnectStatus is_opened();                                    // check if port is opened

    int cmd_connect();                                                  // connect
    int cmd_disconnect();                                               // disconnect
    int cmd_read_data_dump_start(uint8 *pbuff, uint32 max_size );      // start data dump in a buffer (give the maximum buffer size)
    int cmd_read_data_check_inbuffer();                                 // returns how many data bytes are allready read
    int cmd_read_data_stop();                                           // stop data read. - application can use the buffer after this

signals:
    void sig_connect();
    void sig_disconnect();
//qqq    void sig_datadump_start(uint8 *pbuff, uint32 max_size);
    void sig_datadump_start();
    void sig_datadump_stop();

public slots:
    // Events from the underlying serial port driver
    void Serial_GetData();
    void Serial_Overflow();

private slots:
    void pslot_threadstartup();

    void pslot_connect();                       // activated by signal sig_connect
    void pslot_disconnect();                    // activated by signal sig_sidconnect
//qqq    void pslot_datadump_start(uint8 *pbuff, uint32 max_size);
    void pslot_datadump_start();
    void pslot_datadump_stop();


private:
    QThread *port_thread;
    QMutex  mutex;
    int  t_result;

    MSerialPort *port;                          // serial port instance
    enum EconnectStatus port_opened;            // if connection is done and which type
    int dataQ;                                  // data quantity
    int dataMax;                                // max to read - it is set when receiving task is started
    uint8 *dataBuff;                            // data buffer


    bool sig_overflow;
    bool sig_data_avail;

    int OpenCommunication();                        // Open the communication channel
    void CloseCommunication();                      // Closes the communication channel
};

#endif // COM_LINK_H
