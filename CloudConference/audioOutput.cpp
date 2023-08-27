#include "audioOutput.h"
#include <QMutexLocker>
#include "netheader.h"
#include <QDebug>
#include <QHostAddress>
#include <QAudioFormat>
#include <QMediaDevices>


#ifndef FRAME_LEN_125MS
#define FRAME_LEN_125MS 1900 //音频帧的长度
#endif
extern QUEUE_DATA<MESG> audio_recv; //音频接收队列

AudioOutput::AudioOutput(QObject *parent)
    : QThread{parent}
{
    QAudioFormat format;
    //set format
    format.setSampleRate(48000);//采样率
    format.setChannelCount(2);//声道数 双声道
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice info(QMediaDevices::defaultAudioOutput());
    if (!info.isFormatSupported(format)) {
        qWarning() << "Raw audio format not supported by backend, cannot play audio.";
        return;
    }

    audio = new QAudioSink(format, this);
    connect(audio, SIGNAL(stateChanged(QAudio::State)), this, SLOT(handleStateChanged(QAudio::State)));
    outputDevice = nullptr;
}


AudioOutput::~AudioOutput()
{
    delete audio;
}


void AudioOutput::stopImmediately()
{
    QMutexLocker lock(&m_lock);
    is_canRun = false;
}


void AudioOutput::startPlay()
{
    if (audio->state() == QAudio::ActiveState) return;
    WRITE_LOG("start playing audio");
    outputDevice = audio->start();
}


void AudioOutput::stopPlay()
{
    if (audio->state() == QAudio::StoppedState) return;
    {
        QMutexLocker lock(&device_lock);
        outputDevice = nullptr;
    }
    audio->stop();
    WRITE_LOG("stop playing audio");
}


QString AudioOutput::errorString()
{
    if (audio->error() == QAudio::OpenError)
    {
        return QString("AudioOutput An error occurred opening the audio device").toUtf8();
    }
    else if (audio->error() == QAudio::IOError)
    {
        return QString("AudioOutput An error occurred during read/write of audio device").toUtf8();
    }
    else if (audio->error() == QAudio::UnderrunError)
    {
        return QString("AudioOutput Audio data is not being fed to the audio device at a fast enough rate").toUtf8();
    }
    else if (audio->error() == QAudio::FatalError)
    {
        return QString("AudioOutput A non-recoverable error has occurred, the audio device is not usable at this time.");
    }
    else
    {
        return QString("AudioOutput No errors have occurred").toUtf8();
    }
}


void AudioOutput::handleStateChanged(QAudio::State state)
{
    switch (state)
    {
    case QAudio::ActiveState:
        break;
    case QAudio::SuspendedState:
        break;
    case QAudio::StoppedState:
        if (audio->error() != QAudio::NoError)
        {
            audio->stop();
            emit audioOutputError(errorString());
            qDebug() << "out audio error" << audio->error();
        }
        break;
    case QAudio::IdleState:
        break;
    default:
        break;
    }
}

//在循环中不断接收音频消息，并追加到缓冲区中,在缓冲区存到音频帧的长度后写入到输出设备中进行播放.同时支持停止播放的控制.
void AudioOutput::run()
{
    is_canRun = true;
    QByteArray m_pcmDataBuffer;

    WRITE_LOG("start playing audio thread 0x%p", QThread::currentThreadId());
    while(1)
    {
        {
            QMutexLocker lock(&m_lock);
            if (is_canRun == false)
            {
                stopPlay();
                WRITE_LOG("stop playing audio thread 0x%p", QThread::currentThreadId());
                return;
            }
        }
        MESG* msg = audio_recv.pop_msg();//解析音频队列中的数据
        if (msg == NULL) continue;
        {
            QMutexLocker lock(&device_lock);
            if (outputDevice != nullptr)
            {
                m_pcmDataBuffer.append((char*)msg->data, msg->len);//添加数据

                if (m_pcmDataBuffer.size() >= FRAME_LEN_125MS)
                {
                    //写入音频数据
                    qint64 ret = outputDevice->write(m_pcmDataBuffer.data(), FRAME_LEN_125MS);
                    if (ret < 0)
                    {
                        qDebug() << outputDevice->errorString();
                        return;
                    }
                    else
                    {
                        emit speaker(QHostAddress(msg->ip).toString());
                        //截取掉已写入的数据部分 更新m_pcmDataBuffer的内容
                        m_pcmDataBuffer = m_pcmDataBuffer.right(m_pcmDataBuffer.size() - ret);
                    }
                }
            }
            else
            {
                m_pcmDataBuffer.clear();
            }
        }
        if (msg->data) free(msg->data);
        if (msg) free(msg);
    }
}


void AudioOutput::setVolumn(int val)
{
    audio->setVolume(val / 100.0);
}

void AudioOutput::clearQueue()
{
    qDebug() << "audio recv clear";
    audio_recv.clear();
}
