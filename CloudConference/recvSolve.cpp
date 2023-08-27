#include "recvSolve.h"
#include <QMetaType>
#include <QDebug>
#include <QMutexLocker>

extern QUEUE_DATA<MESG> queue_recv;

RecvSolve::RecvSolve(QObject *parent)
    : QThread{parent}
{
    qRegisterMetaType<MESG *>();
    m_isCanRun = true;
}


void RecvSolve::run()
{
    WRITE_LOG("start solving data thread: 0x%p", QThread::currentThreadId());
    while(1)
    {
        {
            QMutexLocker locker(&m_lock);
            if (m_isCanRun == false)
            {
                WRITE_LOG("stop solving data thread: 0x%p", QThread::currentThreadId());
                return;
            }
        }
        MESG * msg = queue_recv.pop_msg();
        if(msg == NULL) continue;
        /*else free(msg);
        qDebug() << "取出队列:" << msg->msg_type;*/
        emit dataRecv(msg);
    }
}


void RecvSolve::stopImmediately()
{
    QMutexLocker locker(&m_lock);
    m_isCanRun = false;
}
