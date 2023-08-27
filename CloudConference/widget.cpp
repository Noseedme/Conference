#include "widget.h"
#include "ui_widget.h"
#include "screen.h"
#include <QCamera>
#include <QDebug>
#include <QPainter>
#include "sendimg.h"
#include <QRegularExpression>//正则表达式
#include <QRegularExpressionValidator>
#include <QMessageBox>
#include <QScrollBar>
#include <QHostAddress>
#include "logqueue.h"
#include <QDateTime>
#include <QCompleter>
#include <QStringListModel>
#include <QSoundEffect>
#include <QDebug>
#include <QMediaPlayer>
//QRect  Widget::pos = QRect(-1, -1, -1, -1);

extern LogQueue *logqueue;

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    //开启日志线程
    logqueue = new LogQueue();
    logqueue->start();

    qDebug() << "main: " <<QThread::currentThread();
    qRegisterMetaType<MSG_TYPE>();

    WRITE_LOG("-------------------------Application Start---------------------------");
    WRITE_LOG("main UI thread id: 0x%p", QThread::currentThreadId());

    m_createMeet = false;
    m_openCamera = false;
    m_joinMeet = false;
   // Widget::pos = QRect(0.1 * Screen::width, 0.1 * Screen::height, 0.8 * Screen::width,0.9 * Screen::height);

    ui->setupUi(this);

    ui->openMicphoneBtn->setText(QString(OPENAUDIO).toUtf8());
    ui->openCameraBtn->setText(QString(OPENVIDEO).toUtf8());

//    this->setGeometry(Widget::pos);
//    this->setMinimumSize(QSize(Widget::pos.width() * 0.7, Widget::pos.height() * 0.7));
//    this->setMaximumSize(QSize(Widget::pos.width(), Widget::pos.height()));


    ui->quitMeetBtn->setDisabled(true);
    ui->joinMeetBtn->setDisabled(true);
    ui->creatMeetBtn->setDisabled(true);
    ui->openMicphoneBtn->setDisabled(true);
    ui->openCameraBtn->setDisabled(true);
    ui->sendBtn->setDisabled(true);
    mainip = 0; //主屏幕显示的用户IP图像

 //-------------------局部线程----------------------------
    //创建传输视频帧线程
    m_sendImg = new SendImg();
    m_imgThread = new QThread();
    m_sendImg->moveToThread(m_imgThread); //新起线程接受视频帧

    m_sendImg->start();
    //处理每一帧数据

    //--------------------------------------------------
    //数据处理（局部线程）
    m_mytcpSocket = new MyTcpSocket(); // 底层线程专管发送
    connect(m_mytcpSocket, SIGNAL(sendTextOver()), this, SLOT(textSend()));

    //----------------------------------------------------------
    //文本传输(局部线程)
    m_sendText = new SendText();
    m_textThread = new QThread();
    m_sendText->moveToThread(m_textThread);// 加入线程
    m_textThread->start();
    m_sendText->start(); // 发送

    connect(this, SIGNAL(PushText(MSG_TYPE,QString)), m_sendText, SLOT(push_Text(MSG_TYPE,QString)));
//-----------------------------------------------------------

    //摄像头
    m_camera = new QCamera(QMediaDevices::defaultVideoInput());
//    connect(m_camera, &QCamera::errorOccurred, [this](){
//        qDebug()<< "摄像头错误"<<this->m_camera->errorString();
//    });

    //取摄像头帧
    m_videoSink = ui->scrollAreaWidget->videoSink();
    QVideoFrame frame;
    frame = m_videoSink->videoFrame();

