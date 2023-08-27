#ifndef NETHEADER_H
#define NETHEADER_H

//QMetaType类管理Qt的元类型系统，它将类型名称与类型关联，可以在run-time时创建、销毁。
#include <QMetaType>
#include <QMutex>//互斥锁
#include <QQueue>
#include <QImage>
#include <QWaitCondition>//条件变量


#define QUEUE_MAXSIZE 1500


#ifndef MB
#define MB 1024*1024
#endif

#ifndef KB
#define KB 1024
#endif

#ifndef WAITSECONDS
#define WAITSECONDS 2 //等待 毫秒
#endif

#ifndef OPENVIDEO
#define OPENVIDEO "打开视频"
#endif

#ifndef CLOSEVIDEO
#define CLOSEVIDEO "关闭视频"
#endif

#ifndef OPENAUDIO
#define OPENAUDIO "打开音频"
#endif

#ifndef CLOSEAUDIO
#define CLOSEAUDIO "关闭音频"
#endif

#ifndef MSG_HEADER
#define MSG_HEADER 11//报头11个字节
#endif


//枚举型是预处理指令#define的替代
//后续枚举成员的值在前一个成员上加1 可自己赋值
enum MSG_TYPE
{
    IMG_SEND = 0,
    IMG_RECV,
    AUDIO_SEND,
    AUDIO_RECV,
    TEXT_SEND,
    TEXT_RECV,
    CREATE_MEETING, //创建会议
    EXIT_MEETING,
    JOIN_MEETING,
    CLOSE_CAMERA,   //关闭摄像机

    CREATE_MEETING_RESPONSE = 20,
    PARTNER_EXIT = 21,
    PARTNER_JOIN = 22,
    JOIN_MEETING_RESPONSE = 23,
    PARTNER_JOIN2 = 24,
    RemoteHostClosedError = 40,
    OtherNetError = 41
};
Q_DECLARE_METATYPE(MSG_TYPE);

struct MESG //消息结构体
{
    MSG_TYPE msg_type;
    uchar* data;
    long len;
    quint32 ip;
};
Q_DECLARE_METATYPE(MESG *);


template<class T>
struct  QUEUE_DATA //消息队列
{
private:
    QMutex send_queueLock;
    QWaitCondition send_queueCond;
    QQueue<T*> send_queue;
public:
    void push_msg(T* msg){//添加消息队列
        send_queueLock.lock();
        //队列长度大于最大长度时阻塞线程
        while(send_queue.size() > QUEUE_MAXSIZE){
            send_queueCond.wait(&send_queueLock);
        }
        send_queue.push_back(msg);
        send_queueLock.unlock();
        send_queueCond.wakeOne();//唤醒一个线程
    }


    T* pop_msg() {//弹出队首消息
        send_queueLock.lock();
        //当队列为空时
        while(send_queue.size() == 0){
            //最多等待 2000ms
            bool f = send_queueCond.wait(&send_queueLock,WAITSECONDS * 1000);
            //超过最大等待时间
            if(f == false) {
                send_queueLock.unlock();
                return NULL;
            }
        }
        T* send = send_queue.front();
        send_queue.pop_front();
        send_queueLock.unlock();
        send_queueCond.wakeOne();
        return send;
    }

    void clear()//清理队列
    {
        send_queueLock.lock();
        send_queue.clear();
        send_queueLock.unlock();
    }

};

struct Log //日志
{
    char *ptr;
    uint len;
};


void log_print(const char *, const char *, int , const char *, ...);
#define WRITE_LOG(LOGTEXT, ...) do{\
log_print(__FILE__, __FUNCTION__, __LINE__, LOGTEXT, ##__VA_ARGS__);\
}while(0);

//使用WRITE_LOG宏时调用log_printf函数传入源文件名、函数名、行号以及可变参数

#endif // NETHEADER_H
