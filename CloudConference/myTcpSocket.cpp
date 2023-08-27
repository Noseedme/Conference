#include "myTcpSocket.h"
#include "netheader.h"
#include <QHostAddress>
#include <QtEndian>
#include <QMetaObject>
#include <QMutexLocker>
#include <QDebug>

extern QUEUE_DATA<MESG> queue_send;
extern QUEUE_DATA<MESG> queue_recv;
extern QUEUE_DATA<MESG> audio_recv;

MyTcpSocket::MyTcpSocket(QObject *parent)
    : QThread{parent}
{
    //声明自定义类型(Sockect错误类型) 防止跨线程信号槽报错
    qRegisterMetaType<QAbstractSocket::SocketError>();
    m_socktcp = nullptr;
    m_sockThread = new QThread();//发送数据线程
    this->moveToThread(m_sockThread);//将创建套接字任务交给线程
    //完成后关闭套接字
    connect(m_sockThread,SIGNAL(finished()),this,SLOT(closeSocket()));

    sendbuf =(uchar *) malloc(4 * MB);
    recvbuf = (uchar*) malloc(4 * MB);
    hasrecvive = 0;//待接收数据
}


MyTcpSocket::~MyTcpSocket()
{
    delete sendbuf;
    delete m_sockThread;
}


QString MyTcpSocket::errorString()
{
    return m_socktcp->errorString();
}


void MyTcpSocket::disconnectFromHost()
{
    //write
    if(this->isRunning())
    {
        QMutexLocker locker(&m_lock);
        m_isCanRun = false;
    }

    if(m_sockThread->isRunning()) //read
    {
        m_sockThread->quit();//退出等待
        m_sockThread->wait();
    }

    //清空发送队列，清空接受队列
    queue_send.clear();
    queue_recv.clear();
    audio_recv.clear();
}


quint32 MyTcpSocket::getlocalip()
{
    if(m_socktcp->isOpen())
    {
        //还回本地Ipv4的地址
        return m_socktcp->localAddress().toIPv4Address();
    }
    else
    {
        return -1;
    }
}
/*
 * 发送线程
 */
void MyTcpSocket::run()
{
    //qDebug() << "send data" << QThread::currentThreadId();
    m_isCanRun = true; //标记可以运行
    /*
    *$_MSGType_IPV4_MSGSize_data_# //
    * 1 2 4 4 MSGSize 1
    *底层写数据线程
    */
    while(1)
    {
        {
            QMutexLocker locker(&m_lock);
            if(m_isCanRun == false) return; //在每次循环判断是否可以运行，如果不行就退出循环
        }

        //构造消息体
        MESG * send = queue_send.pop_msg();
        if(send == NULL) continue;
        //调用发送数据函数
        QMetaObject::invokeMethod(this, "sendData", Q_ARG(MESG*, send));
    }
}


qint64 MyTcpSocket::readn(char * buf, quint64 maxsize, int n)//读取
{
    quint64 needRead = n;
    quint64 hasRead = 0;
    do
    {
        qint64 ret  = m_socktcp->read(buf + hasRead, needRead);
        if(ret < 0)
        {
            return -1;
        }
        if(ret == 0)
        {
            return hasRead;
        }
        hasRead += ret;
        needRead -= ret;
    }while(needRead > 0 && hasRead < maxsize);
    return hasRead;
}


bool MyTcpSocket::connectServer(QString ip, QString port, QIODevice::OpenModeFlag flag)
{
    if(m_socktcp == nullptr) m_socktcp = new QTcpSocket(); //tcp
    m_socktcp->connectToHost(ip, port.toUShort(), flag);
    connect(m_socktcp, SIGNAL(readyRead()), this, SLOT(recvFromSocket()), Qt::UniqueConnection); //接受数据
    //处理套接字错误
    connect(m_socktcp, SIGNAL(socketerror(QAbstractSocket::SocketError)), this, SLOT(errorDetect(QAbstractSocket::SocketError)),Qt::UniqueConnection);

    if(m_socktcp->waitForConnected(5000))//连接超时
    {
        return true;
    }
    m_socktcp->close();
    return false;
}


bool MyTcpSocket::connectToServer(QString ip, QString port, QIODevice::OpenModeFlag flag)
{
    m_sockThread->start(); // 开启socket通信
    bool retVal;
    //调用一个对象的信号、槽、可调用的方法 指针 函数名 连接类型 被调用函数的返回值
    QMetaObject::invokeMethod(this, "connectServer", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, retVal),
                              Q_ARG(QString, ip), Q_ARG(QString, port), Q_ARG(QIODevice::OpenModeFlag, flag));
    if (retVal)
    {
        this->start() ; //写数据
        return true;
    }
    else
    {
        return false;
    }
}