//      connect(m_videoSink, &QVideoSink::videoFrameChanged,
//              [this](const QVideoFrame &frame){
//        ui->mainshowLabel->setPixmap(QPixmap::fromImage(frame.toImage().scaled(ui->mainshowLabel->width(),ui->mainshowLabel->height(),Qt::KeepAspectRatio)));
//       });
    m_captureSession.setCamera(m_camera);
    m_captureSession.setVideoSink(m_videoSink);
    m_captureSession.setVideoOutput(ui->scrollAreaWidget);
    //connect(m_videoSink, SIGNAL(videoFrameChanged()), this, SLOT(cameraImageCapture(frame)));
    connect(m_videoSink,&QVideoSink::videoFrameChanged,this,&Widget::cameraImageCapture);
    //connect(_myvideosurface, SIGNAL(frameAvailable(QVideoFrame)), this, SLOT(cameraImageCapture(QVideoFrame)));
    connect(this, SIGNAL(pushImg(QImage)), m_sendImg, SLOT(ImageCapture(QImage)));

    //监听_imgThread退出信号
    connect(m_imgThread, SIGNAL(finished()), m_sendImg, SLOT(clearImgQueue()));


//------------------启动接收数据线程-------------------------
    m_recvThread = new RecvSolve();
    qDebug() <<"数据接收线程打开";
    connect(m_recvThread, SIGNAL(dataRecv(MESG*)), this, SLOT(dataSolve(MESG*)), Qt::BlockingQueuedConnection);
    m_recvThread->start();



  //音频
    m_ainput = new AudioInput();
    m_ainputThread = new QThread();
    m_ainput->moveToThread(m_ainputThread);

    m_aoutput = new AudioOutput();
    m_ainputThread->start(); //获取音频，发送
    m_aoutput->start(); //播放

    connect(this, SIGNAL(startAudio()), m_ainput, SLOT(startCollect()));
    connect(this, SIGNAL(stopAudio()), m_ainput, SLOT(stopCollect()));
    connect(m_ainput, SIGNAL(audioInputError(QString)), this, SLOT(audioError(QString)));
    connect(m_aoutput, SIGNAL(audioOutputError(QString)), this, SLOT(audioError(QString)));
    connect(m_aoutput, SIGNAL(speaker(QString)), this, SLOT(speaks(QString)));
    //设置滚动条
    //ui->listWidget->setStyleSheet(":/score/style.qss");

    QFont te_font = this->font();
    te_font.setFamily("MicrosoftYaHei");
    te_font.setPointSize(12);

    ui->listWidget->setFont(te_font);

    ui->tabWidget->setCurrentIndex(1);
    ui->tabWidget->setCurrentIndex(0);

    ui->IPEdit->setText("192.168.10.132");
    ui->portEdit->setText("9999");
}

Widget::~Widget()
{

    //终止底层发送与接收线程

    if(m_mytcpSocket->isRunning())
    {
        m_mytcpSocket->stopImmediately();
        m_mytcpSocket->wait();
    }

    //终止接收处理线程
    if(m_recvThread->isRunning())
    {
        m_recvThread->stopImmediately();
        m_recvThread->wait();
    }

    if(m_imgThread->isRunning())
    {
        m_imgThread->quit();
        m_imgThread->wait();
    }

    if(m_sendImg->isRunning())
    {
        m_sendImg->stopImmediately();
        m_sendImg->wait();
    }

    if(m_textThread->isRunning())
    {
        m_textThread->quit();
        m_textThread->wait();
    }

    if(m_sendText->isRunning())
    {
        m_sendText->stopImmediately();
        m_sendText->wait();
    }

    if (m_ainputThread->isRunning())
    {
        m_ainputThread->quit();
        m_ainputThread->wait();
    }

    if (m_aoutput->isRunning())
    {
        m_aoutput->stopImmediately();
        m_aoutput->wait();
    }
    WRITE_LOG("-------------------Application End-----------------");

    //关闭日志
    if(logqueue->isRunning())
    {
        logqueue->stopImmediately();
        logqueue->wait();
    }
    if(m_camera->isActive())
    {
        m_camera->stop();
    }
    delete ui;
   //m_file.close();
}


