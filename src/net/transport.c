/*
 * Kerneloid - UDP and TCP Implementation
 * Transport layer protocols
 */

#include "net/transport.h"
#include "net/ipv4.h"
#include "kernel.h"
#include <string.h>

/* UDP sockets */
static udp_socket_t udp_sockets[MAX_UDP_SOCKETS];
static int udp_socket_count = 0;

/* TCP sockets */
static tcp_socket_t tcp_sockets[MAX_TCP_SOCKETS];
static int tcp_socket_count = 0;

/* Transport initialized */
static int transport_initialized = 0;

/*
 * Initialize transport layer
 */
void transport_init(void) {
    if (transport_initialized) return;
    
    memset(udp_sockets, 0, sizeof(udp_sockets));
    memset(tcp_sockets, 0, sizeof(tcp_sockets));
    udp_socket_count = 0;
    tcp_socket_count = 0;
    transport_initialized = 1;
    
    kprintf("[TRANSPORT] Transport layer initialized\n");
}

/*
 * Initialize UDP
 */
void udp_init(void) {
    transport_init();
    kprintf("[UDP] UDP initialized\n");
}

/*
 * Initialize TCP
 */
void tcp_init(void) {
    transport_init();
    kprintf("[TCP] TCP initialized\n");
}

/*
 * Handle received UDP packet
 */
void udp_handle_packet(ipv4_header_t* ip, udp_header_t* udp, uint16_t len) {
    if (!ip || !udp) return;
    
    uint16_t src_port = udp->src_port;
    uint16_t dst_port = udp->dst_port;
    
    kprintf("[UDP] Packet: %d.%d.%d.%d:%d -> :%d, len=%d\n",
            ip->src.addr[0], ip->src.addr[1],
            ip->src.addr[2], ip->src.addr[3],
            src_port, dst_port, len);
    
    /* Find matching socket */
    for (int i = 0; i < udp_socket_count; i++) {
        udp_socket_t* sock = &udp_sockets[i];
        if (sock->local_port == dst_port) {
            /* Store source info */
            sock->remote_ip = ip->src;
            sock->remote_port = src_port;
            /* TODO: Deliver to application */
            return;
        }
    }
}

/*
 * Handle received TCP packet
 */
void tcp_handle_packet(ipv4_header_t* ip, tcp_header_t* tcp, uint16_t len) {
    if (!ip || !tcp) return;
    
    uint16_t src_port = tcp->src_port;
    uint16_t dst_port = tcp->dst_port;
    uint8_t flags = tcp->flags;
    
    kprintf("[TCP] Packet: %d.%d.%d.%d:%d -> :%d, flags=0x%02X, seq=%u, ack=%u\n",
            ip->src.addr[0], ip->src.addr[1],
            ip->src.addr[2], ip->src.addr[3],
            src_port, dst_port, flags, tcp->seq, tcp->ack);
    
    /* Find matching socket */
    for (int i = 0; i < tcp_socket_count; i++) {
        tcp_socket_t* sock = &tcp_sockets[i];
        if (sock->local_port == dst_port) {
            /* Handle based on state and flags */
            switch (sock->state) {
                case TCP_STATE_LISTEN:
                    if (flags & TCP_FLAG_SYN) {
                        /* Create new connection */
                        /* TODO: Implement full TCP handshake */
                    }
                    break;
                    
                case TCP_STATE_SYN_SENT:
                    if (flags & TCP_FLAG_SYN && flags & TCP_FLAG_ACK) {
                        sock->state = TCP_STATE_ESTABLISHED;
                        sock->remote_seq = tcp->seq + 1;
                        /* Send ACK */
                    }
                    break;
                    
                case TCP_STATE_ESTABLISHED:
                    if (flags & TCP_FLAG_FIN) {
                        sock->state = TCP_STATE_CLOSE_WAIT;
                    }
                    /* Process data */
                    break;
                    
                default:
                    break;
            }
            return;
        }
    }
}

/*
 * Create UDP socket
 */
udp_socket_t* udp_socket(void) {
    if (udp_socket_count >= MAX_UDP_SOCKETS) return NULL;
    
    udp_socket_t* sock = &udp_sockets[udp_socket_count++];
    memset(sock, 0, sizeof(udp_socket_t));
    
    return sock;
}

/*
 * Bind UDP socket
 */
int udp_bind(udp_socket_t* sock, ipv4_addr_t* addr, uint16_t port) {
    if (!sock) return -1;
    
    if (addr) sock->local_ip = *addr;
    sock->local_port = port;
    
    kprintf("[UDP] Bound socket to port %d\n", port);
    
    return 0;
}

/*
 * Send UDP datagram
 */
int udp_sendto(udp_socket_t* sock, const uint8_t* data, uint16_t len,
               ipv4_addr_t* dst, uint16_t port) {
    if (!sock || !data) return -1;
    
    net_interface_t* iface = net_get_first_interface();
    if (!iface) return -1;
    
    /* Build UDP header */
    uint8_t packet[1536];
    udp_header_t* udp = (udp_header_t*)packet;
    
    udp->src_port = sock->local_port;
    udp->dst_port = port;
    udp->length = sizeof(udp_header_t) + len;
    udp->checksum = 0;
    
    /* Copy data */
    memcpy(packet + sizeof(udp_header_t), data, len);
    
    /* Send via IPv4 */
    return ipv4_send(iface, &sock->local_ip, dst, IP_PROTO_UDP,
                    packet, sizeof(udp_header_t) + len);
}

/*
 * Receive UDP datagram
 */
