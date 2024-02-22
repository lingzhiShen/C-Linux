#ifndef _NETMAP_PROTOCOL_STACK_H
#define _NETMAP_PROTOCOL_STACK_H

#define ETH_ALEN 6
#define BUFFER_SIZE 1024

// eth type
#define TYPE_IP	0x0800
#define TYPE_ARP	0x0806

// ip protocol
#define PROTO_TCP   6
#define PROTO_UDP	17
#define PROTO_ICMP	1
#define PROTO_IGMP	2

#define LOCAL_MAC   "08:00:27:ff:f7:9b"
#define LOCAL_IP    "192.168.56.102"

struct eth_header {
    unsigned char d_mac[ETH_ALEN];
    unsigned char s_mac[ETH_ALEN];
    unsigned short type;
}__attribute__((packed));

struct ip_header {
    unsigned char version : 4;
    unsigned char ihl : 4;
    unsigned char tos;
    unsigned short tot_len;
    unsigned short iden;
    unsigned short flag : 3;
    unsigned short foffset : 13;
    unsigned char ttl;
    unsigned char protocol;
    unsigned short checksum;
    unsigned int s_addr;
    unsigned int d_addr;
}__attribute__((packed));

struct udp_pseudo_header {
    unsigned int s_addr;
    unsigned int d_addr;
    unsigned char all0s;
    unsigned char protocol;
    unsigned short len;
}__attribute__((packed));

struct udp_header {
    unsigned short s_port;
    unsigned short d_port;
    unsigned short len;
    unsigned short checksum;
}__attribute__((packed));

struct udp_segment {
    struct udp_header hdr;
    unsigned char data[0];
}__attribute__((packed));

struct icmp_header {
    unsigned char type;
    unsigned char code;
    unsigned short checksum;
}__attribute__((packed));

struct icmp_echo_segment {
    struct icmp_header hdr;
    unsigned short id;
    unsigned short sequence;
    unsigned char data[32];
}__attribute__((packed));

struct arp_packet {
    unsigned short hwtype;
    unsigned short prototype;
    unsigned char hwaddrlen;
    unsigned char protoaddrlen;
    unsigned short opcode;
    unsigned char s_mac[ETH_ALEN];
    unsigned int s_ip;
    unsigned char d_mac[ETH_ALEN];
    unsigned int d_ip;
}__attribute__((packed));

unsigned short in_checksum(unsigned short *addr, int len);
int arp_packet_proc(unsigned char *stream, unsigned char *rbuff, int rsize, unsigned char *mac, unsigned char *ip);
int ip_packet_proc(unsigned char *stream, unsigned char *rbuff, int rsize);
int udp_segment_proc(unsigned char *stream, unsigned char *rbuff, int rsize, unsigned int s_addr, unsigned int d_addr);
int tcp_segment_proc(unsigned char *stream);
int icmp_segment_proc(unsigned char *stream, unsigned char *rbuff, int rsize);

#endif // _NETMAP_PROTOCOL_STACK_H
