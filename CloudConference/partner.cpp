#include "partner.h"
#include <QDebug>
#include <QEvent>
#include <QHostAddress>

Partner::Partner(quint32 p,QWidget *parent)
    : QLabel{parent}
{
    ip = p;

    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    w = ((QWidget *)this->parent())->size().width();
    this->setPixmap(QPixmap::fromImage(QImage(":/score/1.png").scaled(w-10, w-10)));
    this->setFrameShape(QFrame::Box);

    this->setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(0, 0 , 255, 0.7)");
    //    this->setToolTipDuration(5);

    this->setToolTip(QHostAddress(ip).toString());
}

void Partner::mousePressEvent(QMouseEvent *)
{
    emit sendip(ip);
}
void Partner::setpic(QImage img)
{
    this->setPixmap(QPixmap::fromImage(img.scaled(w-10, w-10)));
}