void Widget::cameraImageCapture(QVideoFrame frame)
{
    //    qDebug() << QThread::currentThreadId() << this;

    if(frame.isValid() && frame.isReadable())
    {
        QImage videoImg = QImage(frame.toImage().scaled(ui->mainshowLabel->width(),ui->mainshowLabel->height(),Qt::KeepAspectRatio));

        QTransform matrix;
        matrix.rotate(0);

        QImage img =  videoImg.transformed(matrix, Qt::FastTransformation).scaled(ui->mainshowLabel->size());

        if(partner.size() > 1)
        {
            emit pushImg(img);
        }

        if(m_mytcpSocket->getlocalip() == mainip)
        {
            ui->mainshowLabel->setPixmap(QPixmap::fromImage(img).scaled(ui->mainshowLabel->size()));
        }

        Partner *p = partner[m_mytcpSocket->getlocalip()];
        if(p) p->setpic(img);

        //qDebug()<< "format: " <<  videoImg.format() << "size: " << videoImg.size() << "byteSIze: "<< videoImg.sizeInBytes();
    }
    frame.unmap();
}



void Widget::on_creatMeetBtn_clicked()
{

    if(false == m_createMeet)
    {
        ui->creatMeetBtn->setDisabled(true);
        ui->openMicphoneBtn->setDisabled(true);
        ui->openCameraBtn->setDisabled(true);
        ui->quitMeetBtn->setDisabled(true);
        emit PushText(CREATE_MEETING); //将 “创建会议"加入到发送队列
        WRITE_LOG("create meeting");


    }
}


void Widget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    /*
     * 触发事件(3条， 一般使用第二条进行触发)
     * 1. 窗口部件第一次显示时，系统会自动产生一个绘图事件。从而强制绘制这个窗口部件，主窗口起来会绘制一次
     * 2. 当重新调整窗口部件的大小时，系统也会产生一个绘制事件--QWidget::update()或者QWidget::repaint()
     * 3. 当窗口部件被其它窗口部件遮挡，然后又再次显示出来时，就会对那些隐藏的区域产生一个绘制事件
    */
}


//退出会议（1，加入的会议， 2，自己创建的会议）
void Widget::on_quitMeetBtn_clicked()
{
    if(m_camera->isActive())
    {
        m_camera->stop();
    }

    ui->creatMeetBtn->setDisabled(false);
    ui->quitMeetBtn->setDisabled(true);
    m_createMeet = false;
    m_joinMeet = false;
    //-----------------------------------------
    //清空partner
    clearPartner();
    // 关闭套接字

    //关闭socket
    m_mytcpSocket->disconnectFromHost();
    m_mytcpSocket->wait();

    ui->outlogLabel->setText(tr("已退出会议"));

    ui->connectBtn->setDisabled(false);
    //ui->groupBox_2->setTitle(QString("主屏幕"));
    //ui->groupBox->setTitle(QString("副屏幕"));
    //清除聊天记录
    while(ui->listWidget->count() > 0)
    {
        QListWidgetItem *item = ui->listWidget->takeItem(0);
        ChatMessage *chat = (ChatMessage *) ui->listWidget->itemWidget(item);
        delete item;
        delete chat;
    }
    iplist.clear();
    ui->plainTextEdit->setCompleter(iplist);

    WRITE_LOG("exit meeting");

    QMessageBox::warning(this, "Information", "退出会议" , QMessageBox::Yes, QMessageBox::Yes);

    //-----------------------------------------
}



void Widget::on_openCameraBtn_clicked()
{

    if(m_camera->isActive())
    {
        m_camera->stop();
        WRITE_LOG("close camera");
        if(m_camera->error() == QCamera::NoError)
        {
            m_imgThread->quit();
            m_imgThread->wait();

            ui->openCameraBtn->setText("开启摄像头");
            emit PushText(CLOSE_CAMERA);
        }
        closeImg(m_mytcpSocket->getlocalip());
    }
    else
    {
        m_camera->start(); //开启摄像头
        WRITE_LOG("open camera");
        if(m_camera->error() == QCamera::NoError)
        {
            m_imgThread->start();
            ui->openCameraBtn->setText("关闭摄像头");
        }
    }
}


