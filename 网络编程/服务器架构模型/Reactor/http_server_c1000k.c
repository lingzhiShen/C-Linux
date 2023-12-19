#include "http_server_c1000k.h"

struct timeval tv_begin;

int ntyreactor_init(struct ntyreactor *reactor)
{
    if (reactor == NULL)
    {
        return -1;
    }
    memset(reactor, 0, sizeof(struct ntyreactor));

    reactor->epfd = epoll_create(1);
    if (reactor->epfd <= 0) {
        return -2;
    }

    struct ntyevent *evs = (struct ntyevent *)malloc((MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));
    if (evs == NULL)
    {
        close(reactor->epfd);
        return -3;
    }
    memset(evs, 0, (MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));

    struct eventblock *block = (struct eventblock *)malloc(sizeof(struct eventblock));
    if (block == NULL) {
        free(evs);
        close(reactor->epfd);
        return -3;
    }
    block->events = evs;
    block->next = NULL;

    reactor->evblks = block;
    reactor->blkcnt = 1;

    return 0;
}

int init_sock(short port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(fd, F_SETFL, O_NONBLOCK);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    bind(fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));

    if (listen(fd, 20) < 0)
    {
        return -1;
    }

    gettimeofday(&tv_begin, NULL);

    return fd;
}

int ntyreactor_alloc(struct ntyreactor *reactor)
{
    if (reactor == NULL)
    {
        return -1;
    }
    if (reactor->evblks == NULL)
    {
        return -1;
    }

    struct eventblock *blk = reactor->evblks;

    while (blk->next != NULL)
    {
        blk = blk->next;
    }

    struct ntyevent *evs = (struct ntyevent *)malloc((MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));
    if (evs == NULL)
    {
        return -2;
    }
    memset(evs, 0, (MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));

    struct eventblock *block = (struct eventblock *)malloc(sizeof(struct eventblock));
    if (block == NULL)
    {
        return -2;
    }
    memset(block, 0, sizeof(struct eventblock));

    block->events = evs;
    block->next = NULL;
    blk->next = block;

    reactor->blkcnt++;
    return 0;
}

struct ntyevent *ntyreactor_idx(struct ntyreactor *reactor, int sockfd)
{
    if (reactor == NULL)
    {
        return NULL;
    }
    if (reactor->evblks == NULL)
    {
        return NULL;
    }

    int blkidx = sockfd / MAX_EPOLL_EVENTS;
    while (blkidx >= reactor->blkcnt)
    {
        ntyreactor_alloc(reactor);
    }

    int i = 0;
    struct eventblock *blk = reactor->evblks;
    while (i++ != blkidx && blk != NULL)
    {
        blk = blk->next;
    }

    return &blk->events[sockfd % MAX_EPOLL_EVENTS];
}

void nty_event_set(struct ntyevent *ev, int fd, NCALLBACK callback, void *arg)
{
    ev->fd = fd;
    ev->callback = callback;
    ev->events = 0;
    ev->arg = arg;
    if (ev->rbuffer == NULL)
    {
        ev->rbuffer = (char *)malloc((BUFFER_LENGTH) * sizeof(char));
        ev->rlength = 0;
    }
    if (ev->wbuffer == NULL)
    {
        ev->wbuffer = (char *)malloc((BUFFER_LENGTH) * sizeof(char));
        ev->wlength = 0;
    }

    if (ev->resource == NULL)
    {
        ev->resource = (char *)malloc((RESOURCE_LENGTH) * sizeof(char));
        memset(ev->resource, 0, (RESOURCE_LENGTH) * sizeof(char));
    }

    return ;
}

int nty_event_add(int epfd, int events, struct ntyevent *ev)
{
    struct epoll_event ep_ev = {0, {0}};
    ep_ev.data.ptr = ev;
    ep_ev.events = ev->events = events;

    int op;
    if (ev->status == 1)
    {
        op = EPOLL_CTL_MOD;
    }
    else
    {
        op = EPOLL_CTL_ADD;
        ev->status = 1;
    }

    if (epoll_ctl(epfd, op, ev->fd, &ep_ev) < 0)
    {
        return -1;
    }

    return 0;
}

int nty_event_del(int epfd, struct ntyevent *ev)
{
    struct epoll_event ep_ev = {0, {0}};

    if (ev->status != 1)
    {
        return -1;
    }

    ep_ev.data.ptr = ev;
    ev->status = 0;
    epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, &ep_ev);

    return 0;
}

