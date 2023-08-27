#include "unp.h"
/*
 * get peer ipv4 (network order)
 */
uint32_t getpeerip(int fd)//获得匹配的ip
{
    sockaddr_in peeraddr;
    socklen_t addrlen;
    if(getpeername(fd, (sockaddr *)&peeraddr, &addrlen) < 0)
    {
        err_msg("getpeername error");
        return -1;
    }
    return peeraddr.sin_addr.s_addr;
}


int Select(int nfds, fd_set * readfds, fd_set * writefds, fd_set * execpfds, struct timeval *timeout)
{
    int n;
    while(1)
    {
        if((n = select(nfds, readfds, writefds, execpfds, timeout)) < 0) 
        {
            if(errno == EINTR) continue;
            else err_quit("select error");
        }
        else break;
    }
    return n; //can return 0 on timeout
}


ssize_t	Readn(int fd, void * buf, size_t size)
{
    ssize_t leftToRead = size, hasRead = 0;
    char *ptr = (char *)buf;
    while(leftToRead > 0)
    {
        if((hasRead = read(fd, ptr, leftToRead))<0)
        {
            if(errno == EINTR)
            {
                hasRead = 0;
            }
            else
            {
                return -1;
            }
        }
        else if(hasRead == 0) //eof
        {
            break;
        }
        leftToRead -= hasRead;
        ptr += hasRead;
    }
    return size - leftToRead;
}

ssize_t writen(int fd, const void * buf, size_t n)
{
    ssize_t leftToWrite = n, hasWrite = 0;
    char *ptr = (char *)buf;
    while(leftToWrite > 0)
    {
        if((hasWrite = write(fd, ptr, leftToWrite)) < 0)
        {
            if(hasWrite < 0 && errno == EINTR)
            {
                hasWrite = 0;
            }
            else
            {
                return -1; //error
            }
        }
        leftToWrite -= hasWrite;
        ptr += hasWrite;
    }
    return n;
}



char *Sock_ntop(char * str, int size ,const sockaddr * sa, socklen_t salen)
{
    switch (sa->sa_family)
    {
    case AF_INET:
        {
            struct sockaddr_in *sin = (struct sockaddr_in *) sa;
            if(inet_ntop(AF_INET, &sin->sin_addr, str, size) == NULL)
            {
                err_msg("inet_ntop error");
                return NULL;
            }
            if(ntohs(sin->sin_port) > 0)
            {
                snprintf(str + strlen(str), size  -  strlen(str), ":%d", ntohs(sin->sin_port));
            }
            return str;
        }
    case AF_INET6:
        {
            struct sockaddr_in6 *sin = (struct sockaddr_in6 *) sa;
            if(inet_ntop(AF_INET6, &sin->sin6_addr, str, size) == NULL)
            {
                err_msg("inet_ntop error");
                return NULL;
            }
            if(ntohs(sin->sin6_port) > 0)
            {
                snprintf(str + strlen(str), size -  strlen(str), ":%d", ntohs(sin->sin6_port));
            }
            return str;
        }
     default:
        return "server error";
    }
    return NULL;
}


void Setsockopt(int fd, int level, int optname, const void * optval, socklen_t optlen)
{
    if(setsockopt(fd, level, optname, optval, optlen) < 0)
    {
        err_msg("setsockopt error");
    }
}

void Close(int fd)
{
    if(close(fd) < 0)
    {
        err_msg("Close error");
    }
}

void Listen(int fd, int backlog)
{
    if(listen(fd, backlog) < 0)
    {
        err_quit("listen error");
    }
}

int	Tcp_connect(const char * host, const char * serv)
{
    int sockfd, n;
    struct addrinfo hints, *res, *ressave;
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    //获取与目标主机连接所需的地址信息，并将结果保存在res中
    if((n = getaddrinfo(host, serv, &hints, &res)) != 0)
    {
        err_quit("tcp_connect error for %s, %s: %s", host, serv, gai_strerror(n));
    }

    ressave = res;
    do
    {
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if(sockfd < 0) continue;
        if(connect(sockfd, res->ai_addr, res->ai_addrlen) == 0) break;
        close(sockfd);
    }while((res = res->ai_next) != NULL);

    if(res == NULL)
    {
        err_quit("tcp_connect error for %s,%s", host, serv);
    }
    //释放占用的内存
    freeaddrinfo(ressave);
    return sockfd;
}

