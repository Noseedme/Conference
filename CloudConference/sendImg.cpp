#include "sendImg.h"
#include "netheader.h"
#include <QDebug>
#include <cstring>
#include <QBuffer>

extern QUEUE_DATA<MESG> queue_send;

SendImg::SendImg(QObject *parant):QThread(parant)
{

}
//添加线程
void SendImg::pushToQueue(QImage img)
{
    //压缩
    QByteArray byte;
    QBuffer buf(&byte);//创建一个缓冲区
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG");
    QByteArray ss = qCompress(byte);//压缩
    QByteArray vv = ss.toBase64();
    //    qDebug() << "加入队列:" << QThread::currentThreadId();
    queue_lock.lock();
    while(imgQueue.size() > QUEUE_MAXSIZE)
    {
        queue_waitCond.wait(&queue_lock);
    }
    imgQueue.push_back(vv);
    queue_lock.unlock();
    queue_waitCond.wakeOne();
}


void SendImg::run()
{
    WRITE_LOG("start sending picture thread: 0x%p", QThread::currentThreadId());
    m_isCanRun = true;
    while (1)
    {
        queue_lock.lock(); //加锁

        while(imgQueue.size() == 0)
        {
            //qDebug() << this << QThread::currentThreadId();
            bool f = queue_waitCond.wait(&queue_lock, WAITSECONDS * 1000);
            if (f == false) //超时
            {
                QMutexLocker locker(&m_lock);
                if (m_isCanRun == false)
                {
                    queue_lock.unlock();
                    WRITE_LOG("stop sending picture thread: 0x%p", QThread::currentThreadId());
                    return;
                }
            }
        }

        QByteArray img = imgQueue.front();
        //        qDebug() << "取出队列:" << QThread::currentThreadId();
        imgQueue.pop_front();
        queue_lock.unlock();//解锁
        queue_waitCond.wakeOne(); //唤醒添加线程


        //构造消息体
        MESG* imgSend = (MESG*)malloc(sizeof(MESG));
        if (imgSend == NULL)
        {
            WRITE_LOG("malloc error");
            qDebug() << "malloc imgSend fail";
        }
        else
        {
            memset(imgSend, 0, sizeof(MESG));
            imgSend->msg_type = IMG_SEND;
            imgSend->len = img.size();
            qDebug() << "img size :" << img.size();
            imgSend->data = (uchar*)malloc(imgSend->len);
            if (imgSend->data == nullptr)
            {
                free(imgSend);
                WRITE_LOG("imgSend malloc error");
                qDebug() << "send img error";
                continue;
            }
            else
            {
                memset(imgSend->data, 0, imgSend->len);
                memcpy_s(imgSend->data, imgSend->len, img.data(), img.size());
                //加入发送队列
                queue_send.push_msg(imgSend);
            }
        }
    }
}



void SendImg::ImageCapture(QImage img) //捕获到视频帧
{
    pushToQueue(img);
}

void SendImg::clearImgQueue()
{
    qDebug() << "清空视频队列";
    QMutexLocker locker(&queue_lock);
    imgQueue.clear();
}

void SendImg::stopImmediately()
{
    QMutexLocker locker(&m_lock);
    m_isCanRun = false;
}


