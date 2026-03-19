/*
 * Kerneloid - UDP and TCP
 * Transport layer protocols
 */

#ifndef KERNELOID_TRANSPORT_H
#define KERNELOID_TRANSPORT_H

#include "net/nic.h"
#include <stdint.h>
#include <stdbool.h>

/* UDP header */
typedef struct {
    uint16_t src_port;    /* Source port */
    uint16_t dst_port;    /* Destination port */
    uint16_t length;      /* UDP length */
    uint16_t checksum;    /* UDP checksum */
} __attribute__((packed)) udp_header_t;

/* TCP flags */
#define TCP_FLAG_FIN  0x01
#define TCP_FLAG_SYN  0x02
#define TCP_FLAG_RST  0x04
#define TCP_FLAG_PSH  0x08
#define TCP_FLAG_ACK  0x10
#define TCP_FLAG_URG  0x20

/* TCP states */
typedef enum {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECEIVED,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT1,
    TCP_STATE_FIN_WAIT2,
    TCP_STATE_CLOSING,
    TCP_STATE_TIME_WAIT,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_LAST_ACK
} tcp_state_t;

/* TCP header */
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  data_offset;  /* 4 bits offset, 4 bits reserved */
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed)) tcp_header_t;

/* TCP socket */
typedef struct {
    int fd;
    ipv4_addr_t local_ip;
    uint16_t local_port;
    ipv4_addr_t remote_ip;
    uint16_t remote_port;
    tcp_state_t state;
    uint32_t seq;
    uint32_t ack;
    uint32_t remote_seq;
    uint16_t window;
    void* user_data;
} tcp_socket_t;

/* UDP socket */
typedef struct {
    int fd;
    ipv4_addr_t local_ip;
    uint16_t local_port;
    ipv4_addr_t remote_ip;
    uint16_t remote_port;
    void* user_data;
} udp_socket_t;

/* Maximum sockets */
#define MAX_TCP_SOCKETS 16
#define MAX_UDP_SOCKETS 16

/*
 * Initialize transport layer
 */
void transport_init(void);

/*
 * Initialize UDP
 */
void udp_init(void);

/*
 * Initialize TCP
 */
void tcp_init(void);

/*
 * Handle received UDP packet
 */
void udp_handle_packet(ipv4_header_t* ip, udp_header_t* udp, uint16_t len);

/*
 * Handle received TCP packet
 */
void tcp_handle_packet(ipv4_header_t* ip, tcp_header_t* tcp, uint16_t len);

/*
 * Create UDP socket
 */
udp_socket_t* udp_socket(void);

/*
 * Bind UDP socket
 */
int udp_bind(udp_socket_t* sock, ipv4_addr_t* addr, uint16_t port);

/*
 * Send UDP datagram
 */
int udp_sendto(udp_socket_t* sock, const uint8_t* data, uint16_t len,
               ipv4_addr_t* dst, uint16_t port);

/*
 * Receive UDP datagram
 */
int udp_recvfrom(udp_socket_t* sock, uint8_t* buffer, uint16_t max_len,
                 ipv4_addr_t* src, uint16_t* port);

/*
 * Close UDP socket
 */
void udp_close(udp_socket_t* sock);

/*
 * Create TCP socket
 */
tcp_socket_t* tcp_socket(void);

/*
 * Bind TCP socket
 */
int tcp_bind(tcp_socket_t* sock, ipv4_addr_t* addr, uint16_t port);

/*
 * Listen on TCP socket
 */
int tcp_listen(tcp_socket_t* sock, int backlog);

/*
 * Accept TCP connection
 */
tcp_socket_t* tcp_accept(tcp_socket_t* sock);

/*
 * Connect TCP socket
 */
int tcp_connect(tcp_socket_t* sock, ipv4_addr_t* dst, uint16_t port);

/*
 * Send TCP data
 */
int tcp_send(tcp_socket_t* sock, const uint8_t* data, uint16_t len);

/*
 * Receive TCP data
 */
int tcp_recv(tcp_socket_t* sock, uint8_t* buffer, uint16_t max_len);

/*
 * Close TCP socket
 */
void tcp_close(tcp_socket_t* sock);

#endif /* KERNELOID_TRANSPORT_H */
