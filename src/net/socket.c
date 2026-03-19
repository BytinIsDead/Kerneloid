/*
 * Kerneloid - Socket API Implementation
 * POSIX-like socket interface
 */

#include "net/socket.h"
#include "net/transport.h"
#include "kernel.h"
#include <string.h>

/* Socket table */
#define MAX_SOCKETS 32
static socket_t sockets[MAX_SOCKETS];
static int socket_count = 0;
static int next_fd = 3;  /* Start after stdin/stdout/stderr */

/*
 * Initialize socket subsystem
 */
void socket_init(void) {
    memset(sockets, 0, sizeof(sockets));
    socket_count = 0;
    
    /* Initialize transport layer */
    udp_init();
    tcp_init();
    
    kprintf("[SOCKET] Socket API initialized\n");
}

/*
 * Create a socket
 */
int socket(int domain, int type, int protocol) {
    if (socket_count >= MAX_SOCKETS) {
        return -1;
    }
    
    if (domain != AF_INET) {
        return -1;
    }
    
    socket_t* sock = &sockets[socket_count++];
    sock->fd = next_fd++;
    sock->domain = domain;
    sock->type = type;
    sock->protocol = protocol;
    sock->bound = false;
    sock->connected = false;
    
    /* Create underlying socket */
    if (type == SOCK_DGRAM) {
        sock->socket = (void*)udp_socket();
    } else if (type == SOCK_STREAM) {
        sock->socket = (void*)tcp_socket();
    } else {
        /* Raw sockets not implemented yet */
        return -1;
    }
    
    kprintf("[SOCKET] Created socket fd=%d, type=%d\n", sock->fd, type);
    
    return sock->fd;
}

/*
 * Bind socket to address
 */
int bind(int sockfd, const sockaddr_in_t* addr, int addrlen) {
    if (sockfd < 0 || !addr || addrlen < sizeof(sockaddr_in_t)) {
        return -1;
    }
    
    /* Find socket */
    socket_t* sock = NULL;
    for (int i = 0; i < socket_count; i++) {
        if (sockets[i].fd == sockfd) {
            sock = &sockets[i];
            break;
        }
    }
    
    if (!sock) {
        return -1;
    }
    
    /* Store local address */
    sock->local_addr = *addr;
    sock->bound = true;
    
    /* Bind underlying socket */
    if (sock->type == SOCK_DGRAM) {
        udp_socket_t* udp = (udp_socket_t*)sock->socket;
        return udp_bind(udp, (ipv4_addr_t*)&addr->sin_addr, addr->sin_port);
    } else if (sock->type == SOCK_STREAM) {
        tcp_socket_t* tcp = (tcp_socket_t*)sock->socket;
        return tcp_bind(tcp, (ipv4_addr_t*)&addr->sin_addr, addr->sin_port);
    }
    
    return -1;
}

/*
 * Connect to remote address
 */
int connect(int sockfd, const sockaddr_in_t* addr, int addrlen) {
    if (sockfd < 0 || !addr || addrlen < sizeof(sockaddr_in_t)) {
        return -1;
    }
    
    /* Find socket */
    socket_t* sock = NULL;
    for (int i = 0; i < socket_count; i++) {
        if (sockets[i].fd == sockfd) {
            sock = &sockets[i];
            break;
        }
    }
    
    if (!sock) {
        return -1;
    }
    
    /* Store remote address */
    sock->remote_addr = *addr;
    
    /* Connect */
    if (sock->type == SOCK_DGRAM) {
        /* For UDP, just store remote address */
        sock->connected = true;
        return 0;
    } else if (sock->type == SOCK_STREAM) {
        tcp_socket_t* tcp = (tcp_socket_t*)sock->socket;
        int result = tcp_connect(tcp, (ipv4_addr_t*)&addr->sin_addr, addr->sin_port);
        if (result == 0) {
            sock->connected = true;
        }
        return result;
    }
    
    return -1;
}

/*
 * Listen for connections
 */
int listen(int sockfd, int backlog) {
    if (sockfd < 0) {
        return -1;
    }
    
    /* Find socket */
    socket_t* sock = NULL;
    for (int i = 0; i < socket_count; i++) {
        if (sockets[i].fd == sockfd) {
            sock = &sockets[i];
            break;
        }
    }
    
    if (!sock || sock->type != SOCK_STREAM) {
        return -1;
    }
    
    tcp_socket_t* tcp = (tcp_socket_t*)sock->socket;
    return tcp_listen(tcp, backlog);
}

/*
 * Accept connection
 */
