#ifndef LOGQUEUE_H
#define LOGQUEUE_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <queue>
#include "netheader.h"

class LogQueue : public QThread
{
public:
    // explicit 制止隐式类型转换
    explicit LogQueue(QObject *parent = nullptr);
    void stopImmediately();//立即停止
    void pushLog(Log*);
private:
    void run();
    QMutex m_lock;
    bool m_isCanRun;

    QUEUE_DATA<Log> log_queue;
    FILE *logfile;
};

#endif // LOGQUEUE_H