void MyTcpSocket::sendData(MESG* send)
{
    //未连接状态
    if (m_socktcp->state() == QAbstractSocket::UnconnectedState)
    {
        emit sendTextOver();
        if (send->data) free(send->data);
        if (send) free(send);
        return;
    }
    quint64 bytestowrite = 0;
    //构造消息头
    sendbuf[bytestowrite++] = '$';

    //消息类型 转为大端
    qToBigEndian<quint16>(send->msg_type, sendbuf + bytestowrite);
    bytestowrite += 2;

    //发送者ip
    quint32 ip = m_socktcp->localAddress().toIPv4Address();
    qToBigEndian<quint32>(ip, sendbuf + bytestowrite);
    bytestowrite += 4;

    if (send->msg_type == CREATE_MEETING || send->msg_type == AUDIO_SEND || send->msg_type == CLOSE_CAMERA || send->msg_type == IMG_SEND || send->msg_type == TEXT_SEND) //创建会议,发送音频,关闭摄像头，发送图片
    {
        //发送数据大小
        qToBigEndian<quint32>(send->len, sendbuf + bytestowrite);
        bytestowrite += 4;
    }
    else if (send->msg_type == JOIN_MEETING)
    {
        qToBigEndian<quint32>(send->len, sendbuf + bytestowrite);
        bytestowrite += 4;
        uint32_t room;
        memcpy(&room, send->data, send->len);
        qToBigEndian<quint32>(room, send->data);
    }

    //将数据拷入sendbuf
    memcpy(sendbuf + bytestowrite, send->data, send->len);
    bytestowrite += send->len;
    sendbuf[bytestowrite++] = '#'; //结尾字符

    //----------------write to server-------------------------

    qint64 needWrite = bytestowrite;
    qint64 ret = 0, haswrite = 0;
    while ((ret = m_socktcp->write((char*)sendbuf + haswrite, needWrite - haswrite)) < needWrite)
    {
       //临时错误
        if (ret == -1 && m_socktcp->error() == QAbstractSocket::TemporaryError)
        {
            ret = 0;
        }
        else if (ret == -1)
        {
            qDebug() << "sendDate network error";
            break;
        }
        haswrite += ret;
        needWrite -= ret;
    }

    m_socktcp->waitForBytesWritten();

    if(send->msg_type == TEXT_SEND)
    {
        emit sendTextOver(); //成功往内核发送文本信息
    }
    qDebug() <<"发送成功";
    //free
    if (send->data)
    {
        free(send->data);
    }
    if (send)
    {
        free(send);
    }
}

void MyTcpSocket::closeSocket()
{
    if (m_socktcp && m_socktcp->isOpen())
    {
        m_socktcp->close();
    }
}



