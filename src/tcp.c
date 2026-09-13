/*
 * Minimal TCP/IP Stack Implementation
 */

#include "tcp.h"

typedef int ssize_t;

static char *tcp_memcpy(void *dest, const void *src, int n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

#define memcpy tcp_memcpy

static __attribute__((unused)) uint16_t checksum_counter = 0;

uint16_t tcp_checksum(const void *data, size_t len) {
    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t sum = 0;
    /* Handle 16-bit words correctly even if unaligned and handle odd length per RFC 1071 */
    while (len > 1) {
        uint16_t word = (uint16_t)ptr[0] | ((uint16_t)ptr[1] << 8);
        /* Network order is big-endian; but for checksum we just sum host order and complement.
           To be endian-independent, we assemble as little-endian consistent with x86. */
        sum += word;
        ptr += 2;
        len -= 2;
    }
    if (len == 1) {
        /* Odd byte: pad with zero high byte (RFC 1071) -> add as 0x00XX where XX is last byte */
        uint16_t last = (uint16_t)*ptr;
        sum += last;
    }
    /* Fold 32-bit sum to 16 bits */
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

uint32_t tcp_inet_addr(const char *addr) {
    uint32_t ip = 0;
    uint32_t part;
    int i;
    if (!addr || *addr == '\0') return 0; /* 0.0.0.0 invalid */
    for (i = 0; i < 4; i++) {
        if (*addr < '0' || *addr > '9') return 0xFFFFFFFFU; /* INADDR_NONE */
        part = 0;
        int digits = 0;
        while (*addr >= '0' && *addr <= '9') {
            part = part * 10 + (*addr - '0');
            if (part > 255) return 0xFFFFFFFFU;
            addr++;
            digits++;
            if (digits > 3) return 0xFFFFFFFFU;
        }
        ip |= (part << (24 - i * 8));
        if (i < 3) {
            if (*addr != '.') return 0xFFFFFFFFU;
            addr++;
            if (*addr == '\0') return 0xFFFFFFFFU;
        } else {
            if (*addr != '\0') return 0xFFFFFFFFU; /* extra chars */
        }
    }
    return ip;
}

int tcp_init(struct net_if *iface, const uint8_t *mac, uint32_t ip) {
    int i;
    if (!iface || !mac) {
        return -1;
    }
    
    tcp_memcpy(iface->mac, mac, 6);
    iface->ip = ip;
    iface->netmask = 0xFFFFFF00; /* Default /24 */
    iface->gateway = ip & 0xFFFFFF00;
    iface->gateway |= 0x00000001; /* Assume .1 is gateway */
    iface->socket_count = 0;
    
    /* Initialize all sockets as closed */
    for (i = 0; i < 16; i++) {
        iface->sockets[i].state = SOCKET_CLOSED;
    }
    
    return 0;
}

int tcp_socket(struct net_if *iface, uint16_t port) {
    int i;
    if (!iface) {
        return -1;
    }
    
    /* Find free socket */
    for (i = 0; i < 16; i++) {
        if (iface->sockets[i].state == SOCKET_CLOSED) {
            iface->sockets[i].local_port = port;
            iface->sockets[i].local_ip = iface->ip;
            iface->sockets[i].state = SOCKET_CLOSED;
            iface->sockets[i].send_seq = 0;
            iface->sockets[i].recv_ack = 0;
            iface->socket_count++;
            return i;
        }
    }
    
    return -1; /* No free sockets */
}

int tcp_listen(struct net_if *iface, int sock_fd) {
    if (!iface || sock_fd < 0 || sock_fd >= 16) {
        return -1;
    }
    
    iface->sockets[sock_fd].state = SOCKET_LISTEN;
    return 0;
}

int tcp_accept(struct net_if *iface, int sock_fd) {
    (void)iface;
    (void)sock_fd;
    /* Simplified - would wait for connection in real impl */
    return -1;
}

int tcp_connect(struct net_if *iface, int sock_fd, uint32_t ip, uint16_t port) {
    struct socket *sock;
    if (!iface || sock_fd < 0 || sock_fd >= 16) {
        return -1;
    }
    
    sock = &iface->sockets[sock_fd];
    
    sock->remote_ip = ip;
    sock->remote_port = port;
    sock->state = SOCKET_SYN_SENT;
    sock->send_seq = 1000; /* Initial sequence number */
    
    /* In real impl: send SYN packet */
    
    return 0;
}

ssize_t tcp_send(struct net_if *iface, int sock_fd, const void *data, size_t len) {
    struct socket *sock;
    if (!iface || sock_fd < 0 || sock_fd >= 16 || !data) {
        return -1;
    }
    
    sock = &iface->sockets[sock_fd];
    
    if (sock->state != SOCKET_ESTABLISHED) {
        return -1;
    }
    
    /* In real impl: create and send TCP packet */
    sock->send_seq += (uint32_t)len;
    
    return (ssize_t)len;
}

ssize_t tcp_recv(struct net_if *iface, int sock_fd, void *buf, size_t len) {
    struct socket *sock;
    size_t to_copy;
    if (!iface || sock_fd < 0 || sock_fd >= 16 || !buf) {
        return -1;
    }
    
    sock = &iface->sockets[sock_fd];
    
    if (sock->state != SOCKET_ESTABLISHED) {
        return -1;
    }
    
    if (sock->recv_len == 0) {
        return 0; /* No data available */
    }
    
    to_copy = (len < sock->recv_len) ? len : sock->recv_len;
    tcp_memcpy(buf, sock->recv_buf, (int)to_copy);
    sock->recv_len -= to_copy;
    sock->recv_ack += (uint32_t)to_copy;
    
    return (ssize_t)to_copy;
}

int tcp_close(struct net_if *iface, int sock_fd) {
    struct socket *sock;
    if (!iface || sock_fd < 0 || sock_fd >= 16) {
        return -1;
    }
    
    sock = &iface->sockets[sock_fd];
    
    if (sock->state == SOCKET_ESTABLISHED) {
        sock->state = SOCKET_FIN_WAIT_1;
        /* In real impl: send FIN packet */
    } else {
        sock->state = SOCKET_CLOSED;
    }
    
    iface->socket_count--;
    return 0;
}

int tcp_process_packet(struct net_if *iface, const void *packet, size_t len) {
    const struct ip_header *ip;
    const struct tcp_header *tcp;
    int i;
    
    if (!iface || !packet || len < sizeof(struct eth_header) + sizeof(struct ip_header) + sizeof(struct tcp_header)) {
        return -1;
    }
    
    ip = (const struct ip_header *)(packet + sizeof(struct eth_header));
    tcp = (const struct tcp_header *)(packet + sizeof(struct eth_header) + sizeof(struct ip_header));
    
    /* Check if packet is for us */
    if (ip->dest_ip != iface->ip) {
        return -1;
    }
    
    /* Find matching socket */
    for (i = 0; i < 16; i++) {
        struct socket *sock = &iface->sockets[i];
        
        if (sock->state == SOCKET_LISTEN && tcp->dest_port == sock->local_port) {
            /* Incoming connection request */
            if (tcp->flags & TCP_SYN) {
                sock->remote_ip = ip->src_ip;
                sock->remote_port = tcp->src_port;
                sock->state = SOCKET_SYN_RECV;
                sock->recv_ack = tcp->seq_num + 1;
                /* In real impl: send SYN-ACK */
            }
            return 0;
        }
        
        if (sock->local_port == tcp->dest_port && 
            sock->remote_port == tcp->src_port &&
            sock->remote_ip == ip->src_ip) {
            
            /* Process based on state and flags */
            if (tcp->flags & TCP_ACK) {
                if (sock->state == SOCKET_SYN_SENT) {
                    sock->state = SOCKET_ESTABLISHED;
                }
                sock->send_seq = tcp->ack_num;
            }
            
            if (tcp->flags & TCP_FIN) {
                sock->state = SOCKET_CLOSE_WAIT;
            }
            
            return 0;
        }
    }
    
    return -1; /* No matching socket */
}
