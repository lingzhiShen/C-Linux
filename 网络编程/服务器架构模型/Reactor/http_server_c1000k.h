#ifndef _HTTP_SERVER_C1000K_
#define _HTTP_SERVER_C1000K_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/stat.h>


#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include <sys/sendfile.h>

#define BUFFER_LENGTH       1024
#define RESOURCE_LENGTH     1024

#define MAX_EPOLL_EVENTS    1024

#define SERVER_PORT         8888
#define PORT_COUNT          1

#define HTTP_METHOD_GET		0
#define HTTP_METHOD_POST	1

#define HTTP_WEB_ROOT       "/usr/www"

#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

typedef int NCALLBACK(int, int, void *);

struct ntyevent {
    int fd;
    int events;
    void *arg;
    int (*callback)(int fd, int events, void *arg);

    char *rbuffer;
    int rlength;

    char *wbuffer;
    int wlength;

    int status;

    //http request
    int method;
    char *resource;
};

struct eventblock{
    struct ntyevent *events;
    struct eventblock *next;
};

struct ntyreactor {
    int epfd;

    struct eventblock *evblks;
    int blkcnt;
};

int init_sock(short port);
int readline(char *allbuf, int idx, char *linebuf);

int accept_cb(int fd, int events, void *arg);
int recv_cb(int fd, int events, void *arg);
int send_cb(int fd, int events, void *arg);
struct ntyevent *ntyreactor_idx(struct ntyreactor *reactor, int sockfd);

void nty_event_set(struct ntyevent *ev, int fd, NCALLBACK callback, void *arg);
int nty_event_add(int epfd, int events, struct ntyevent *ev);
int nty_event_del(int epfd, struct ntyevent *ev);

int nty_http_request(struct ntyevent *ev);
int nty_http_response(struct ntyevent *ev);
int nty_http_response_get_method(struct ntyevent *ev);
int ntyreactor_alloc(struct ntyreactor *reactor);
int ntyreactor_addlistener(struct ntyreactor *reactor, int sockfd, NCALLBACK *acceptor);
int ntyreactor_init(struct ntyreactor *reactor);
int ntyreactor_run(struct ntyreactor *reactor);
int ntyreactor_destory(struct ntyreactor *reactor);

#endif // _HTTP_SERVER_C1000K_
