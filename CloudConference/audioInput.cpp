#include "audioInput.h"
#include "netheader.h"
#include <QAudioFormat>
#include <QDebug>
#include <QThread>


extern QUEUE_DATA<MESG> queue_send;
extern QUEUE_DATA<MESG> queue_recv;

AudioInput::AudioInput(QObject *parent)
    : QObject{parent}
{
    recvBuf = (char*)malloc(MB * 2);
    QAudioFormat format;
    //set format
    format.setSampleRate(48000);//采样率
    format.setChannelCount(2);//声道数 双声道
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice info = QMediaDevices::defaultAudioInput();
    if (!info.isFormatSupported(format))
    {
        qWarning() << "Default format not supported, trying to use the nearest.";
        //format = info.nearestFormat(format);
    }
    // audio = new QAudioInput(format, this);
    audio = new QAudioSource(format, this);
    connect(audio, SIGNAL(stateChanged(QAudio::State)), this, SLOT(handleStateChanged(QAudio::State)));
}

AudioInput::~AudioInput()
{
    delete audio;
}

void AudioInput::onreadyRead()//读取recvBuf里存的数据添加到队列
{
    static int num = 0, totallen  = 0;
    if (inputDevice == nullptr) return;
    int len = inputDevice->read(recvBuf + totallen, 2 * MB - totallen);
    if (num < 2)//前两次循环
    {
        totallen += len;
        num++;
        return;
    }
    totallen += len;
    qDebug() << "totallen = " << totallen;
    MESG* msg = (MESG*)malloc(sizeof(MESG));
    if (msg == nullptr)
    {
        qWarning() << __LINE__ << "malloc fail";
    }
    else
    {
        memset(msg, 0, sizeof(MESG));
        msg->msg_type = AUDIO_SEND;
        //压缩数据，转base64
        QByteArray rr(recvBuf, totallen);
        QByteArray cc = qCompress(rr).toBase64();
        msg->len = cc.size();

        msg->data = (uchar*)malloc(msg->len);
        if (msg->data == nullptr)
        {
            qWarning() << "malloc mesg.data fail";
        }
        else
        {
            memset(msg->data, 0, msg->len);
            memcpy_s(msg->data, msg->len, cc.data(), cc.size());
            queue_send.push_msg(msg);
        }
    }
    totallen = 0;
    num = 0;
}


void AudioInput::startCollect()
{
    if (audio->state() == QAudio::ActiveState) return;
    WRITE_LOG("start collecting audio");
    inputDevice = audio->start();
    connect(inputDevice, SIGNAL(readyRead()), this, SLOT(onreadyRead()));
}

void AudioInput::stopCollect()
{
    if (audio->state() == QAudio::StoppedState) return;
    disconnect(this, SLOT(onreadyRead()));
    audio->stop();
    WRITE_LOG("stop collecting audio");
    inputDevice = nullptr;
}


void AudioInput::handleStateChanged(QAudio::State newState)
{
    switch (newState)
    {
    case QAudio::StoppedState:
        if (audio->error() != QAudio::NoError)
        {
            stopCollect();
            emit audioInputError(errorString());
        }
        else
        {
            qWarning() << "stop recording";
        }
        break;
    case QAudio::ActiveState:
        //start recording
        qWarning() << "start recording";
        break;
    default:
        break;
    }
}

QString AudioInput::errorString()
{
    if (audio->error() == QAudio::OpenError)
    {
        return QString("AudioInput An error occurred opening the audio device").toUtf8();
    }
    else if (audio->error() == QAudio::IOError)
    {
        return QString("AudioInput An error occurred during read/write of audio device").toUtf8();
    }
    else if (audio->error() == QAudio::UnderrunError)
    {
        return QString("AudioInput Audio data is not being fed to the audio device at a fast enough rate").toUtf8();
    }
    else if (audio->error() == QAudio::FatalError)
    {
        return QString("AudioInput A non-recoverable error has occurred, the audio device is not usable at this time.");
    }
    else
    {
        return QString("AudioInput No errors have occurred").toUtf8();
    }
}

void AudioInput::setVolumn(int v)
{
    qDebug() << v;
    audio->setVolume(v / 100.0);
}