void Widget::on_openMicphoneBtn_clicked()
{
    if (!m_createMeet && !m_joinMeet) return;
    if (ui->openMicphoneBtn->text().toUtf8() == QString(OPENAUDIO).toUtf8())
    {
        emit startAudio();
        ui->openMicphoneBtn->setText(QString(CLOSEAUDIO).toUtf8());
    }
    else if(ui->openMicphoneBtn->text().toUtf8() == QString(CLOSEAUDIO).toUtf8())
    {
        emit stopAudio();
        ui->openMicphoneBtn->setText(QString(OPENAUDIO).toUtf8());
    }
}


void Widget::closeImg(quint32 ip)
{
    if (!partner.contains(ip))
    {
        qDebug() << "close img error";
        return;
    }
    Partner * p = partner[ip];
    p->setpic(QImage(":/score/1.png"));

    if(mainip == ip)
    {
        ui->mainshowLabel->setPixmap(QPixmap::fromImage(QImage(":/score/1.png").scaled(ui->mainshowLabel->size())));
    }
}



void Widget::on_connectBtn_clicked()
{
    QString ip = ui->IPEdit->text(), port = ui->portEdit->text();
    ui->outlogLabel->setText("正在连接到" + ip + ":" + port);
    repaint();
    //利用正则表达式对用户输入的IP地址和端口号进行验证
    QRegularExpression ipreg("((2{2}[0-3]|2[01][0-9]|1[0-9]{2}|0?[1-9][0-9]|0{0,2}[1-9])\\.)((25[0-5]|2[0-4][0-9]|[01]?[0-9]{0,2})\\.){2}(25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})");

    QRegularExpression portreg("^([0-9]|[1-9]\\d|[1-9]\\d{2}|[1-9]\\d{3}|[1-5]\\d{4}|6[0-4]\\d{3}|65[0-4]\\d{2}|655[0-2]\\d|6553[0-5])$");
    QRegularExpressionValidator ipvalidate(ipreg), portvalidate(portreg);
    int pos = 0;
    if(ipvalidate.validate(ip, pos) != QValidator::Acceptable)
    {
        QMessageBox::warning(this, "Input Error", "Ip Error", QMessageBox::Yes, QMessageBox::Yes);
        return;
    }
    if(portvalidate.validate(port, pos) != QValidator::Acceptable)
    {
        QMessageBox::warning(this, "Input Error", "Port Error", QMessageBox::Yes, QMessageBox::Yes);
        return;
    }

    if(m_mytcpSocket ->connectToServer(ip, port, QIODevice::ReadWrite))
    {
        ui->outlogLabel->setText("成功连接到" + ip + ":" + port);
        ui->openMicphoneBtn->setDisabled(false);
        ui->openCameraBtn->setDisabled(false);
        ui->creatMeetBtn->setDisabled(false);
        ui->quitMeetBtn->setDisabled(true);
        ui->joinMeetBtn->setDisabled(false);
        WRITE_LOG("succeeed connecting to %s:%s", ip.toStdString().c_str(), port.toStdString().c_str());
        QMessageBox::warning(this, "Connection success", "成功连接服务器" , QMessageBox::Yes, QMessageBox::Yes);
        ui->sendBtn->setDisabled(false);
        ui->connectBtn->setDisabled(true);
    }
    else
    {
        ui->outlogLabel->setText("连接失败,请重新连接...");
        WRITE_LOG("failed to connenct %s:%s", ip.toStdString().c_str(), port.toStdString().c_str());
        QMessageBox::warning(this, "Connection error", m_mytcpSocket->errorString() , QMessageBox::Yes, QMessageBox::Yes);
    }
}


void Widget::cameraError(QCamera::Error)
{
    QMessageBox::warning(this, "Camera error", m_camera->errorString() , QMessageBox::Yes, QMessageBox::Yes);
}


