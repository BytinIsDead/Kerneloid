/*
 * Minimal TCP/IP Stack for Tinx Kernel
 * Note: This is a simplified educational implementation
 */

#ifndef TINX_TCP_H
#define TINX_TCP_H

#include <stdint.h>
#include <stddef.h>

typedef int ssize_t;

/* Ethernet header */
struct eth_header {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t type;
} __attribute__((packed));

/* IP header */
struct ip_header {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed));

/* TCP header */
struct tcp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t data_offset;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed));

/* TCP Flags */
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20

/* Socket states */
#define SOCKET_CLOSED      0
#define SOCKET_LISTEN      1
#define SOCKET_SYN_SENT    2
#define SOCKET_SYN_RECV    3
#define SOCKET_ESTABLISHED 4
#define SOCKET_FIN_WAIT_1  5
#define SOCKET_FIN_WAIT_2  6
#define SOCKET_CLOSE_WAIT  7
#define SOCKET_CLOSING     8
#define SOCKET_LAST_ACK    9
#define SOCKET_TIME_WAIT   10

/* Socket structure */
struct socket {
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
    uint8_t state;
    uint32_t send_seq;
    uint32_t recv_ack;
    uint8_t *recv_buf;
    size_t recv_len;
    uint8_t *send_buf;
    size_t send_len;
};

/* Network interface */
struct net_if {
    uint8_t mac[6];
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    struct socket sockets[16];
    int socket_count;
};

/* Function prototypes */

/* Initialize network interface */
int tcp_init(struct net_if *iface, const uint8_t *mac, uint32_t ip);

/* Create a socket */
int tcp_socket(struct net_if *iface, uint16_t port);

/* Listen on a socket */
int tcp_listen(struct net_if *iface, int sock_fd);

/* Accept a connection */
int tcp_accept(struct net_if *iface, int sock_fd);

/* Connect to a remote host */
int tcp_connect(struct net_if *iface, int sock_fd, uint32_t ip, uint16_t port);

/* Send data */
ssize_t tcp_send(struct net_if *iface, int sock_fd, const void *data, size_t len);

/* Receive data */
ssize_t tcp_recv(struct net_if *iface, int sock_fd, void *buf, size_t len);

/* Close a socket */
int tcp_close(struct net_if *iface, int sock_fd);

/* Process incoming packet */
int tcp_process_packet(struct net_if *iface, const void *packet, size_t len);

/* Utility functions */
uint16_t tcp_checksum(const void *data, size_t len);
uint32_t tcp_inet_addr(const char *addr);

#endif /* TINX_TCP_H */
