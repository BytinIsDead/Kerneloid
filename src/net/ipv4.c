/*
 * Kerneloid - IPv4 and ICMP Implementation
 * IP packet handling and ICMP echo (ping)
 */

#include "net/ipv4.h"
#include "net/ethernet.h"
#include "kernel.h"
#include <string.h>

/* IPv4 state */
static int ipv4_initialized = 0;
static uint16_t ipv4_id = 0;

/*
 * Initialize IPv4 stack
 */
void ipv4_init(void) {
    if (ipv4_initialized) return;
    
    ipv4_id = 0;
    ipv4_initialized = 1;
    
    kprintf("[IPv4] IPv4 stack initialized\n");
}

/*
 * Calculate IP header checksum
 */
uint16_t ipv4_checksum(const void* data, uint16_t len) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;
    
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    
    if (len) {
        sum += *(const uint8_t*)ptr;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return (uint16_t)(~sum);
}

/*
 * Handle received IPv4 packet
 */
void ipv4_handle_packet(net_interface_t* iface, ipv4_header_t* ip, uint16_t len) {
    if (!iface || !ip) return;
    
    /* Verify version */
    if ((ip->ver_ihl >> 4) != 4) {
        kprintf("[IPv4] Invalid version\n");
        return;
    }
    
    /* Verify header length */
    uint8_t ihl = (ip->ver_ihl & 0x0F) * 4;
    if (ihl < 20) {
        kprintf("[IPv4] Invalid header length\n");
        return;
    }
    
    /* Verify total length */
    uint16_t total_len = ip->total_len;
    if (total_len > len) {
        kprintf("[IPv4] Truncated packet\n");
        return;
    }
    
    /* Verify checksum */
    uint16_t recv_checksum = ip->checksum;
    ip->checksum = 0;
    if (ipv4_checksum(ip, ihl) != recv_checksum) {
        kprintf("[IPv4] Bad checksum\n");
        return;
    }
    ip->checksum = recv_checksum;
    
    /* Check if destined for us */
    bool for_us = (memcmp(&ip->dst, &iface->ip, sizeof(ipv4_addr_t)) == 0);
    bool broadcast = (ip->dst.addr[0] == 0xFF);
    
    if (!for_us && !broadcast) {
        /* Not for us - could implement routing here */
        return;
    }
    
    /* Handle based on protocol */
    uint16_t payload_len = total_len - ihl;
    uint8_t* payload = ((uint8_t*)ip) + ihl;
    
    switch (ip->proto) {
        case IP_PROTO_ICMP:
            icmp_handle_packet(ip, payload, payload_len);
            break;
            
        case IP_PROTO_TCP:
            kprintf("[IPv4] TCP packet received\n");
            break;
            
        case IP_PROTO_UDP:
            kprintf("[IPv4] UDP packet received\n");
            break;
            
        default:
            kprintf("[IPv4] Unknown protocol: %d\n", ip->proto);
            break;
    }
}

/*
 * Send IPv4 packet
 */
