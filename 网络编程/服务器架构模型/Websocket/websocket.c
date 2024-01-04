#include "websocket.h"
#include <sys/socket.h>
#include <sys/epoll.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

uint64_t htonll(uint64_t val)
{
    if (1 == htonl(1))
    {
        return val;
    }
    return (((uint64_t)htonl(val)) << 32) + htonl(val >> 32);
}

uint64_t ntohll(uint64_t val)
{
    if (1 == htonl(1))
    {
        return val;
    }
    return (((uint64_t)ntohl(val)) << 32) + ntohl(val >> 32);
}

void nty_event_set(struct ntyevent *ev, int fd, NCALLBACK callback, void *arg)
{
    ev->fd = fd;
    ev->callback = callback;
    ev->arg = arg;
    ev->events = 0;
}

int nty_event_add(int epfd, int events, struct ntyevent *ev)
{
    struct epoll_event ep_event = {0, {0}};
    ep_event.events = ev->events = events;
    ep_event.data.ptr = ev;

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

    printf("EPOLL_CTL_MOD:%d , EPOLL_CTL_ADD:%d\n", EPOLL_CTL_MOD, EPOLL_CTL_ADD);
    printf("epoll_ctl epfd:%d , op:%d, fd:%d, events:%d\n", epfd, op, ev->fd, events);
    if (epoll_ctl(epfd, op, ev->fd, &ep_event) < 0)
    {
        printf("epoll_ctl failed!\n");
        return -1;
    }
    return 0;
}

int nty_event_del(int epfd, struct ntyevent *ev)
{
    struct epoll_event ep_event = {0, {0}};

    if (ev->status != 1)
    {
        return -1;
    }

    ep_event.data.ptr = ev;
    ev->status = 0;
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, NULL) < 0)//&ep_event
    {
        printf("epoll_ctl failed ! in %s err %s\n", __func__, strerror(errno));
        return -1;
    }
    return 0;
}

int ntyreactor_init(struct ntyreactor* reactor)
{
    if (reactor == NULL)
    {
        return -1;
    }

    int epfd = epoll_create(1);
    if (epfd < 0)
    {
        printf("create epfd in %s err %s\n", __func__, strerror(errno));
        return -1;
    }

    struct ntyevent *evs = (struct ntyevent *)malloc(MAX_EPOLL_EVENTS*sizeof(struct ntyevent));
    if (evs == NULL)
    {
        return -1;
    }
    memset(evs, 0, MAX_EPOLL_EVENTS*sizeof(struct ntyevent));

    struct eventblock *block = (struct eventblock *)malloc(sizeof(struct eventblock));
    if (block == NULL)
    {
        return -1;
    }
    memset(block, 0, sizeof(struct eventblock));

    block->events = evs;
    block->next = NULL;

    reactor->blkcnt = 1;
    reactor->evblk = block;
    reactor->epfd = epfd;

    return 0;
}

int ntyreactor_alloc(struct ntyreactor* reactor)
{
    if (reactor == NULL)
    {
        return -1;
    }
    if (reactor->evblk == NULL)
    {
        return -1;
    }

    struct ntyevent *evs = (struct ntyevent *)malloc(MAX_EPOLL_EVENTS*sizeof(struct ntyevent));
    if (evs == NULL)
    {
        return -1;
    }
    memset(evs, 0, MAX_EPOLL_EVENTS*sizeof(struct ntyevent));

    struct eventblock *block = (struct eventblock *)malloc(sizeof(struct eventblock));
    if (block == NULL)
    {
        return -1;
    }
    memset(block, 0, sizeof(struct eventblock));

    block->events = evs;
    block->next = NULL;

    struct eventblock *blk = reactor->evblk;
    while (blk->next != NULL)
    {
        blk = blk->next;
    }

    blk->next = block;
    reactor->blkcnt++;
    return 0;
}

struct ntyevent* ntyreactor_idx(struct ntyreactor* reactor, int sockfd)
{
    if (reactor == NULL)
    {
        return NULL;
    }

    int blkidx = sockfd / MAX_EPOLL_EVENTS;
    while(blkidx >= reactor->blkcnt)
    {
        ntyreactor_alloc(reactor);
    }

    int i = 0;
    struct eventblock *blk = reactor->evblk;
    while(i++ < blkidx && blk)
    {
        blk = blk->next;
    }

    return &blk->events[sockfd % MAX_EPOLL_EVENTS];
}