int accept(int sockfd, sockaddr_in_t* addr, int* addrlen) {
    if (sockfd < 0) {
        return -1;
    }
    
    /* Find socket */
    socket_t* sock = NULL;
    for (int i = 0; i < socket_count; i++) {
        if (sockets[i].fd == sockfd) {
            sock = &sockets[i];
            break;
        }
    }
    
    if (!sock || sock->type != SOCK_STREAM) {
        return -1;
    }
    
    tcp_socket_t* tcp = (tcp_socket_t*)sock->socket;
    tcp_socket_t* new_conn = tcp_accept(tcp);
    
    if (!new_conn) {
        return -1;
    }
    
    /* Create new socket for connection */
    if (socket_count >= MAX_SOCKETS) {
        return -1;
    }
    
    socket_t* new_sock = &sockets[socket_count++];
    new_sock->fd = next_fd++;
    new_sock->domain = sock->domain;
    new_sock->type = sock->type;
    new_sock->protocol = sock->protocol;
    new_sock->socket = new_conn;
    new_sock->connected = true;
    
    if (addr && addrlen) {
        *addrlen = sizeof(sockaddr_in_t);
        addr->sin_family = AF_INET;
        addr->sin_port = new_conn->remote_port;
        addr->sin_addr = new_conn->remote_ip;
    }
    
    return new_sock->fd;
}

/*
 * Send data
 */
int send(int sockfd, const void* buf, int len, int flags) {
    return sendto(sockfd, buf, len, flags, NULL, 0);
}

/*
 * Sendto data
 */
int sendto(int sockfd, const void* buf, int len, int flags,
            const sockaddr_in_t* dest_addr, int addrlen) {
    (void)flags;
    
    if (sockfd < 0 || !buf || len < 0) {
        return -1;
    }
    
    /* Find socket */
    socket_t* sock = NULL;
    for (int i = 0; i < socket_count; i++) {
        if (sockets[i].fd == sockfd) {
            sock = &sockets[i];
            break;
        }
    }
    
    if (!sock) {
        return -1;
    }
    
    if (sock->type == SOCK_DGRAM) {
        udp_socket_t* udp = (udp_socket_t*)sock->socket;
        
        if (dest_addr) {
            return udp_sendto(udp, buf, len, 
                             (ipv4_addr_t*)&dest_addr->sin_addr, 
                             dest_addr->sin_port);
        } else if (sock->connected) {
            return udp_sendto(udp, buf, len,
                             &sock->remote_addr.sin_addr,
                             sock->remote_addr.sin_port);
        }
    } else if (sock->type == SOCK_STREAM) {
        tcp_socket_t* tcp = (tcp_socket_t*)sock->socket;
        return tcp_send(tcp, buf, len);
    }
    
    return -1;
}

/*
 * Receive data
 */
int recv(int sockfd, void* buf, int len, int flags) {
    return recvfrom(sockfd, buf, len, flags, NULL, NULL);
}

/*
 * Recvfrom data
 */
int recvfrom(int sockfd, void* buf, int len, int flags,
             sockaddr_in_t* src_addr, int* addrlen) {
    (void)flags;
    
    if (sockfd < 0 || !buf || len < 0) {
        return -1;
    }
    
    /* Find socket */
    socket_t* sock = NULL;
    for (int i = 0; i < socket_count; i++) {
        if (sockets[i].fd == sockfd) {
            sock = &sockets[i];
            break;
        }
    }
    
    if (!sock) {
        return -1;
    }
    
    if (sock->type == SOCK_DGRAM) {
        udp_socket_t* udp = (udp_socket_t*)sock->socket;
        ipv4_addr_t src;
        uint16_t port;
        int result = udp_recvfrom(udp, buf, len, &src, &port);
        
        if (result >= 0 && src_addr) {
            src_addr->sin_family = AF_INET;
            src_addr->sin_port = port;
            src_addr->sin_addr = src;
        }
        
        return result;
    } else if (sock->type == SOCK_STREAM) {
        tcp_socket_t* tcp = (tcp_socket_t*)sock->socket;
        return tcp_recv(tcp, buf, len);
    }
    
    return -1;
}

/*
 * Close socket
 */
int close(int sockfd) {
    if (sockfd < 0) {
        return -1;
    }
    
    /* Find socket */
    socket_t* sock = NULL;
    for (int i = 0; i < socket_count; i++) {
        if (sockets[i].fd == sockfd) {
            sock = &sockets[i];
            break;
        }
    }
    
    if (!sock) {
        return -1;
    }
    
    if (sock->type == SOCK_DGRAM) {
        udp_close((udp_socket_t*)sock->socket);
    } else if (sock->type == SOCK_STREAM) {
        tcp_close((tcp_socket_t*)sock->socket);
    }
    
    /* Mark as closed */
    sock->fd = -1;
    
    return 0;
}

/*
 * Get socket error
 */
int getsockopt(int sockfd, int level, int optname, void* optval, int* optlen) {
    (void)sockfd;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    return -1;
}

/*
 * Set socket option
 */
int setsockopt(int sockfd, int level, int optname, const void* optval, int optlen) {
    (void)sockfd;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    return -1;
}
