#include "netHeader.h"
#include "logqueue.h"
#include <QDebug>
#include <QDateTime>

QUEUE_DATA<MESG> queue_send;
QUEUE_DATA<MESG> queue_recv;
QUEUE_DATA<MESG> audio_recv;

LogQueue *logqueue = nullptr;

void log_print(const char *filename, const char *funcname, int line, const char *fmt, ...){
   Log *log = (Log *) malloc(sizeof (Log));
    if(log == nullptr)
    {
       qDebug() << "malloc log fail";
    }
    else {
       memset(log, 0, sizeof (Log));//初始化清零
       log->ptr = (char *) malloc(1 * KB);
       if(log->ptr == nullptr)
       {
           free(log);
           qDebug() << "malloc log->ptr fail";
           return;
        }
       else {
           memset(log->ptr, 0, 1 * KB);
           QDateTime currentTime = QDateTime::currentDateTime();
           QString formattedTime = currentTime.toString("yyyy-MM-dd HH:mm:ss");

           int pos = 0;
           int m = snprintf(log->ptr + pos, KB - 2 - pos, "%s ", formattedTime.toStdString().c_str());
           pos += m;

           m = snprintf(log->ptr + pos, KB - 2 - pos, "%s:%s::%d>>>", filename, funcname, line);
           pos += m;

           va_list ap;
           //取可变参数列表的起始位置
           va_start(ap, fmt);
           //格式化的日志内容字符串写入
           m = vsnprintf(log->ptr + pos, KB - 2 - pos, fmt, ap);
           pos += m;
           //结束位置
           va_end(ap);
           //追加换行符
           strcat_s(log->ptr+pos, KB-pos, "\n");
           log->len = strlen(log->ptr);
           //将写好的对象推入队列中
           logqueue->pushLog(log);
       }
    }
}