int websocket_request(struct ntyevent *ev)
{
    if (ev->status_machine == WS_HANDSHARK)
    {
        websocket_handshark(ev);
    }
    else if (ev->status_machine == WS_TRANSMISSION)
    {
        websocket_transmission(ev);
        websocket_business(ev);
    }
    else
    {
        //
    }

    printf("websocket_request --> %d\n", ev->status_machine);
    return 0;
}

void websocket_umask(char *payload, long long payload_len, char *masking_key)
{
    long long i = 0;
    for (i = 0; i < payload_len; ++i)
    {
        payload[i] ^= masking_key[i%MASKING_KEY_COUNT];
    }
}

int websocket_transmission(struct ntyevent *ev)
{
    ws_ophdr *hdr = (ws_ophdr *)ev->buffer;
    printf("length: %d, fin: %d\n", hdr->payload_len, hdr->fin);

    unsigned char *payload = NULL;
    unsigned char *masks = NULL;
    unsigned long long len = 0;
    if (hdr->payload_len < 126)
    {
        payload = ev->buffer + sizeof(ws_ophdr) + sizeof(ws_head);
        len = hdr->payload_len;
        if (hdr->mask)
        {
            masks = ev->buffer + sizeof(ws_ophdr);
            websocket_umask(payload, len>(BUFFER_LENGTH-6)?(BUFFER_LENGTH-6):len, masks);
        }
        printf("[hdr->payload_len < 126]\n");
    }
    else if (hdr->payload_len == 126)
    {
        payload = ev->buffer + sizeof(ws_ophdr) + sizeof(ws_head_126);
        ws_head_126 *head_126 = (ws_head_126 *)(ev->buffer + sizeof(ws_ophdr));
        len = ntohs(head_126->ex_payload_len);  //做大小端转化
        if (hdr->mask)
        {
            masks = head_126->mask_key;
            websocket_umask(payload, len>(BUFFER_LENGTH-(sizeof(ws_ophdr) + sizeof(ws_head_126)))?(BUFFER_LENGTH-(sizeof(ws_ophdr) + sizeof(ws_head_126))):len, masks);
        }
        printf("[hdr->payload_len == 126]\n");
    }
    else if (hdr->payload_len == 127)
    {
        payload = ev->buffer + sizeof(ws_ophdr) + sizeof(ws_head_127);
        ws_head_127 *head_127 = (ws_head_127 *)(ev->buffer + sizeof(ws_ophdr));
        len = ntohll(head_127->ex_payload_len);
        if (hdr->mask)
        {
            masks = head_127->mask_key;
            websocket_umask(payload, len>(BUFFER_LENGTH-(sizeof(ws_ophdr) + sizeof(ws_head_127)))?(BUFFER_LENGTH-(sizeof(ws_ophdr) + sizeof(ws_head_127))):len, masks);
        }
        printf("[hdr->payload_len == 127]\n");
    }

    return 0;
}