int ipv4_send(net_interface_t* iface, ipv4_addr_t* src, ipv4_addr_t* dst,
              uint8_t proto, const uint8_t* data, uint16_t len) {
    if (!iface || !dst || !data) return -1;
    
    /* Build IP header */
    ipv4_packet_t packet;
    ipv4_header_t* ip = &packet.ip;
    
    ip->ver_ihl = (4 << 4) | 5;  /* IPv4, 20 byte header */
    ip->tos = 0;
    ip->total_len = sizeof(ipv4_header_t) + len;
    ip->id = ipv4_id++;
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->proto = proto;
    ip->checksum = 0;
    
    if (src) {
        ip->src = *src;
    } else {
        ip->src = iface->ip;
    }
    ip->dst = *dst;
    
    /* Calculate checksum */
    ip->checksum = ipv4_checksum(ip, sizeof(ipv4_header_t));
    
    /* Copy payload */
    memcpy(packet.payload, data, len);
    
    /* Look up MAC address for destination */
    mac_addr_t dst_mac;
    if (dst->addr[0] == 0xFF && dst->addr[1] == 0xFF &&
        dst->addr[2] == 0xFF && dst->addr[3] == 0xFF &&
        dst->addr[4] == 0xFF && dst->addr[5] == 0xFF) {
        /* Broadcast */
        dst_mac = (mac_addr_t){{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    } else if (dst->addr[0] == 127) {
        /* Loopback */
        dst_mac = (mac_addr_t){{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    } else {
        /* Try ARP lookup */
        if (!arp_lookup(dst, &dst_mac)) {
            /* Send ARP request */
            arp_send_request(iface, *dst);
            return -1;
        }
    }
    
    /* Send via Ethernet */
    return eth_send(iface, &dst_mac, ETH_TYPE_IPV4, 
                   (uint8_t*)&packet, sizeof(ipv4_header_t) + len);
}

/*
 * Initialize ICMP
 */
void icmp_init(void) {
    ipv4_init();
    kprintf("[ICMP] ICMP initialized\n");
}

/*
 * Handle ICMP packet
 */
void icmp_handle_packet(ipv4_header_t* ip, uint8_t* data, uint16_t len) {
    if (!ip || !data || len < sizeof(icmp_echo_t)) return;
    
    icmp_echo_t* icmp = (icmp_echo_t*)data;
    
    switch (icmp->type) {
        case ICMP_TYPE_ECHO_REQUEST:
            kprintf("[ICMP] Echo request from %d.%d.%d.%d\n",
                    ip->src.addr[0], ip->src.addr[1],
                    ip->src.addr[2], ip->src.addr[3]);
            /* Send echo reply */
            icmp_send_echo_reply(ip, icmp, len);
            break;
            
        case ICMP_TYPE_ECHO_REPLY:
            kprintf("[ICMP] Echo reply from %d.%d.%d.%d, id=%d, seq=%d\n",
                    ip->src.addr[0], ip->src.addr[1],
                    ip->src.addr[2], ip->src.addr[3],
                    icmp->id, icmp->sequence);
            break;
            
        default:
            kprintf("[ICMP] Unknown type: %d\n", icmp->type);
            break;
    }
}

/*
 * Send ICMP echo request
 */
int icmp_send_echo(net_interface_t* iface, ipv4_addr_t* target,
                   uint16_t id, uint16_t sequence) {
    if (!iface || !target) return -1;
    
    icmp_echo_t echo;
    echo.type = ICMP_TYPE_ECHO_REQUEST;
    echo.code = 0;
    echo.checksum = 0;
    echo.id = id;
    echo.sequence = sequence;
    
    /* Fill with some data */
    for (int i = 0; i < 56; i++) {
        echo.data[i] = i & 0xFF;
    }
    
    /* Calculate checksum */
    echo.checksum = ipv4_checksum(&echo, sizeof(echo) + 56);
    
    return ipv4_send(iface, NULL, target, IP_PROTO_ICMP,
                    (uint8_t*)&echo, sizeof(echo) + 56);
}

/*
 * Send ICMP echo reply
 */
int icmp_send_echo_reply(ipv4_header_t* ip, icmp_echo_t* echo, uint16_t len) {
    if (!ip || !echo) return -1;
    
    net_interface_t* iface = net_get_first_interface();
    if (!iface) return -1;
    
    /* Swap source and destination */
    ipv4_addr_t dst = ip->src;
    
    /* Change type to echo reply */
    echo->type = ICMP_TYPE_ECHO_REPLY;
    echo->checksum = 0;
    
    /* Recalculate checksum */
    echo->checksum = ipv4_checksum(echo, len);
    
    return ipv4_send(iface, NULL, &dst, IP_PROTO_ICMP,
                    (uint8_t*)echo, len);
}

/*
 * Simple ping function
 */
int ping(ipv4_addr_t* target) {
    net_interface_t* iface = net_get_first_interface();
    if (!iface) return -1;
    
    kprintf("[PING] Sending to %d.%d.%d.%d\n",
            target->addr[0], target->addr[1],
            target->addr[2], target->addr[3]);
    
    return icmp_send_echo(iface, target, 1, 1);
}
