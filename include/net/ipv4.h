/*
 * Kerneloid - IPv4 and ICMP
 * IP packet handling and ICMP echo (ping)
 */

#ifndef KERNELOID_IPV4_H
#define KERNELOID_IPV4_H

#include "net/nic.h"
#include <stdint.h>
#include <stdbool.h>

/* IP protocol numbers */
#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

/* IPv4 header */
typedef struct {
    uint8_t  ver_ihl;     /* Version (4) + IHL (5) */
    uint8_t  tos;          /* Type of service */
    uint16_t total_len;   /* Total length */
    uint16_t id;          /* Identification */
    uint16_t flags_frag;  /* Flags + Fragment offset */
    uint8_t  ttl;         /* Time to live */
    uint8_t  proto;       /* Protocol */
    uint16_t checksum;    /* Header checksum */
    ipv4_addr_t src;     /* Source address */
    ipv4_addr_t dst;     /* Destination address */
} __attribute__((packed)) ipv4_header_t;

/* ICMP types */
#define ICMP_TYPE_ECHO_REPLY     0
#define ICMP_TYPE_DEST_UNREACH   3
#define ICMP_TYPE_SRC_QUENCH    4
#define ICMP_TYPE_REDIRECT      5
#define ICMP_TYPE_ECHO_REQUEST  8
#define ICMP_TYPE_ROUTER_ADV    9
#define ICMP_TYPE_ROUTER_SOL    10
#define ICMP_TYPE_TIME_EXCEED   11

/* ICMP echo request/reply */
typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
    uint8_t  data[];
} __attribute__((packed)) icmp_echo_t;

/* IP packet buffer */
typedef struct {
    ipv4_header_t ip;
    uint8_t payload[1460];
} __attribute__((packed)) ipv4_packet_t;

/*
 * Initialize IPv4 stack
 */
void ipv4_init(void);

/*
 * Handle received IPv4 packet
 */
void ipv4_handle_packet(net_interface_t* iface, ipv4_header_t* ip, 
                       uint16_t len);

/*
 * Send IPv4 packet
 */
int ipv4_send(net_interface_t* iface, ipv4_addr_t* src, ipv4_addr_t* dst,
              uint8_t proto, const uint8_t* data, uint16_t len);

/*
 * Calculate IP header checksum
 */
uint16_t ipv4_checksum(const void* data, uint16_t len);

/*
 * Initialize ICMP
 */
void icmp_init(void);

/*
 * Handle ICMP packet
 */
void icmp_handle_packet(ipv4_header_t* ip, uint8_t* data, uint16_t len);

/*
 * Send ICMP echo request (ping)
 */
int icmp_send_echo(net_interface_t* iface, ipv4_addr_t* target, 
                   uint16_t id, uint16_t sequence);

/*
 * Send ICMP echo reply
 */
int icmp_send_echo_reply(ipv4_header_t* ip, icmp_echo_t* echo, uint16_t len);

#endif /* KERNELOID_IPV4_H */