int udp_recvfrom(udp_socket_t* sock, uint8_t* buffer, uint16_t max_len,
                 ipv4_addr_t* src, uint16_t* port) {
    if (!sock || !buffer) return -1;
    
    /* This would be implemented with a receive queue */
    /* For now, return no data */
    (void)max_len;
    (void)src;
    (void)port;
    
    return -1;
}

/*
 * Close UDP socket
 */
void udp_close(udp_socket_t* sock) {
    if (!sock) return;
    
    sock->local_port = 0;
    /* Remove from list if needed */
}

/*
 * Create TCP socket
 */
tcp_socket_t* tcp_socket(void) {
    if (tcp_socket_count >= MAX_TCP_SOCKETS) return NULL;
    
    tcp_socket_t* sock = &tcp_sockets[tcp_socket_count++];
    memset(sock, 0, sizeof(tcp_socket_t));
    
    sock->state = TCP_STATE_CLOSED;
    sock->seq = 0;
    sock->window = 65535;
    
    return sock;
}

/*
 * Bind TCP socket
 */
int tcp_bind(tcp_socket_t* sock, ipv4_addr_t* addr, uint16_t port) {
    if (!sock) return -1;
    
    if (addr) sock->local_ip = *addr;
    sock->local_port = port;
    
    kprintf("[TCP] Bound socket to port %d\n", port);
    
    return 0;
}

/*
 * Listen on TCP socket
 */
int tcp_listen(tcp_socket_t* sock, int backlog) {
    if (!sock) return -1;
    
    sock->state = TCP_STATE_LISTEN;
    
    kprintf("[TCP] Listening on port %d\n", sock->local_port);
    
    (void)backlog;
    return 0;
}

/*
 * Accept TCP connection
 */
tcp_socket_t* tcp_accept(tcp_socket_t* sock) {
    if (!sock || sock->state != TCP_STATE_LISTEN) return NULL;
    
    /* This would wait for incoming connection */
    /* For now, return NULL */
    return NULL;
}

/*
 * Connect TCP socket
 */
int tcp_connect(tcp_socket_t* sock, ipv4_addr_t* dst, uint16_t port) {
    if (!sock || !dst) return -1;
    
    net_interface_t* iface = net_get_first_interface();
    if (!iface) return -1;
    
    sock->remote_ip = *dst;
    sock->remote_port = port;
    sock->state = TCP_STATE_SYN_SENT;
    
    /* Build TCP SYN */
    uint8_t packet[40];
    tcp_header_t* tcp = (tcp_header_t*)packet;
    
    tcp->src_port = sock->local_port;
    tcp->dst_port = port;
    tcp->seq = sock->seq;
    tcp->ack = 0;
    tcp->data_offset = (5 << 4);  /* 20 byte header */
    tcp->flags = TCP_FLAG_SYN;
    tcp->window = sock->window;
    tcp->checksum = 0;
    tcp->urgent = 0;
    
    /* Send SYN via IPv4 */
    int result = ipv4_send(iface, &sock->local_ip, dst, IP_PROTO_TCP,
                          packet, 20);
    
    if (result == 0) {
        sock->seq++;
    }
    
    return result;
}

/*
 * Send TCP data
 */
int tcp_send(tcp_socket_t* sock, const uint8_t* data, uint16_t len) {
    if (!sock || !data || sock->state != TCP_STATE_ESTABLISHED) return -1;
    
    net_interface_t* iface = net_get_first_interface();
    if (!iface) return -1;
    
    /* Build TCP segment with PSH+ACK */
    uint8_t packet[1536];
    tcp_header_t* tcp = (tcp_header_t*)packet;
    
    tcp->src_port = sock->local_port;
    tcp->dst_port = sock->remote_port;
    tcp->seq = sock->seq;
    tcp->ack = sock->remote_seq;
    tcp->data_offset = (5 << 4);
    tcp->flags = TCP_FLAG_PSH | TCP_FLAG_ACK;
    tcp->window = sock->window;
    tcp->checksum = 0;
    tcp->urgent = 0;
    
    /* Copy data */
    memcpy(packet + 20, data, len);
    
    /* Send */
    int result = ipv4_send(iface, &sock->local_ip, &sock->remote_ip, 
                          IP_PROTO_TCP, packet, 20 + len);
    
    if (result == 0) {
        sock->seq += len;
    }
    
    return result;
}

/*
 * Receive TCP data
 */
int tcp_recv(tcp_socket_t* sock, uint8_t* buffer, uint16_t max_len) {
    if (!sock || !buffer) return -1;
    
    /* This would receive from a queue */
    /* For now, return no data */
    (void)max_len;
    
    return -1;
}

/*
 * Close TCP socket
 */
void tcp_close(tcp_socket_t* sock) {
    if (!sock) return;
    
    if (sock->state == TCP_STATE_ESTABLISHED) {
        /* Send FIN */
        net_interface_t* iface = net_get_first_interface();
        if (iface) {
            uint8_t packet[20];
            tcp_header_t* tcp = (tcp_header_t*)packet;
            
            tcp->src_port = sock->local_port;
            tcp->dst_port = sock->remote_port;
            tcp->seq = sock->seq;
            tcp->ack = sock->remote_seq;
            tcp->data_offset = (5 << 4);
            tcp->flags = TCP_FLAG_FIN | TCP_FLAG_ACK;
            tcp->window = sock->window;
            tcp->checksum = 0;
            
            ipv4_send(iface, &sock->local_ip, &sock->remote_ip,
                     IP_PROTO_TCP, packet, 20);
        }
        
        sock->state = TCP_STATE_FIN_WAIT1;
    } else {
        sock->state = TCP_STATE_CLOSED;
    }
}
