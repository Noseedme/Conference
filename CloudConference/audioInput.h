#ifndef AUDIOINPUT_H
#define AUDIOINPUT_H

#include <QObject>
#include <QAudioInput>
#include <QIODevice>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QAudioSource>

class AudioInput : public QObject
{
    Q_OBJECT
public:
    explicit AudioInput(QObject *parent = nullptr);
    ~AudioInput();
private:
    QAudioSource *audio;
    QIODevice* inputDevice;
    char* recvBuf;

signals:
    void audioInputError(QString);
private slots:
    void onreadyRead();//准备开始读
    void handleStateChanged(QAudio::State);
    QString errorString();
    void setVolumn(int);//设置音量
public slots:
    void startCollect();//开始收集视频帧
    void stopCollect();
};

#endif // AUDIOINPUT_H
