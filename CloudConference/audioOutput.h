#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QObject>
#include <QThread>
#include <QAudioSink>
#include <QMutex>

class AudioOutput : public QThread
{
    Q_OBJECT
public:
    explicit AudioOutput(QObject *parent = nullptr);
    ~AudioOutput();
    void stopImmediately();
    void startPlay();
    void stopPlay();

private:
    QAudioSink * audio;
    QIODevice* outputDevice;
    QMutex device_lock;

    volatile bool is_canRun;
    QMutex m_lock;
    void run();
    QString errorString();
signals:
    void audioOutputError(QString);
    void speaker(QString);
private slots:
    void handleStateChanged(QAudio::State);
    void setVolumn(int);
    void clearQueue();
};

#endif // AUDIOOUTPUT_H
