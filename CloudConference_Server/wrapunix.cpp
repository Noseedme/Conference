#include "unp.h"

void sig_chld(int signo) //signal action
{
    printf("signal\n");
    pid_t pid; // 子进程的进程ID
    int stat; // 子进程状态
    //等待子进程退出，WNOHANG 表示非阻塞等待
    while((pid = waitpid(-1, &stat, WNOHANG)) > 0)
    {
        if(WIFEXITED(stat))
        {
            printf("child %d normal termination, exit status = %d\n",pid,  WEXITSTATUS(stat));
        }
        else if(WIFSIGNALED(stat))
        {
            printf("child %d abnormal termination, singal number  = %d%s\n", pid, WTERMSIG(stat),
            #ifdef WCOREDUMP
                   WCOREDUMP(stat)? " (core file generated) " : "");
            #else
                   "");
            #endif
        }
    }
    return;
}

void * Calloc(size_t n, size_t size)//分配内存
{
    void *ptr;
    if( (ptr = calloc(n, size)) == NULL)
    {
        errno = ENOMEM;
        err_quit("Calloc error");
    }
    return ptr;
}


Sigfunc *Signal(int signo, Sigfunc * func)
{
    //act 新的信号处理配置，oact 旧的信号处理配置。
    struct sigaction act, oact;
    act.sa_handler = func;
    // 清空 act 的信号掩码，以确保在信号处理函数执行时不会被其他信号中断。
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if(signo == SIGALRM){
#ifdef SA_INTERRUPT
        act.sa_flags |= SA_INTERRUPT;
#endif
    } else {
#ifdef SA_RESTART
        act.sa_flags |= SA_RESTART;
#endif
    }
    //之前定义的 act 结构体传递给oact
    if(sigaction(signo, &act, &oact) < 0)
    {
        return SIG_ERR;
    }
    return oact.sa_handler;
}
