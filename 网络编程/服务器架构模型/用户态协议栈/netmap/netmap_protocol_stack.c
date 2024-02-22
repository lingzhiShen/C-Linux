#include "netmap_protocol_stack.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <sys/poll.h>
#include <arpa/inet.h>

#define NETMAP_WITH_LIBS
#include <net/netmap_user.h>

int str2mac(char *mac, char *str) {

	char *p = str;
	unsigned char value = 0x0;
	int i = 0;

	while (p != '\0') {

		if (*p == ':') {
			mac[i++] = value;
			value = 0x0;
		} else {

			unsigned char temp = *p;
			if (temp <= '9' && temp >= '0') {
				temp -= '0';
			} else if (temp <= 'f' && temp >= 'a') {
				temp -= 'a';
				temp += 10;
			} else if (temp <= 'F' && temp >= 'A') {
				temp -= 'A';
				temp += 10;
			} else {
				break;
			}
			value <<= 4;
			value |= temp;
		}
		p ++;
	}

	mac[i] = value;

	return 0;
}

unsigned short in_checksum(unsigned short *addr, int len)
{
    register int nleft = len;
	register unsigned short *w = addr;
	register int sum = 0;
	unsigned short answer = 0;

	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w;
		sum += answer;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer = ~sum;

	return (answer);
}

int ip_packet_proc(unsigned char *stream, unsigned char *rbuff, int rsize)
{
    int buff_size = 0;
    struct ip_header *ip = (struct ip_header *)stream;
    struct in_addr s_addr, d_addr;
	s_addr.s_addr = ip->s_addr;
	d_addr.s_addr = ip->d_addr;
    printf("%s to %s--> ", inet_ntoa(s_addr), inet_ntoa(d_addr));

    struct ip_header ip_hdr;
    memcpy(&ip_hdr, ip, sizeof(struct ip_header));
    ip_hdr.d_addr = ip->s_addr;
    ip_hdr.s_addr = ip->d_addr;
    ip_hdr.checksum = 0;
    //ip_hdr.flag = 2;

    int cpy_len = rsize < sizeof(struct ip_header) ? rsize : sizeof(struct ip_header);
    memcpy(rbuff, &ip_hdr, cpy_len);
    buff_size += cpy_len;

    stream += sizeof(struct ip_header);
    if (ip->protocol == PROTO_TCP)
    {
        //
    }
    else if (ip->protocol == PROTO_UDP)
    {
        buff_size += udp_segment_proc(stream, rbuff+cpy_len, rsize-cpy_len, ip->s_addr, ip->d_addr);
    }
    else if (ip->protocol == PROTO_ICMP)
    {
        buff_size += icmp_segment_proc(stream, rbuff+cpy_len, rsize-cpy_len);
    }

    ((struct ip_header *)rbuff)->tot_len = htons(buff_size);
    ((struct ip_header *)rbuff)->checksum = in_checksum((unsigned short *)rbuff, sizeof(struct ip_header));

    return buff_size;
}

int icmp_segment_proc(unsigned char *stream, unsigned char *rbuff, int rsize)
{
    int buff_size = 0;
    struct icmp_header *hdr = (struct icmp_header *)stream;
    if (hdr->type == 8 && hdr->code == 0)
    {
        struct icmp_echo_segment *icmp = (struct icmp_echo_segment *)stream;
        struct icmp_echo_segment icmp_seg;
        memcpy(&icmp_seg, icmp, sizeof(struct icmp_echo_segment));
        icmp_seg.hdr.type = 0;
        icmp_seg.hdr.code = 0;
        icmp_seg.hdr.checksum = 0;
        icmp_seg.id = icmp->id;
        icmp_seg.sequence = icmp->sequence;

        int icmp_len = sizeof(struct icmp_echo_segment);
        icmp_seg.hdr.checksum = in_checksum((unsigned short *)&icmp_seg, icmp_len);

        int cpy_len = rsize < icmp_len ? rsize : icmp_len;
        memcpy(rbuff, (unsigned char *)&icmp_seg, cpy_len);
        buff_size += cpy_len;
    }

    return buff_size;
}