int Tcp_listen(const char * host, const char * service, socklen_t * addrlen)
{
    int listenfd, n;
    const int on = 1;
    struct addrinfo hints, *res, *ressave;
    bzero(&hints, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE; //设置了AI_PASSIVE标志，但没有指定主机名，那么return ipv6和ipv4通配地址
    hints.ai_family = AF_UNSPEC; //返回的是适用于指定主机名和服务名且适合任意协议族的地址
    hints.ai_socktype = SOCK_STREAM;

    char addr[MAXSOCKADDR];

    if((n = getaddrinfo(host, service, &hints, &res)) > 0)
    {
        err_quit("tcp listen error for %s %s: %s", host, service, gai_strerror(n));
    }
	ressave = res;
	do
	{
		listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if(listenfd < 0)
		{
			continue; //error try again
		}
        Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        if(bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
        {
            printf("server address: %s\n", Sock_ntop(addr, MAXSOCKADDR, res->ai_addr, res->ai_addrlen));
            break; //success
        }
        Close(listenfd);
	}while((res = res->ai_next) != NULL);
    freeaddrinfo(ressave); //free

    if(res == NULL)
    {
        err_quit("tcp listen error for %s: %s", host, service);
    }

    Listen(listenfd, LISTENQ);

    if(addrlen)
    {
        *addrlen = res->ai_addrlen;
    }

    return listenfd;
}

int Accept(int listenfd, SA * addr, socklen_t *addrlen)
{
    int n;
   while(1)
    {
        if((n = accept(listenfd, addr, addrlen)) < 0)
        {
            if(errno == EINTR)
                continue;
            else
                err_quit("accept error");
        }
        else
        {
            return n;
        }
    }
}

void Socketpair(int family, int type, int protocol, int * sockfd)
{
    if(socketpair(family, type, protocol, sockfd) < 0)
    {
        err_quit("socketpair error");
    }
}
//进行进程间通信的函数，用于向指定文件描述符fd写入数据并传递文件描述符sendfd
ssize_t write_fd(int fd, void *ptr, size_t nbytes, int sendfd)
{
    // 创建消息头结构体和IO向量结构体数组
    struct msghdr msg;
    struct iovec iov[1];
    //创建联合体，用于存储控制消息头结构体cmsghdr或者用于存储控制数据的空间
    union{
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    }control_un;
    struct cmsghdr *cmptr;

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);
    //获取控制消息头结构体的指针，并设置其属性
    cmptr = CMSG_FIRSTHDR(&msg);
    cmptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;
    *((int *) CMSG_DATA(cmptr)) = sendfd;
    // 设置消息的目标信息为空，因为这里不涉及网络通信
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    // 设置IO向量结构体数组，指定要写入的数据和长度
    iov[0].iov_base = ptr;
    iov[0].iov_len = nbytes;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    // 调用sendmsg函数将消息写入指定文件描述符，并传递控制消息
    return (sendmsg(fd, &msg, 0));
}


ssize_t Write_fd(int fd, void *ptr, size_t nbytes, int sendfd)
{
    ssize_t n;
    if((n = write_fd(fd, ptr, nbytes, sendfd)) < 0)
    {
        err_quit("write fd error");
    }
    return n;
}


ssize_t read_fd(int fd, void *ptr, size_t nbytes, int *recvfd)
{
    struct msghdr msg;
    struct iovec iov[1];
    ssize_t n;

    union {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    }control_un;

    struct cmsghdr *cmptr;
    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    //-------------------------
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    iov[0].iov_base = ptr;
    iov[0].iov_len = nbytes;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    if((n  = recvmsg(fd, &msg, MSG_WAITALL)) < 0)
    {
        return n;
    }

    if((cmptr = CMSG_FIRSTHDR(&msg)) != NULL && cmptr->cmsg_len == CMSG_LEN(sizeof(int)))
    {
        if(cmptr->cmsg_level != SOL_SOCKET)
        {
            err_msg("control level != SOL_SOCKET");
        }
        if(cmptr->cmsg_type != SCM_RIGHTS)
        {
            err_msg("control type != SCM_RIGHTS");
        }
        *recvfd = *((int *) CMSG_DATA(cmptr));
    }
    else
    {
        *recvfd = -1; // descroptor was not passed
    }

    return n;
}
