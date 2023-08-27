#include <errno.h>
#include "unp.h"
#include <stdarg.h>

//：一个标志（errnoflag），一个错误号（error），一个格式化字符串（fmt），以及一个变长参数列表（va_list ap）。
static void err_doit(int errnoflag, int error, const char *fmt, va_list ap)
{
    char buf[MAXLINE];//声明一个字符数组 buf，用于存储生成的错误信息
   //格式化字符串 fmt 和变长参数列表 ap 格式化成一个字符串，并将结果存储在 buf 中。MAXLINE - 1 是为了在 buf 末尾留出一个空间来放置字符串结尾的空字符。
    vsnprintf(buf, MAXLINE - 1, fmt, ap);

    if(errnoflag)
    {
        //buf 的末尾追加一个字符串，包含一个冒号、一个空格，以及通过 strerror 函数获取的与错误号相对应的错误描述字符串
        snprintf(buf + strlen(buf), MAXLINE - 1 - strlen(buf), ": %s", strerror(error));
    }
    strcat(buf, "\n");//buf末尾加上换行符
    fflush(stdout);
    fputs(buf, stderr);//buf 中的错误信息输出到标准错误流（stderr）。
    fflush(NULL);
}

/*
 * fatal error unrelated to a system call
 * print a message and terminate *
*/

void err_quit(const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);//：初始化 ap，使其指向可变参数列表的开始位置
    err_doit(1, errno, fmt, ap);
    va_end(ap);
    exit(1);
}

void err_msg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    err_doit(1, errno, fmt, ap);
    va_end(ap);
}