void Widget::audioError(QString err)
{
    QMessageBox::warning(this, "Audio error", err, QMessageBox::Yes);
}


void Widget::dataSolve(MESG *msg)
{
    if(msg->msg_type == CREATE_MEETING_RESPONSE)
    {
        int roomno;
        memcpy(&roomno, msg->data, msg->len);

        if(roomno != 0)
        {
            QMessageBox::information(this, "Room No", QString("房间号：%1").arg(roomno), QMessageBox::Yes, QMessageBox::Yes);

            ui->groupBox_5->setTitle(QString("主屏幕(房间号: %1)").arg(roomno));
            ui->outlogLabel->setText(QString("创建成功 房间号: %1").arg(roomno) );
            m_createMeet = true;
            ui->quitMeetBtn->setDisabled(false);
            ui->openCameraBtn->setDisabled(false);
            ui->openMicphoneBtn->setDisabled(false);
            ui->joinMeetBtn->setDisabled(true);

            WRITE_LOG("succeed creating room %d", roomno);
            //添加用户自己
            addPartner(m_mytcpSocket->getlocalip());
            mainip = m_mytcpSocket->getlocalip();
            ui->groupBox_5->setTitle(QHostAddress(mainip).toString());
            ui->mainshowLabel->setPixmap(QPixmap::fromImage(QImage(":/score/1.png").scaled(ui->mainshowLabel->size())));
        }
        else
        {
            m_createMeet = false;
            QMessageBox::information(this, "Room Information", QString("无可用房间"), QMessageBox::Yes, QMessageBox::Yes);
            ui->outlogLabel->setText(QString("无可用房间"));
            ui->creatMeetBtn->setDisabled(false);
            WRITE_LOG("no empty room");
        }
    }
    else if(msg->msg_type == JOIN_MEETING_RESPONSE)
    {
        qint32 c;
        memcpy(&c, msg->data, msg->len);
        if(c == 0)
        {
            QMessageBox::information(this, "Meeting Error", tr("会议不存在") , QMessageBox::Yes, QMessageBox::Yes);
            ui->outlogLabel->setText(QString("会议不存在"));
            WRITE_LOG("meeting not exist");
            ui->quitMeetBtn->setDisabled(true);
            ui->openCameraBtn->setDisabled(true);
            ui->joinMeetBtn->setDisabled(false);
            ui->connectBtn->setDisabled(true);
            m_joinMeet = false;
        }
        else if(c == -1)
        {
            QMessageBox::warning(this, "Meeting information", "成员已满，无法加入" , QMessageBox::Yes, QMessageBox::Yes);
            ui->outlogLabel->setText(QString("成员已满，无法加入"));
            WRITE_LOG("full room, cannot join");
        }
        else if (c > 0)
        {
            QMessageBox::warning(this, "Meeting information", "加入成功" , QMessageBox::Yes, QMessageBox::Yes);
            ui->outlogLabel->setText(QString("加入成功"));
            WRITE_LOG("succeed joining room");
            //添加用户自己
            addPartner(m_mytcpSocket->getlocalip());
            mainip = m_mytcpSocket->getlocalip();
            ui->groupBox_5->setTitle(QHostAddress(mainip).toString());
            ui->mainshowLabel->setPixmap(QPixmap::fromImage(QImage(":/score/1.png").scaled(ui->mainshowLabel->size())));
            ui->joinMeetBtn->setDisabled(true);
            ui->quitMeetBtn->setDisabled(false);
            ui->creatMeetBtn->setDisabled(true);
            m_joinMeet = true;
        }
    }
    else if(msg->msg_type == IMG_RECV)
    {
        QHostAddress a(msg->ip);
        qDebug() << a.toString();
        QImage img;
        img.loadFromData(msg->data, msg->len);
        if(partner.count(msg->ip) == 1)
        {
            Partner* p = partner[msg->ip];
            p->setpic(img);
        }
        else
        {
            Partner* p = addPartner(msg->ip);
            p->setpic(img);
        }

        if(msg->ip == mainip)
        {
            ui->mainshowLabel->setPixmap(QPixmap::fromImage(img).scaled(ui->mainshowLabel->size()));
        }
        repaint();
    }
    else if(msg->msg_type == TEXT_RECV)
    {
        QString str = QString::fromStdString(std::string((char *)msg->data, msg->len));
        //qDebug() << str;
        QString time = QString::number(QDateTime::currentSecsSinceEpoch());
        ChatMessage *message = new ChatMessage(ui->listWidget);
        QListWidgetItem *item = new QListWidgetItem();
        dealMessageTime(time);
        dealMessage(message, item, str, time, QHostAddress(msg->ip).toString() ,ChatMessage::User_She);
        if(str.contains('@' + QHostAddress(m_mytcpSocket->getlocalip()).toString()))
        {
           QSoundEffect* player = new QSoundEffect(this);
            player->setSource(QUrl(":/score/2.wav"));
        }
    }
    else if(msg->msg_type == PARTNER_JOIN)
    {
        Partner* p = addPartner(msg->ip);
        if(p)
        {
            p->setpic(QImage(":/score/4.png"));
            ui->outlogLabel->setText(QString("%1 join meeting").arg(QHostAddress(msg->ip).toString()));
            iplist.append(QString("@") + QHostAddress(msg->ip).toString());
            ui->plainTextEdit->setCompleter(iplist);
        }
    }
    else if(msg->msg_type == PARTNER_EXIT)
    {
        removePartner(msg->ip);
        if(mainip == msg->ip)
        {
            ui->mainshowLabel->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.jpg").scaled(ui->mainshowLabel->size())));
        }
        if(iplist.removeOne(QString("@") + QHostAddress(msg->ip).toString()))
        {
            ui->plainTextEdit->setCompleter(iplist);
        }
        else
        {
            qDebug() << QHostAddress(msg->ip).toString() << "not exist";
            WRITE_LOG("%s not exist",QHostAddress(msg->ip).toString().toStdString().c_str());
        }
        ui->outlogLabel->setText(QString("%1 exit meeting").arg(QHostAddress(msg->ip).toString()));
    }
    else if (msg->msg_type == CLOSE_CAMERA)
    {
        closeImg(msg->ip);
    }
    else if (msg->msg_type == PARTNER_JOIN2)
    {
        uint32_t ip;
        int other = msg->len / sizeof(uint32_t), pos = 0;
        for (int i = 0; i < other; i++)
        {
            memcpy_s(&ip, sizeof(uint32_t), msg->data + pos , sizeof(uint32_t));
            pos += sizeof(uint32_t);
            Partner* p = addPartner(ip);
            if (p)
            {
                p->setpic(QImage(":/score/4.png"));
                iplist << QString("@") + QHostAddress(ip).toString();
            }
        }
        ui->plainTextEdit->setCompleter(iplist);
        ui->openCameraBtn->setDisabled(false);
    }
    else if(msg->msg_type == RemoteHostClosedError)
    {

        clearPartner();
        m_mytcpSocket->disconnectFromHost();
        m_mytcpSocket->wait();
        ui->outlogLabel->setText(QString("关闭与服务器的连接"));
        ui->creatMeetBtn->setDisabled(true);
        ui->quitMeetBtn->setDisabled(true);
        ui->connectBtn->setDisabled(false);
        ui->joinMeetBtn->setDisabled(true);
        //清除聊天记录
        while(ui->listWidget->count() > 0)
        {
            QListWidgetItem *item = ui->listWidget->takeItem(0);
            ChatMessage *chat = (ChatMessage *)ui->listWidget->itemWidget(item);
            delete item;
            delete chat;
        }
        iplist.clear();
        ui->plainTextEdit->setCompleter(iplist);
        if(m_createMeet || m_joinMeet) QMessageBox::warning(this, "Meeting Information", "会议结束" , QMessageBox::Yes, QMessageBox::Yes);
    }
    else if(msg->msg_type == OtherNetError)
    {
        QMessageBox::warning(NULL, "Network Error", "网络异常" , QMessageBox::Yes, QMessageBox::Yes);
        clearPartner();
        m_mytcpSocket->disconnectFromHost();
        m_mytcpSocket->wait();
        ui->outlogLabel->setText(QString("网络异常......"));
    }
    if(msg->data)
    {
        free(msg->data);
        msg->data = NULL;
    }
    if(msg)
    {
        free(msg);
        msg = NULL;
    }
}