int ntyreactor_addlistener(struct ntyreactor *reactor, int sockfd, NCALLBACK *acceptor)
{
    if (reactor == NULL)
    {
        return -1;
    }
    if (reactor->evblks == NULL)
    {
        return -1;
    }

    struct ntyevent *event = ntyreactor_idx(reactor, sockfd);
    if (event == NULL)
    {
        return -1;
    }

    nty_event_set(event, sockfd, acceptor, reactor);
    nty_event_add(reactor->epfd, EPOLLIN, event);

    return 0;
}

int accept_cb(int fd, int events, void *arg)
{
    struct ntyreactor *reactor = (struct ntyreactor *)arg;
    if (reactor == NULL)
    {
        return -1;
    }

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);

    int clientfd;

    if ((clientfd = accept(fd, (struct sockaddr *)&client_addr, &len)) == -1)
    {
        if (errno != EAGAIN && errno != EINTR)
        {

        }
        return -1;
    }

    int flag = 0;
    if ((flag = fcntl(clientfd, F_SETFL, O_NONBLOCK)) < 0)
    {
        return -1;
    }

    struct ntyevent *event = ntyreactor_idx(reactor, clientfd);
    if (event == NULL)
    {
        return -1;
    }

    nty_event_set(event, clientfd, recv_cb, reactor);
    nty_event_add(reactor->epfd, EPOLLIN, event);

    static int curfds = 0;
    if (curfds++ % 1000 == 999)
    {
        struct timeval tv_cur;
        memcpy(&tv_cur, &tv_begin, sizeof(struct timeval));
        gettimeofday(&tv_begin, NULL);
        int time_used = TIME_SUB_MS(tv_begin, tv_cur);
		printf("connections: %d, sockfd:%d, time_used:%d\n", curfds, clientfd, time_used);
    }
    return 0;
}

int nty_http_response_get_method(struct ntyevent *ev)
{
    int len;
    int filefd = open(ev->resource, O_RDONLY);
    if (filefd == -1)
    {
        len = sprintf(ev->wbuffer,  "HTTP/1.1 200 OK\r\n"
                                    "Accept-Ranges: bytes\r\n"
                                    "Content-Length: 78\r\n"
                                    "Content-Type: text/html\r\n"
                                    "Date: Sat, 06 Aug 2022 13:16:46 GMT\r\n\r\n"
                                    "<html><head><title>Web Server</title></head><body><h1>Lingzhi Web Server</h1><body/></html>"
                                    );
        ev->wlength = len;
    }
    else
    {
        struct stat stat_buf;
        fstat(filefd, &stat_buf);
        close(filefd);
        len = sprintf(ev->wbuffer,   "HTTP/1.1 200 OK\r\n"
                                    "Accept-Ranges: bytes\r\n"
                                    "Content-Length: %ld\r\n"
                                    "Content-Type: text/html\r\n"
                                    "Date: Sat, 06 Aug 2022 13:16:46 GMT\r\n\r\n"
                                    , stat_buf.st_size);
        ev->wlength = len;
    }

    return len;
}

int nty_http_response(struct ntyevent *ev)
{
    if (ev->method == HTTP_METHOD_GET)
    {
        return nty_http_response_get_method(ev);
    }
    else if (ev->method == HTTP_METHOD_POST)
    {
    }

    return 0;
}

int send_cb(int fd, int events, void *arg)
{
    struct ntyreactor *reactor = (struct ntyreactor *)arg;
    struct ntyevent *ev = ntyreactor_idx(reactor, fd);
    if (ev == NULL)
    {
        return -1;
    }

    nty_http_response(ev);

    int len = send(fd, ev->wbuffer, ev->wlength, 0);
    if (len > 0)
    {
        printf("resource: %s\n", ev->resource);
        int filefd = open(ev->resource, O_RDONLY);
        if (filefd >= 0)
        {
            struct stat stat_buf;
            fstat(filefd, &stat_buf);

            int flag = fcntl(fd, F_GETFL, 0);
            flag &= ~O_NONBLOCK;
            fcntl(fd, F_SETFL, flag);

            int ret = sendfile(fd, filefd, NULL, stat_buf.st_size);
            if (ret == -1)
            {
                printf("sendfile: errno: %d\n", errno);
            }

            flag |= O_NONBLOCK;
            fcntl(fd, F_SETFL, flag);

            close(filefd);
        }

        send(fd, "\r\n", 2, 0);

        nty_event_del(reactor->epfd, ev);
        nty_event_set(ev, fd, recv_cb, reactor);
        nty_event_add(reactor->epfd, EPOLLIN, ev);
    }
    else
    {
        nty_event_del(reactor->epfd, ev);
        close(ev->fd);
    }

    return len;
}

