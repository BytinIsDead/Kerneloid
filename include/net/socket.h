/*
 * Kerneloid - Socket API
 * POSIX-like socket interface
 */

#ifndef KERNELOID_SOCKET_H
#define KERNELOID_SOCKET_H

#include <stdint.h>
#include "net/nic.h"

/* Socket domains */
#define AF_INET   2   /* IPv4 */

/* Socket types */
#define SOCK_STREAM  1   /* TCP */
#define SOCK_DGRAM   2   /* UDP */
#define SOCK_RAW     3   /* Raw */

/* Socket protocols */
#define IPPROTO_TCP  6
#define IPPROTO_UDP  17
#define IPPROTO_ICMP 1

/* Socket address */
typedef struct {
    uint16_t sin_family;   /* Address family */
    uint16_t sin_port;     /* Port */
    ipv4_addr_t sin_addr;  /* IP address */
} sockaddr_in_t;

/* Socket descriptor */
typedef struct {
    int fd;
    int domain;
    int type;
    int protocol;
    sockaddr_in_t local_addr;
    sockaddr_in_t remote_addr;
    void* socket;  /* tcp_socket_t or udp_socket_t */
    bool bound;
    bool connected;
} socket_t;

/* Socket errors */
#define EOK         0
#define EADDRINUSE  1
#define ECONNREFUSED 2
#define ETIMEDOUT   3
#define EINVAL      4
#define ENOMEM      5

/*
 * Initialize socket subsystem
 */
void socket_init(void);

/*
 * Create a socket
 * Returns: socket file descriptor, or -1 on error
 */
int socket(int domain, int type, int protocol);

/*
 * Bind socket to address
 * Returns: 0 on success, -1 on error
 */
int bind(int sockfd, const sockaddr_in_t* addr, int addrlen);

/*
 * Connect to remote address
 * Returns: 0 on success, -1 on error
 */
int connect(int sockfd, const sockaddr_in_t* addr, int addrlen);

/*
 * Listen for connections
 * Returns: 0 on success, -1 on error
 */
int listen(int sockfd, int backlog);

/*
 * Accept connection
 * Returns: new socket descriptor, or -1 on error
 */
int accept(int sockfd, sockaddr_in_t* addr, int* addrlen);

/*
 * Send data
 * Returns: number of bytes sent, or -1 on error
 */
int send(int sockfd, const void* buf, int len, int flags);

/*
 * Sendto data
 * Returns: number of bytes sent, or -1 on error
 */
int sendto(int sockfd, const void* buf, int len, int flags,
            const sockaddr_in_t* dest_addr, int addrlen);

/*
 * Receive data
 * Returns: number of bytes received, or -1 on error
 */
int recv(int sockfd, void* buf, int len, int flags);

/*
 * Recvfrom data
 * Returns: number of bytes received, or -1 on error
 */
int recvfrom(int sockfd, void* buf, int len, int flags,
             sockaddr_in_t* src_addr, int* addrlen);

/*
 * Close socket
 * Returns: 0 on success, -1 on error
 */
int close(int sockfd);

/*
 * Get socket error
 */
int getsockopt(int sockfd, int level, int optname, void* optval, int* optlen);

/*
 * Set socket option
 */
int setsockopt(int sockfd, int level, int optname, const void* optval, int optlen);

#endif /* KERNELOID_SOCKET_H */
