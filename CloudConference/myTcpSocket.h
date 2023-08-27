#ifndef MYTCPSOCKET_H
#define MYTCPSOCKET_H

#include <QObject>
#include <QThread>
#include <QTcpSocket>
#include <QMutex>
#include "netheader.h"
#ifndef MB
#define MB (1024 * 1024)
#endif


typedef unsigned char uchar;

class MyTcpSocket : public QThread
{
    Q_OBJECT
public:
    explicit MyTcpSocket(QObject *parent = nullptr);
    ~MyTcpSocket();
    bool connectToServer(QString, QString, QIODevice::OpenModeFlag);
    QString errorString();
    void disconnectFromHost();//与主机断开连接
    quint32 getlocalip();

private:
    void run() override;
    qint64 readn(char *, quint64, int);
    QTcpSocket *m_socktcp;
    QThread *m_sockThread;
    uchar *sendbuf;
    uchar *recvbuf;
    quint64 hasrecvive;

    QMutex m_lock;
    volatile bool m_isCanRun;

private slots://槽函数声明
    bool connectServer(QString, QString, QIODevice::OpenModeFlag);
    void sendData(MESG *);
    void closeSocket();

public slots:
    void recvFromSocket();
    void stopImmediately();//立即停止
    void errorDetect(QAbstractSocket::SocketError error);//错误检测
signals:
    void socketerror(QAbstractSocket::SocketError);
    void sendTextOver();

};

#endif // MYTCPSOCKET_H