int readline(char *allbuf, int idx, char *linebuf)
{
    int len = strlen(allbuf);

    for (; idx < len; ++idx)
    {
        if (allbuf[idx] == '\r' && allbuf[idx+1] == '\n')
        {
            return idx+2;
        }
        else
        {
            *(linebuf++) = allbuf[idx];
        }
    }
    return -1;
}

int nty_http_request(struct ntyevent *ev)
{
    char linebuffer[1024] = {0};

    int idx = readline(ev->rbuffer, 0, linebuffer);
    if (strstr(linebuffer, "GET"))
    {
        ev->method = HTTP_METHOD_GET;

        int i = 0;
        while (linebuffer[sizeof("GET ")+i] != ' ')
        {
            ++i;
        }
        linebuffer[sizeof("GET ") + i] = '\0';
        sprintf(ev->resource, "%s/%s", HTTP_WEB_ROOT, linebuffer+sizeof("GET "));
    }
    else if (strstr(linebuffer, "POST"))
    {
        ev->method = HTTP_METHOD_POST;
    }

    return 0;
}

int recv_cb(int fd, int events, void *arg)
{
    struct ntyreactor *reactor = (struct ntyreactor *)arg;
    if (reactor == NULL)
    {
        return -1;
    }

    struct ntyevent *ev = ntyreactor_idx(reactor, fd);

    if (ev == NULL)
    {
        return -1;
    }

    int len = recv(fd, ev->rbuffer, BUFFER_LENGTH, 0);
    nty_event_del(reactor->epfd, ev);

    if (len > 0)
    {
        ev->rlength = len;
        ev->rbuffer[len] = '\0';

        nty_http_request(ev);

        nty_event_set(ev, fd, send_cb, reactor);
        nty_event_add(reactor->epfd, EPOLLOUT, ev);
    }
    else if (len == 0)
    {
        nty_event_del(reactor->epfd, ev);
        close(ev->fd);
    }
    else
    {
        if (errno == EAGAIN && errno == EWOULDBLOCK)
        {
            //
        }
        else if (errno == ECONNRESET)
        {
            nty_event_del(reactor->epfd, ev);
			close(ev->fd);
        }
    }

    return len;
}

int ntyreactor_run(struct ntyreactor *reactor)
{
    if (reactor == NULL)
    {
        return -1;
    }

    if (reactor->epfd < 0)
    {
        return -1;
    }

    if (reactor->evblks == NULL)
    {
        return -1;
    }

    struct epoll_event events[MAX_EPOLL_EVENTS + 1];  //

    int checkpos = 0, i;

    while (1)
    {
        int nready = epoll_wait(reactor->epfd, events, MAX_EPOLL_EVENTS, 1000);
        if (nready < 0)
        {
            printf("epoll_wait error, exit\n");
			continue;
        }

        for (i = 0; i < nready; ++i)
        {
            struct ntyevent *ev = (struct ntyevent *)events[i].data.ptr;

            if ((events[i].events & EPOLLIN) && (ev->events & EPOLLIN))
            {
                ev->callback(ev->fd, events[i].events, ev->arg);
            }
            if ((events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT))
            {
                ev->callback(ev->fd, events[i].events, ev->arg);
            }
        }
    }
}

int ntyreactor_destory(struct ntyreactor *reactor)
{
    close(reactor->epfd);
    struct eventblock *block = reactor->evblks;
    struct eventblock *block_cur;
    while (block) {
        block_cur = block;
        block = block->next;

        int i = 0;
        for (i = 0; i < MAX_EPOLL_EVENTS; ++i)
        {
            free(block_cur->events[i].rbuffer);
            free(block_cur->events[i].wbuffer);
            free(block_cur->events[i].resource);
        }
        free(block_cur->events);
        free(block_cur);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    struct ntyreactor *reactor = (struct ntyreactor *)malloc(sizeof(struct ntyreactor));
    ntyreactor_init(reactor);

    unsigned int port = SERVER_PORT;
    if (argc == 2)
    {
        port = atoi(argv[1]);
    }

    int i = 0;
    int sockfds[PORT_COUNT] = {0};

    for (i = 0; i < PORT_COUNT; ++i)
    {
        sockfds[i] = init_sock(port+i);
        ntyreactor_addlistener(reactor, sockfds[i], accept_cb);
    }

    ntyreactor_run(reactor);

    ntyreactor_destory(reactor);

    for (i = 0; i < PORT_COUNT; ++i)
    {
        close(sockfds[i]);
    }
    free(reactor);

    return 0;
}