void MyTcpSocket::recvFromSocket()
{

    qDebug() << "从socket接收数据" <<QThread::currentThread();
    /*
    *$_msgtype_ip_size_data_#
    */
    //可用的字节数
    qint64 availbytes = m_socktcp->bytesAvailable();
    if (availbytes <=0 )
    {
        return;
    }
    qint64 ret = m_socktcp->read((char *) recvbuf + hasrecvive, availbytes);
    if (ret <= 0)
    {
        qDebug() << "error or no more data";
        return;
    }
    hasrecvive += ret;

    //数据包不够
    if (hasrecvive < MSG_HEADER)
    {
        return;
    }
    else
    {
        quint32 data_size;
       //获取接收数据大小
        qFromBigEndian<quint32>(recvbuf + 7, 4, &data_size);
        if ((quint64)data_size + 1 + MSG_HEADER <= hasrecvive) //收够一个包
        {
            if (recvbuf[0] == '$' && recvbuf[MSG_HEADER + data_size] == '#') //且包结构正确
            {
                MSG_TYPE msgtype;
                uint16_t type;
                qFromBigEndian<uint16_t>(recvbuf + 1, 2, &type);
                msgtype = (MSG_TYPE)type;
                qDebug() << "recv data type: " << msgtype;
                if (msgtype == CREATE_MEETING_RESPONSE || msgtype == JOIN_MEETING_RESPONSE || msgtype == PARTNER_JOIN2)
                {
                    if (msgtype == CREATE_MEETING_RESPONSE)
                    {
                        qDebug() <<"接收到房间信息";
                        qint32 roomNo;//房间编号
                       //接收缓冲区中的消息头部位置（MSG_HEADER)读取4个字节的数据后将其转换为主机字节序后存入roomNo变量中
                        qFromBigEndian<qint32>(recvbuf + MSG_HEADER, 4, &roomNo);

                        MESG* msg = (MESG*)malloc(sizeof(MESG));

                        if (msg == NULL)
                        {
                            qDebug() << __LINE__ << " CREATE_MEETING_RESPONSE malloc MESG failed";
                        }
                        else
                        {
                            memset(msg, 0, sizeof(MESG));
                            msg->msg_type = msgtype;
                            msg->data = (uchar*)malloc((quint64)data_size);
                            if (msg->data == NULL)
                            {
                                free(msg);
                                qDebug() << __LINE__ << "CREATE_MEETING_RESPONSE malloc MESG.data failed";
                            }
                            else
                            {
                                memset(msg->data, 0, (quint64)data_size);
                                memcpy(msg->data, &roomNo, data_size);
                                msg->len = data_size;
                                queue_recv.push_msg(msg);
                            }

                        }
                    }
                    else if (msgtype == JOIN_MEETING_RESPONSE)
                    {
                        qint32 c;
                        memcpy(&c, recvbuf + MSG_HEADER, data_size);

                        MESG* msg = (MESG*)malloc(sizeof(MESG));

                        if (msg == NULL)
                        {
                            qDebug() << __LINE__ << "JOIN_MEETING_RESPONSE malloc MESG failed";
                        }
                        else
                        {
                            memset(msg, 0, sizeof(MESG));
                            msg->msg_type = msgtype;
                            msg->data = (uchar*)malloc(data_size);
                            if (msg->data == NULL)
                            {
                                free(msg);
                                qDebug() << __LINE__ << "JOIN_MEETING_RESPONSE malloc MESG.data failed";
                            }
                            else
                            {
                                memset(msg->data, 0, data_size);
                                memcpy(msg->data, &c, data_size);

                                msg->len = data_size;
                                queue_recv.push_msg(msg);
                            }
                        }
                    }
                    else if (msgtype == PARTNER_JOIN2)
                    {
                        MESG* msg = (MESG*)malloc(sizeof(MESG));
                        if (msg == NULL)
                        {
                            qDebug() << "PARTNER_JOIN2 malloc MESG error";
                        }
                        else
                        {
                            memset(msg, 0, sizeof(MESG));
                            msg->msg_type = msgtype;
                            msg->len = data_size;
                            msg->data = (uchar*)malloc(data_size);
                            if (msg->data == NULL)
                            {
                                free(msg);
                                qDebug() << "PARTNER_JOIN2 malloc MESG.data error";
                            }
                            else
                            {
                                memset(msg->data, 0, data_size);
                                uint32_t ip;
                                int pos = 0;
                                //遍历整个数据大小（data_size）并以每个uint32_t（4个字节）为单位进行迭代
                                for (int i = 0; i < data_size / sizeof(uint32_t); i++)
                                {
                                    //获取recvbuf中的ip
                                    qFromBigEndian<uint32_t>(recvbuf + MSG_HEADER + pos, sizeof(uint32_t), &ip);
                                    memcpy_s(msg->data + pos, data_size - pos, &ip, sizeof(uint32_t));
                                    pos += sizeof(uint32_t);
                                }
                                queue_recv.push_msg(msg);
                            }

                        }
                    }
                }
                else if (msgtype == IMG_RECV || msgtype == PARTNER_JOIN || msgtype == PARTNER_EXIT || msgtype == AUDIO_RECV || msgtype == CLOSE_CAMERA || msgtype == TEXT_RECV)
                {
                    //read ipv4
                    quint32 ip;
                    qFromBigEndian<quint32>(recvbuf + 3, 4, &ip);

                    if (msgtype == IMG_RECV)
                    {
                        //QString ss = QString::fromLatin1((char *)recvbuf + MSG_HEADER, data_len);
                        QByteArray cc((char *) recvbuf + MSG_HEADER, data_size);
                        //进行base64解码，将解码后的结果存储在rc中
                        QByteArray rc = QByteArray::fromBase64(cc);
                        QByteArray rdc = qUncompress(rc);//对rc解压缩
                        //将消息加入到接收队列
                        //qDebug() << roomNo;

                        if (rdc.size() > 0)
                        {
                            MESG* msg = (MESG*)malloc(sizeof(MESG));
                            if (msg == NULL)
                            {
                                qDebug() << __LINE__ << " malloc failed";
                            }
                            else
                            {
                                memset(msg, 0, sizeof(MESG));
                                msg->msg_type = msgtype;
                                msg->data = (uchar*)malloc(rdc.size()); // 10 = format + width + width
                                if (msg->data == NULL)
                                {
                                    free(msg);
                                    qDebug() << __LINE__ << " malloc failed";
                                }
                                else
                                {
                                    memset(msg->data, 0, rdc.size());
                                    memcpy_s(msg->data, rdc.size(), rdc.data(), rdc.size());
                                    msg->len = rdc.size();
                                    msg->ip = ip;
                                    queue_recv.push_msg(msg);
                                }
                            }
                        }
                    }
                    else if (msgtype == PARTNER_JOIN || msgtype == PARTNER_EXIT || msgtype == CLOSE_CAMERA)
                    {
                        MESG* msg = (MESG*)malloc(sizeof(MESG));
                        if (msg == NULL)
                        {
                            qDebug() << __LINE__ << " malloc failed";
                        }
                        else
                        {
                            memset(msg, 0, sizeof(MESG));
                            msg->msg_type = msgtype;
                            msg->ip = ip;
                            queue_recv.push_msg(msg);
                        }
                    }
                    else if (msgtype == AUDIO_RECV)
                    {
                        //解压缩
                        QByteArray cc((char*)recvbuf + MSG_HEADER, data_size);
                        QByteArray rc = QByteArray::fromBase64(cc);
                        QByteArray rdc = qUncompress(rc);

                        if (rdc.size() > 0)
                        {
                            MESG* msg = (MESG*)malloc(sizeof(MESG));
                            if (msg == NULL)
                            {
                                qDebug() << __LINE__ << "malloc failed";
                            }
                            else
                            {
                                memset(msg, 0, sizeof(MESG));
                                msg->msg_type = AUDIO_RECV;
                                msg->ip = ip;

                                msg->data = (uchar*)malloc(rdc.size());
                                if (msg->data == nullptr)
                                {
                                    free(msg);
                                    qDebug() << __LINE__ << "malloc msg.data failed";
                                }
                                else
                                {
                                    memset(msg->data, 0, rdc.size());
                                    memcpy_s(msg->data, rdc.size(), rdc.data(), rdc.size());
                                    msg->len = rdc.size();
                                    msg->ip = ip;
                                    audio_recv.push_msg(msg);
                                }
                            }
                        }
                    }
                    else if(msgtype == TEXT_RECV)
                    {
                        //解压缩
                        QByteArray cc((char *)recvbuf + MSG_HEADER, data_size);
                        std::string rr = qUncompress(cc).toStdString();
                        if(rr.size() > 0)
                        {
                            MESG* msg = (MESG*)malloc(sizeof(MESG));
                            if (msg == NULL)
                            {
                                qDebug() << __LINE__ << "malloc failed";
                            }
                            else
                            {
                                memset(msg, 0, sizeof(MESG));
                                msg->msg_type = TEXT_RECV;
                                msg->ip = ip;
                                msg->data = (uchar*)malloc(rr.size());
                                if (msg->data == nullptr)
                                {
                                    free(msg);
                                    qDebug() << __LINE__ << "malloc msg.data failed";
                                }
                                else
                                {
                                    memset(msg->data, 0, rr.size());
                                    memcpy_s(msg->data, rr.size(), rr.data(), rr.size());
                                    msg->len = rr.size();
                                    queue_recv.push_msg(msg);
                                }
                            }
                        }
                    }
                }
            }
            else //丢包
            {
                qDebug() << "package error";
            }
            /*接收缓冲区中的数据向前移动，覆盖已处理的消息
             * 移动的起始位置为recvbuf + MSG_HEADER + data_size + 1，移动的长度为hasrecvive - ((quint64)data_size + 1 + MSG_HEADER)
            */
            memmove_s(recvbuf, 4 * MB, recvbuf + MSG_HEADER + data_size + 1, hasrecvive - ((quint64)data_size + 1 + MSG_HEADER));
            hasrecvive -= ((quint64)data_size + 1 + MSG_HEADER);//更新
        }
        else
        {
            return;
        }
    }
}




void MyTcpSocket::stopImmediately()
{
    {
        QMutexLocker lock(&m_lock);
        if(m_isCanRun == true) m_isCanRun = false;
    }
    //关闭read
    m_sockThread->quit();
    m_sockThread->wait();
}



void MyTcpSocket::errorDetect(QAbstractSocket::SocketError error)
{
    qDebug() <<"Sock error" <<QThread::currentThreadId();
    MESG * msg = (MESG *) malloc(sizeof (MESG));
    if (msg == NULL)
    {
        qDebug() << "errorDetect malloc error";
    }
    else
    {
        memset(msg, 0, sizeof(MESG));
        if (error == QAbstractSocket::RemoteHostClosedError)
        {
            //远程主机关闭连接
            msg->msg_type = RemoteHostClosedError;
        }
        else
        {
            msg->msg_type = OtherNetError;
        }
        queue_recv.push_msg(msg);
    }
}