Partner* Widget::addPartner(quint32 ip)
{
    if (partner.contains(ip)) return NULL;
    Partner *p = new Partner(ip,ui->mainshowLabel);
    if (p == NULL)
    {
        qDebug() << "new Partner error";
        return NULL;
    }
    else
    {
        connect(p, SIGNAL(sendip(quint32)), this, SLOT(recvip(quint32)));
        partner.insert(ip, p);
       // ui->verticalLayout_3->addWidget(p, 1);

        //当有人员加入时，开启滑动条滑动事件，开启输入(只有自己时，不打开)
        if (partner.size() > 1)
        {
            connect(this, SIGNAL(volumnChange(int)), m_ainput, SLOT(setVolumn(int)), Qt::UniqueConnection);
            connect(this, SIGNAL(volumnChange(int)), m_aoutput, SLOT(setVolumn(int)), Qt::UniqueConnection);
            ui->openMicphoneBtn->setDisabled(false);
            ui->sendBtn->setDisabled(false);
            m_aoutput->startPlay();
        }
        return p;
    }
}


void Widget::removePartner(quint32 ip)
{
    if(partner.contains(ip))
    {
        Partner *p = partner[ip];
        disconnect(p, SIGNAL(sendip(quint32)), this, SLOT(recvip(quint32)));
        ui->verticalLayout_3->removeWidget(p);
        delete p;
        partner.remove(ip);

        //只有自已一个人时，关闭传输音频
        if (partner.size() <= 1)
        {
            disconnect(m_ainput, SLOT(setVolumn(int)));
            disconnect(m_aoutput, SLOT(setVolumn(int)));
            m_ainput->stopCollect();
            m_aoutput->stopPlay();
            ui->openMicphoneBtn->setText(QString(OPENAUDIO).toUtf8());
            ui->openMicphoneBtn->setDisabled(true);
        }
    }
}


