/*
 * Kerneloid - Ethernet and ARP
 * Ethernet frame handling and ARP protocol
 */

#ifndef KERNELOID_ETHERNET_H
#define KERNELOID_ETHERNET_H

#include "net/nic.h"
#include <stdint.h>
#include <stdbool.h>

/* Ethernet protocol types */
#define ETH_TYPE_IPV4    0x0800
#define ETH_TYPE_ARP     0x0806
#define ETH_TYPE_IPV6    0x86DD

/* Ethernet header */
typedef struct {
    mac_addr_t dst;      /* Destination MAC */
    mac_addr_t src;      /* Source MAC */
    uint16_t type;       /* Protocol type */
} __attribute__((packed)) eth_header_t;

/* ARP operation codes */
#define ARP_OP_REQUEST   1
#define ARP_OP_REPLY     2

/* ARP packet */
typedef struct {
    uint16_t htype;     /* Hardware type (1 = Ethernet) */
    uint16_t ptype;     /* Protocol type (0x0800 = IPv4) */
    uint8_t  hlen;      /* Hardware address length (6) */
    uint8_t  plen;      /* Protocol address length (4) */
    uint16_t op;        /* Operation (1=request, 2=reply) */
    mac_addr_t sha;     /* Sender hardware address */
    ipv4_addr_t spa;    /* Sender protocol address */
    mac_addr_t tha;     /* Target hardware address */
    ipv4_addr_t tpa;    /* Target protocol address */
} __attribute__((packed)) arp_packet_t;

/* ARP cache entry */
typedef struct {
    ipv4_addr_t ip;
    mac_addr_t mac;
    uint32_t timestamp;
    bool valid;
} arp_cache_entry_t;

#define ARP_CACHE_SIZE 32

/* Ethernet frame buffer */
typedef struct {
    uint8_t data[1536];
    uint16_t length;
    mac_addr_t src;
    mac_addr_t dst;
    uint16_t type;
} eth_frame_t;

/*
 * Initialize Ethernet/ARP
 */
void eth_init(void);

/*
 * Handle received Ethernet frame
 */
void eth_handle_frame(eth_frame_t* frame);

/*
 * Send Ethernet frame
 */
int eth_send(net_interface_t* iface, const mac_addr_t* dst, 
             uint16_t type, const uint8_t* data, uint16_t len);

/*
 * Send ARP request
 */
int arp_send_request(net_interface_t* iface, ipv4_addr_t target_ip);

/*
 * Send ARP reply
 */
int arp_send_reply(net_interface_t* iface, const ipv4_addr_t* target_ip,
                   const mac_addr_t* target_mac);

/*
 * Lookup MAC address for IP
 * Returns: true if found in cache
 */
bool arp_lookup(ipv4_addr_t* ip, mac_addr_t* mac);

/*
 * Add entry to ARP cache
 */
void arp_add_entry(ipv4_addr_t* ip, mac_addr_t* mac);

/*
 * Clear ARP cache
 */
void arp_clear_cache(void);

/*
 * Get ARP cache entry
 */
arp_cache_entry_t* arp_get_cache(void);

/*
 * Handle ARP packet
 */
void arp_handle_packet(net_interface_t* iface, arp_packet_t* packet);

#endif /* KERNELOID_ETHERNET_H */