int udp_segment_proc(unsigned char *stream, unsigned char *rbuff, int rsize, unsigned int s_addr, unsigned int d_addr)
{
    struct udp_segment *udp = (struct udp_segment *)stream;
    int udp_total_len = ntohs(udp->hdr.len);
    const udp_hdr_len = sizeof(struct udp_header);
    printf("udp(from fort : %u --> to port : %u)\n", ntohs(udp->hdr.s_port), ntohs(udp->hdr.d_port));

    udp->data[udp_total_len-udp_hdr_len] = '\0';
    printf("segment len:%d data--> %s\n", udp_total_len, udp->data);

    char buffer[] = {"from netmap --> udp reply"};

    struct udp_segment udp_seg;
    memcpy(&udp_seg, &udp->hdr, sizeof(struct udp_header));
    udp_seg.hdr.s_port = udp->hdr.d_port;
    udp_seg.hdr.d_port = udp->hdr.s_port;
    udp_seg.hdr.checksum = 0;
    udp_seg.hdr.len = htons(sizeof(buffer)+sizeof(struct udp_header));

    int cpy_len = 0;
    assert(rsize > sizeof(struct udp_header));
    memcpy(rbuff, &udp_seg, sizeof(struct udp_header));
    cpy_len += sizeof(struct udp_header);

    memcpy(rbuff+cpy_len, buffer, sizeof(buffer));
    cpy_len += sizeof(buffer);

    struct udp_pseudo_header pseudo_header;
    pseudo_header.s_addr = s_addr;
    pseudo_header.d_addr = d_addr;
    pseudo_header.all0s = 0;
    pseudo_header.protocol = PROTO_UDP;
    pseudo_header.len = udp_seg.hdr.len;

    unsigned char buf[BUFFER_SIZE] = {0};
    unsigned short buf_len = 0;
    memcpy(buf, &pseudo_header, sizeof(struct udp_pseudo_header));
    buf_len += sizeof(struct udp_pseudo_header);
    memcpy(buf+buf_len, rbuff, cpy_len);
    buf_len += cpy_len;
    ((struct udp_header *)rbuff)->checksum = in_checksum((unsigned short *)&buf, buf_len);

    return cpy_len;
}

int tcp_segment_proc(unsigned char *stream)
{
    return 0;
}

int arp_packet_proc(unsigned char *stream, unsigned char *rbuff, int rsize, unsigned char *mac, unsigned char *ip)
{
    int cpy_len = 0;
    struct arp_packet *arp = (struct arp_packet *)stream;
    if (ntohs(arp->opcode) == 0x1 && inet_addr(ip) == arp->d_ip)
    {
        struct arp_packet arp_reply;
        memcpy(&arp_reply, arp, sizeof(struct arp_packet));

        arp_reply.opcode = htons(0x0002);
        arp_reply.d_ip = arp->s_ip;
        arp_reply.s_ip = arp->d_ip;
        memcpy(arp_reply.d_mac, arp->s_mac, ETH_ALEN);
        memcpy(arp_reply.s_mac, mac, ETH_ALEN);

        cpy_len += sizeof(struct arp_packet);
        memcpy(rbuff, &arp_reply, cpy_len);
    }

    return cpy_len;
}

int main()
{
    unsigned char local_mac[ETH_ALEN] = {0};
    struct eth_header *eth;
    struct pollfd pfd = {0};
    struct nm_pkthdr h;
    unsigned char *stream = NULL;
    unsigned char buffer[BUFFER_SIZE] = {0};

    struct nm_desc *nmr = nm_open("netmap:eth1", NULL, 0, NULL);
	if (nmr == NULL) {
		return -1;
	}

	pfd.fd = nmr->fd;
	pfd.events = POLLIN;

	while (1)
    {
        int ret = poll(&pfd, 1, -1);
        if (ret < 0)
            continue;

        if (pfd.revents & POLLIN)
        {
            int buff_size = 0;
            stream = nm_nextpkt(nmr, &h);
            eth = (struct eth_header *)stream;

            struct eth_header eth_hdr;
            memcpy(&eth_hdr, eth, sizeof(struct eth_header));
            memcpy(eth_hdr.d_mac, eth->s_mac, ETH_ALEN);
            str2mac(local_mac, LOCAL_MAC);
            memcpy(eth_hdr.s_mac, local_mac, ETH_ALEN);
            eth_hdr.type = eth->type;

            int cpy_len = BUFFER_SIZE < sizeof(struct eth_header) ? BUFFER_SIZE : sizeof(struct eth_header);
            memcpy(buffer, &eth_hdr, cpy_len);
            buff_size += cpy_len;

            if (ntohs(eth->type) == TYPE_IP)
            {
                buff_size += ip_packet_proc(stream + sizeof(struct eth_header), buffer+buff_size, BUFFER_SIZE-buff_size);
            }
            else if (ntohs(eth->type) == TYPE_ARP)
            {
                buff_size += arp_packet_proc(stream + sizeof(struct eth_header), buffer+buff_size, BUFFER_SIZE-buff_size, local_mac, LOCAL_IP);
            }

            nm_inject(nmr, buffer, buff_size);
        }
    }

    return 0;
}