void Widget::clearPartner()
{
    ui->mainshowLabel->setPixmap(QPixmap());
    if(partner.size() == 0) return;

    QMap<quint32, Partner*>::iterator iter =   partner.begin();
    while (iter != partner.end())
    {
        quint32 ip = iter.key();
        iter++;
        Partner *p =  partner.take(ip);
        ui->verticalLayout_3->removeWidget(p);
        delete p;
        p = nullptr;
    }

    //关闭传输音频
    disconnect(m_ainput, SLOT(setVolumn(int)));
    disconnect(m_aoutput, SLOT(setVolumn(int)));
    //关闭音频播放与采集
    m_ainput->stopCollect();
    m_aoutput->stopPlay();
    ui->openMicphoneBtn->setText(QString(CLOSEAUDIO).toUtf8());
    ui->openMicphoneBtn->setDisabled(true);


    //关闭图片传输线程
    if(m_imgThread->isRunning())
    {
        m_imgThread->quit();
        m_imgThread->wait();
    }
    ui->openCameraBtn->setText(QString(OPENVIDEO).toUtf8());
    ui->openCameraBtn->setDisabled(true);
}


void Widget::recvip(quint32 ip)
{
    if (partner.contains(mainip))
    {
        Partner* p = partner[mainip];
        p->setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(0, 0 , 255, 0.7)");
    }
    if (partner.contains(ip))
    {
        Partner* p = partner[ip];
        p->setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(255, 0 , 0, 0.7)");
    }
    ui->mainshowLabel->setPixmap(QPixmap::fromImage(QImage(":/score/1.png").scaled(ui->mainshowLabel->size())));
    mainip = ip;
    ui->groupBox_5->setTitle(QHostAddress(mainip).toString());
    qDebug() << ip;
}