int base64_encode(char *in_str, int in_len, char *out_str)
{
    BIO *b64, *bio;
    BUF_MEM *bptr = NULL;
    size_t size = 0;

    if (in_str == NULL || out_str == NULL)
    {
        return -1;
    }

    b64 = BIO_new(BIO_f_base64());
	bio = BIO_new(BIO_s_mem());
	bio = BIO_push(b64, bio);

	BIO_write(bio, in_str, in_len);
	BIO_flush(bio);

	BIO_get_mem_ptr(bio, &bptr);
	memcpy(out_str, bptr->data, bptr->length);
	out_str[bptr->length-1] = '\0';
	size = bptr->length;

	BIO_free_all(bio);
	return size;
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

void print_hex(const unsigned char* buf, int len)
{
    for (int i = 0; i < len; ++i)
    {
        printf("%02x", buf[i]);
    }
    printf("\n");
}

int websocket_handshark(struct ntyevent *ev)
{
    char linebuf[1024] = {0};
    int idx = 0;
    unsigned char sec_sh1[20] = {0};
    char sec_accept[32] = {0};

    printf("ws request : %s\n", ev->buffer);
    do{
        memset(linebuf, 0, 1024);
        idx = readline(ev->buffer, idx, linebuf);

        if (strstr(linebuf, WEBSOCK_KEY))
        {
            strcat(linebuf, WEBSOCK_GUID);

            SHA1(linebuf + WEBSOCK_KEY_LENGTH, strlen(linebuf + WEBSOCK_KEY_LENGTH), sec_sh1);

            base64_encode(sec_sh1, strlen(sec_sh1), sec_accept);

            memset(ev->buffer, 0, BUFFER_LENGTH);

            ev->wlength = sprintf(ev->wbuffer, "HTTP/1.1 101 Switching Protocols\r\n"
					"Upgrade: websocket\r\n"
					"Connection: Upgrade\r\n"
					"Sec-WebSocket-Accept: %s\r\n\r\n", sec_accept);

			printf("ws response : %s\n", ev->wbuffer);

			break;
        }
    }while((ev->buffer[idx] != '\r' || ev->buffer[idx+1] != '\n') && (idx != -1));

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


    /*
    ev->length = 0;
    char *pbuffer = ev->buffer;
    do {
        printf("recv\n");
        int len = recv(fd, pbuffer, BUFFER_LENGTH, 0);
        if (len < 0)
        {
            if (errno == EWOULDBLOCK)   //recv complete!
            {
                printf("recv finish !!! [fd=%d], [len=%d]\n", fd, ev->length);
                websocket_request(ev);

                nty_event_set(ev, fd, send_cb, reactor);
                nty_event_add(reactor->epfd, EPOLLOUT, ev);

                break;
            }
            if (errno == EAGAIN || errno == EINTR)
            {
                //
            }
        }
        else if (len > 0)
        {
            printf("recv[fd=%d], [len=%d]\n", fd, len);
            ev->length += len;
            pbuffer += len;
        }
        else if (len == 0)
        {
            printf("recv connection closed!\n");
            ev->status_machine = WS_END;
            nty_event_del(reactor->epfd, ev);
            close(ev->fd);
        }
    } while(1);
    */

    int len = recv(fd, ev->buffer, BUFFER_LENGTH, 0);
    if (len > 0)
    {
        printf("recv[fd=%d], [len=%d]\n", fd, len);
        ev->length = len;

        websocket_request(ev);

        printf("recv[fd=%d], [len=%d]%.*s\n", fd, ev->length, ev->length, ev->buffer);

        //nty_event_del(reactor->epfd, ev);
        nty_event_set(ev, fd, send_cb, reactor);
        nty_event_add(reactor->epfd, EPOLLOUT, ev);
    }
    else if (len == 0)
    {
        printf("recv connection closed!\n");
        ev->status_machine = WS_END;
        nty_event_del(reactor->epfd, ev);
        close(ev->fd);
    }

    return ev->length;
}

int websocket_business(struct ntyevent *ev)
{
    //memcpy(ev->wbuffer, ev->buffer, ev->length);
    //ev->wlength = ev->length;

    ev->wlength = sprintf(ev->wbuffer, "OK!");

    return 0;
}

int websocket_encode(struct ntyevent *ev)
{
    ws_ophdr hdr;
    hdr.mask = 0;
    hdr.fin = 1;
    hdr.rsv1 = hdr.rsv2 = hdr.rsv3 = 0;
    hdr.opcode = 1;

    char buffer[BUFFER_LENGTH] = {0};

    if (ev->wlength < 126)
    {
        hdr.payload_len = ev->wlength;
        memcpy(buffer, &hdr, sizeof(hdr));
        memcpy(buffer+sizeof(hdr), ev->wbuffer, ev->wlength);

        memset(ev->wbuffer, 0, BUFFER_LENGTH);
        ev->wlength += sizeof(hdr);
        memcpy(ev->wbuffer, buffer, ev->wlength);
    }
    else if (ev->wlength <= 65535)
    {
        hdr.payload_len = 126;
        unsigned short ex_payload_len = htons(ev->wlength);
        memcpy(buffer, &hdr, sizeof(hdr));
        memcpy(buffer+sizeof(hdr), &ex_payload_len, sizeof(unsigned short));
        memcpy(buffer+sizeof(hdr)+sizeof(unsigned short), ev->wbuffer, ev->wlength);

        memset(ev->wbuffer, 0, BUFFER_LENGTH);
        ev->wlength += (sizeof(hdr)+sizeof(unsigned short));
        memcpy(ev->wbuffer, buffer, ev->wlength);
    }
    else
    {
        hdr.payload_len = 127;
        unsigned long long ex_payload_len = htonll(ev->wlength);
        memcpy(buffer, &hdr, sizeof(hdr));
        memcpy(buffer+sizeof(hdr), &ex_payload_len, sizeof(unsigned long long));
        memcpy(buffer+sizeof(hdr)+sizeof(unsigned long long), ev->wbuffer, ev->wlength);

        memset(ev->wbuffer, 0, BUFFER_LENGTH);
        ev->wlength += (sizeof(hdr)+sizeof(unsigned long long));
        memcpy(ev->wbuffer, buffer, ev->wlength);
    }

    return 0;
}

int websocket_response(struct ntyevent *ev)
{
    if (ev->status_machine == WS_HANDSHARK)
    {
        ev->status_machine = WS_TRANSMISSION;
    }
    else if (ev->status_machine == WS_TRANSMISSION)
    {
        websocket_encode(ev);
    }
    else
    {
        //
    }

    return 0;
}

int send_cb(int fd, int events, void *arg)
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

    websocket_response(ev);

    int len = send(fd, ev->wbuffer, ev->wlength, 0);
    if (len > 0)
    {
        printf("send[fd=%d],[len=%d]%s\n", fd, len, ev->wbuffer);

        //nty_event_del(reactor->epfd, ev);
        nty_event_set(ev, fd, recv_cb, reactor);
        nty_event_add(reactor->epfd, EPOLLIN, ev);
    }
    else
    {
        printf("send connection closed!\n");
        ev->status_machine = WS_END;
        nty_event_del(reactor->epfd, ev);
        close(ev->fd);
    }
    return len;
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
            //
        }
        printf("accept: %s\n", strerror(errno));
		return -1;
    }

    if (fcntl(clientfd, F_SETFL, O_NONBLOCK) < 0)
    {
        printf("%s: fcntl nonblocking failed, %d\n", __func__, MAX_EPOLL_EVENTS);
		return -1;
    }

    struct ntyevent *ev = ntyreactor_idx(reactor, clientfd);
    //init websocket status
    ev->status_machine = WS_HANDSHARK;
    nty_event_set(ev, clientfd, recv_cb, reactor);
    nty_event_add(reactor->epfd, EPOLLIN, ev);

    printf("new connect [%s:%d], pos[%d]\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), clientfd);

    return 0;
}

