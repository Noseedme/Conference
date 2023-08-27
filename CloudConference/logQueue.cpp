#include "logQueue.h"
#include <QDebug>

LogQueue::LogQueue(QObject *parent)
    : QThread{parent}
{

}

void LogQueue::stopImmediately()
{
    QMutexLocker lock(&m_lock);//自动管理互斥锁的生命周期
    m_isCanRun = false;
}

void LogQueue::pushLog(Log* log)
{
    log_queue.push_msg(log);
}

void LogQueue::run() {//将日志数据写入日志文件
    m_isCanRun = true;
    while(1){
        {
         QMutexLocker lock(&m_lock);
         if(m_isCanRun == false) {
                fclose(logfile);
                return;
            }
        }
        Log* log = log_queue.pop_msg();
        if(log == NULL || log->ptr == NULL) continue;


        //write to logfile
        errno_t open = fopen_s(&logfile,"./log.txt","a");//追加读写
        if( open != 0){
            qDebug() << "打开文件失败:" << open;
            continue;
        }


        qint64 needWrite = log->len;//需要写入的字节数
        qint64 ret = 0, hasWrite = 0;
        while((ret = fwrite( (char*)log->ptr + hasWrite, 1 ,needWrite - hasWrite, logfile)) < needWrite)
        {
            if(ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                ret = 0;
            }
            else{
                qDebug() << "写入日志文件失败";
                break;
            }
            hasWrite += ret; //更新已写入字节数和剩余待写入字节数
            needWrite -= ret;
        }

        //free
        if(log->ptr) free(log->ptr);
        if(log) free(log);

        fflush(logfile);
        fclose(logfile);
    }
}