//加入会议



void Widget::on_joinMeetBtn_clicked()
{
    QString roomNo = ui->joinMeetEdit->text();

    QRegularExpression roomreg("^[1-9][0-9]{1,4}$");
    QRegularExpressionValidator  roomvalidate(roomreg);
    int pos = 0;
    if(roomvalidate.validate(roomNo, pos) != QValidator::Acceptable)
    {
        QMessageBox::warning(this, "RoomNo Error", "房间号不合法" , QMessageBox::Yes, QMessageBox::Yes);
    }
    else
    {
        //加入发送队列
        emit PushText(JOIN_MEETING, roomNo);
    }
}


void Widget::on_volumeSlider_valueChanged(int value)
{
    emit volumnChange(value);
}


void Widget::speaks(QString ip)
{
    ui->outlogLabel->setText(QString(ip + " 正在说话").toUtf8());
}




void Widget::on_sendBtn_clicked()
{
    QString msg = ui->plainTextEdit->toPlainText().trimmed();
    if(msg.size() == 0)
    {
        qDebug() << "empty";
        return;
    }
    qDebug()<<msg;
    ui->plainTextEdit->setPlainText("");
    QString time = QString::number(QDateTime::currentSecsSinceEpoch());
    ChatMessage *message = new ChatMessage(ui->listWidget);
    QListWidgetItem *item = new QListWidgetItem();
    dealMessageTime(time);
    dealMessage(message, item, msg, time, QHostAddress(m_mytcpSocket->getlocalip()).toString() ,ChatMessage::User_Me);
    emit PushText(TEXT_SEND, msg);
    ui->sendBtn->setDisabled(true);
}


void Widget::dealMessage(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip ,ChatMessage::User_Type type)
{
    ui->listWidget->addItem(item);
    messageW->setFixedWidth(ui->listWidget->width());
    QSize size = messageW->fontRect(text);
    item->setSizeHint(size);
    messageW->setText(text, time, size, ip, type);
    ui->listWidget->setItemWidget(item, messageW);
}


void Widget::dealMessageTime(QString curMsgTime)
{
    bool isShowTime = false;
    if(ui->listWidget->count() > 0) {
        QListWidgetItem* lastItem = ui->listWidget->item(ui->listWidget->count() - 1);
        ChatMessage* messageW = (ChatMessage *)ui->listWidget->itemWidget(lastItem);
        int lastTime = messageW->time().toInt();
        int curTime = curMsgTime.toInt();
        qDebug() << "curTime lastTime:" << curTime - lastTime;
        isShowTime = ((curTime - lastTime) > 60); // 两个消息相差一分钟
        //        isShowTime = true;
    } else {
        isShowTime = true;
    }
    if(isShowTime) {
        ChatMessage* messageTime = new ChatMessage(ui->listWidget);
        QListWidgetItem* itemTime = new QListWidgetItem();
        ui->listWidget->addItem(itemTime);
        QSize size = QSize(ui->listWidget->width() , 40);
        messageTime->resize(size);
        itemTime->setSizeHint(size);
        messageTime->setText(curMsgTime, curMsgTime, size);
        ui->listWidget->setItemWidget(itemTime, messageTime);
    }
}

void Widget::textSend()
{
    qDebug() << "send text over";
    QListWidgetItem* lastItem = ui->listWidget->item(ui->listWidget->count() - 1);
    ChatMessage* messageW = (ChatMessage *)ui->listWidget->itemWidget(lastItem);
    messageW->setTextSuccess();
    ui->sendBtn->setDisabled(false);
}

