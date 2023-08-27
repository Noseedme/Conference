#ifndef RECVSOLVE_H
#define RECVSOLVE_H

#include <QObject>
#include "netheader.h"
#include <QThread>
#include <QMutex>
/*
 * 接收线程
 * 从接收队列拿取数据
 */
class RecvSolve : public QThread
{
    Q_OBJECT
public:
    explicit RecvSolve(QObject *parent = nullptr);
    void run() override;
private:
    QMutex m_lock;
    bool m_isCanRun;
signals:
    void dataRecv(MESG *);
public slots:
    void stopImmediately();
};

#endif // RECVSOLVE_H
