#ifndef WIDGET_H
#define WIDGET_H

#include "AudioInput.h"
#include <QWidget>
#include <QVideoFrame>
#include <QTcpSocket>
#include "mytcpsocket.h"
#include "sendtext.h"
#include "recvsolve.h"
#include "partner.h"
#include "netheader.h"
#include <QMap>
#include "AudioOutput.h"
#include "chatmessage.h"
#include <QStringListModel>

//摄像头所需头文件
#include <QCamera>
#include <QMediaDevices>
#include <QMediaCaptureSession>
#include <QMediaRecorder>
#include <QImageCapture>
#include <QVideoWidget>
#include <QVideoSink>
#include <QFile>

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class QCameraImageCapture;
//class MyVideoSurface;
class SendImg;
class QListWidgetItem;

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private:
    static QRect pos;
    quint32 mainip; //主屏幕显示的IP图像
    QCameraImageCapture *m_imageCapture; //截屏
    bool  m_createMeet; //是否创建会议
    bool m_joinMeet; // 是否加入会议
    bool m_openCamera; //是否开启摄像头

   // MyVideoSurface *m_myvideosurface;

   //QVideoFrame mainshow;

    SendImg *m_sendImg;
    QThread *m_imgThread;

    RecvSolve *m_recvThread;
    SendText * m_sendText;
    QThread *m_textThread;
    //socket
    MyTcpSocket *m_mytcpSocket;
    void paintEvent(QPaintEvent *event);

    QMap<quint32, Partner *> partner; //用于记录房间用户
    Partner* addPartner(quint32);
    void removePartner(quint32);
    void clearPartner(); //退出会议，或者会议结束
    void closeImg(quint32); //根据IP重置图像

    void dealMessage(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip ,ChatMessage::User_Type type); //用户发送文本
    void dealMessageTime(QString curMsgTime); //处理时间

    //音频
    AudioInput* m_ainput;
    QThread* m_ainputThread;
    AudioOutput* m_aoutput;

    QStringList iplist;
private:
    Ui::Widget *ui;

    QMediaCaptureSession m_captureSession;
    QVideoSink *m_videoSink;
    QCamera *m_camera;
    QFile m_file;
    int frame_num = 0;
private slots:

    void cameraError(QCamera::Error);
    void audioError(QString);
    //    void mytcperror(QAbstractSocket::SocketError);
    void dataSolve(MESG *);
    void recvip(quint32);
    void cameraImageCapture(QVideoFrame frame);

    void speaks(QString);

    //void on_sendmsg_clicked();

    void textSend();


   // void on_frame_changed(const QVideoFrame &frame); // 一帧视频到来的信号
    void on_creatMeetBtn_clicked();

    void on_quitMeetBtn_clicked();

    void on_openCameraBtn_clicked();

    void on_openMicphoneBtn_clicked();

    void on_connectBtn_clicked();

    void on_joinMeetBtn_clicked();

    void on_volumeSlider_valueChanged(int value);

    void on_sendBtn_clicked();

signals:
    void pushImg(QImage);
    void PushText(MSG_TYPE, QString = "");
    void stopAudio();
    void startAudio();
    void volumnChange(int);
};
#endif // WIDGET_H
