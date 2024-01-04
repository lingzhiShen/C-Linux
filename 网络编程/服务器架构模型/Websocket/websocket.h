#ifndef _WEB_SOCKET_
#define _WEB_SOCKET_

#include <stdint.h>

#define _LITTLE_ENDIAN

#define BUFFER_LENGTH       4096
#define MAX_EPOLL_EVENTS    1024
#define PORT_COUNT          1

#define SERVER_PORT         8888

#define WEBSOCK_GUID        "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

#define WEBSOCK_KEY         "Sec-WebSocket-Key: "
#define WEBSOCK_KEY_LENGTH	19
#define MASKING_KEY_COUNT   4

uint64_t htonll(uint64_t val);
uint64_t ntohll(uint64_t val);
void print_hex(const unsigned char* buf, int len);

enum {
    WS_HANDSHARK=0,
    WS_TRANSMISSION=1,
    WS_END=2,
    WS_STATUS_COUNT=3
};

typedef struct _ws_ophdr {
#ifdef _LITTLE_ENDIAN
    unsigned char opcode:4,
                    rsv3:1,
                    rsv2:1,
                    rsv1:1,
                    fin:1;

    unsigned char payload_len:7,
                    mask:1;
#elif defined _BIG_ENDIAN
    unsigned char fin:1,
                    rsv1:1,
                    rsv2:1,
                    rsv3:1,
                    opcode:4;

    unsigned char mask:1,
                    payload_len:7;
#endif
} ws_ophdr;

typedef struct _ws_head {
    char mask_key[MASKING_KEY_COUNT];
}__attribute__((packed)) ws_head;

typedef struct _ws_head_126 {
    unsigned short ex_payload_len;
    char mask_key[MASKING_KEY_COUNT];
}__attribute__((packed)) ws_head_126;

typedef struct _ws_head_127 {
    long long ex_payload_len;
    char mask_key[MASKING_KEY_COUNT];
}__attribute__((packed)) ws_head_127;

typedef int (*NCALLBACK)(int, int, void *);

struct ntyevent{
    int fd;
    int events;
    void *arg;
    int (*callback)(int fd, int events, void *arg);

    int status;
    char buffer[BUFFER_LENGTH];
    int length;

    char wbuffer[BUFFER_LENGTH];
    int wlength;

    //web socket
    int status_machine;
};

struct eventblock{
    struct ntyevent *events;
    struct eventblock *next;
};

struct ntyreactor{
    int epfd;
    struct eventblock *evblk;
    int blkcnt;
};

int init_sock(int port);

int accept_cb(int fd, int events, void *arg);
int recv_cb(int fd, int events, void *arg);
int send_cb(int fd, int events, void *arg);

int nty_event_add(int epfd, int events, struct ntyevent *ev);
int nty_event_del(int epfd, struct ntyevent *ev);
void nty_event_set(struct ntyevent *ev, int fd, NCALLBACK callback, void *arg);

int ntyreactor_init(struct ntyreactor* reactor);
int ntyreactor_alloc(struct ntyreactor* reactor);
struct ntyevent* ntyreactor_idx(struct ntyreactor* reactor, int sockfd);
int ntyreactor_addlistener(struct ntyreactor *reactor, int sockfd, NCALLBACK acceptor);
int ntyreactor_run(struct ntyreactor *reactor);
int ntyreactor_destory(struct ntyreactor *reactor);

int base64_encode(char *in_str, int in_len, char *out_str);

int websocket_request(struct ntyevent *ev);
int websocket_response(struct ntyevent *ev);
int websocket_handshark(struct ntyevent *ev);
int websocket_transmission(struct ntyevent *ev);
int websocket_encode(struct ntyevent *ev);
void websocket_umask(char *payload, long long payload_len, char *masking_key);

int websocket_business(struct ntyevent *ev);

#endif // _WEB_SOCKET_
