#ifndef SENDTEXT_H
#define SENDTEXT_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include "netheader.h"

struct M
{
    QString str;
    MSG_TYPE type;

    M(QString s,MSG_TYPE e) {
        str = s;
        type = e;
    }
};

class SendText : public QThread
{
    Q_OBJECT
public:
    explicit SendText(QObject *parent = nullptr);
    ~SendText();
private:
    QQueue<M> textqueue;
    QMutex textqueue_lock; //队列锁
    QWaitCondition queue_waitCond;//队列条件变量
    void run() override;
    QMutex m_lock;
    bool m_isCanRun;

signals:
public slots:
    void push_Text(MSG_TYPE, QString str = "");
    void stopImmediately();
};

#endif // SENDTEXT_H