int init_sock(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(fd, F_SETFL, O_NONBLOCK);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (listen(fd, 20) < 0)
    {
        printf("listen failed : %s\n", strerror(errno));
    }

    return fd;
}

int ntyreactor_addlistener(struct ntyreactor *reactor, int sockfd, NCALLBACK acceptor)
{
    if (reactor == NULL)
    {
        return -1;
    }
    if (reactor->evblk == NULL)
    {
        return -1;
    }

    struct ntyevent *ev = ntyreactor_idx(reactor, sockfd);

    nty_event_set(ev, sockfd, acceptor, reactor);
    nty_event_add(reactor->epfd, EPOLLIN, ev);

    return 0;
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

    if (reactor->evblk == NULL)
    {
        return -1;
    }

    struct epoll_event ep_events[MAX_EPOLL_EVENTS+1];

    while (1)
    {
        int nready = epoll_wait(reactor->epfd, ep_events, MAX_EPOLL_EVENTS, 1000);
        if (nready < 0)
        {
            printf("epoll_wait error, exit\n");
			continue;
        }

        int i = 0;
        for (i = 0; i < nready; ++i)
        {
            struct ntyevent *ev = (struct ntyevent *)ep_events[i].data.ptr;
            if ((ep_events[i].events & EPOLLIN) && (ev->events & EPOLLIN))
            {
                ev->callback(ev->fd, ep_events[i].events, reactor);
            }

            if ((ep_events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT))
            {
                ev->callback(ev->fd, ep_events[i].events, reactor);
            }
        }
    }
    return 0;
}

int ntyreactor_destory(struct ntyreactor *reactor)
{
    if (reactor == NULL)
    {
        return -1;
    }

    close(reactor->epfd);

    struct eventblock *evblk = reactor->evblk;
    struct eventblock *cur = NULL;
    while (evblk != NULL)
    {
        cur = evblk;
        evblk = evblk->next;

        if (cur->events != NULL)
        {
            free(cur->events);
        }
        free(cur);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    unsigned short port = SERVER_PORT;
    if (argc == 2)
    {
        port = atoi(argv[1]);
    }

    struct ntyreactor *reactor = (struct ntyreactor *)malloc(sizeof(struct ntyreactor));
    if (reactor == NULL)
    {
        return -1;
    }
    memset(reactor, 0, sizeof(struct ntyreactor));
    if (ntyreactor_init(reactor) < 0)
    {
        return -2;
    }

    int i;
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
