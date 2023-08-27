#ifndef PARTNER_H
#define PARTNER_H

#include <QLabel>

class Partner : public QLabel
{
    Q_OBJECT
public:
    Partner(quint32 = 0,QWidget *parent = nullptr);
    void setpic(QImage img);

private:
    quint32 ip;

    void mousePressEvent(QMouseEvent *ev) override;
    int w;
signals:
    void sendip(quint32); //发送ip
};

#endif // PARTNER_H
