/*
 * Kerneloid - Ethernet and ARP Implementation
 * Ethernet frame handling and ARP protocol
 */

#include "net/ethernet.h"
#include "net/nic.h"
#include "kernel.h"
#include <string.h>

/* ARP cache */
static arp_cache_entry_t arp_cache[ARP_CACHE_SIZE];
static int arp_initialized = 0;

/* Broadcast MAC address */
static const mac_addr_t broadcast_mac = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

/*
 * Initialize Ethernet/ARP
 */
void eth_init(void) {
    if (arp_initialized) return;
    
    memset(arp_cache, 0, sizeof(arp_cache));
    arp_initialized = 1;
    
    kprintf("[ETH] Ethernet/ARP initialized\n");
}

/*
 * Handle received Ethernet frame
 */
void eth_handle_frame(eth_frame_t* frame) {
    if (!frame) return;
    
    switch (frame->type) {
        case ETH_TYPE_IPV4:
            /* Hand off to IPv4 layer */
            kprintf("[ETH] IPv4 packet received\n");
            break;
            
        case ETH_TYPE_ARP:
            /* Handle ARP */
            if (frame->length >= sizeof(arp_packet_t)) {
                arp_packet_t* arp = (arp_packet_t*)frame->data;
                net_interface_t* iface = net_get_first_interface();
                if (iface) {
                    arp_handle_packet(iface, arp);
                }
            }
            break;
            
        default:
            kprintf("[ETH] Unknown frame type: 0x%04X\n", frame->type);
            break;
    }
}

/*
 * Send Ethernet frame
 */
int eth_send(net_interface_t* iface, const mac_addr_t* dst, 
             uint16_t type, const uint8_t* data, uint16_t len) {
    if (!iface || !dst || !data) return -1;
    
    /* Build Ethernet header */
    uint8_t frame_data[1536];
    eth_header_t* header = (eth_header_t*)frame_data;
    
    header->dst = *dst;
    header->src = iface->mac;
    header->type = type;
    
    /* Copy payload */
    if (len > sizeof(frame_data) - sizeof(eth_header_t)) {
        return -1;
    }
    memcpy(frame_data + sizeof(eth_header_t), data, len);
    
    /* Transmit */
    return net_transmit(iface, frame_data, sizeof(eth_header_t) + len);
}

/*
 * Handle ARP packet
 */
void arp_handle_packet(net_interface_t* iface, arp_packet_t* packet) {
    if (!iface || !packet) return;
    
    /* Verify ARP is for Ethernet/IPv4 */
    if (packet->htype != 1 || packet->ptype != ETH_TYPE_IPV4 ||
        packet->hlen != 6 || packet->plen != 4) {
        return;
    }
    
    /* Add sender to ARP cache */
    arp_add_entry(&packet->spa, &packet->sha);
    
    /* Check if this is an ARP request for our IP */
    if (packet->op == ARP_OP_REQUEST) {
        /* Check if target is our IP */
        if (memcmp(&packet->tpa, &iface->ip, sizeof(ipv4_addr_t)) == 0) {
            /* Send ARP reply */
            arp_send_reply(iface, &packet->spa, &packet->sha);
        }
    }
}

/*
 * Send ARP request
 */
int arp_send_request(net_interface_t* iface, ipv4_addr_t target_ip) {
    if (!iface) return -1;
    
    /* Build ARP request */
    arp_packet_t arp;
    arp.htype = 1;          /* Ethernet */
    arp.ptype = ETH_TYPE_IPV4;
    arp.hlen = 6;
    arp.plen = 4;
    arp.op = ARP_OP_REQUEST;
    arp.sha = iface->mac;
    arp.spa = iface->ip;
    arp.tha = broadcast_mac;  /* Unknown */
    arp.tpa = target_ip;
    
    /* Send broadcast */
    return eth_send(iface, &broadcast_mac, ETH_TYPE_ARP, 
                   (uint8_t*)&arp, sizeof(arp));
}

/*
 * Send ARP reply
 */
int arp_send_reply(net_interface_t* iface, const ipv4_addr_t* target_ip,
                   const mac_addr_t* target_mac) {
    if (!iface || !target_ip || !target_mac) return -1;
    
    /* Build ARP reply */
    arp_packet_t arp;
    arp.htype = 1;
    arp.ptype = ETH_TYPE_IPV4;
    arp.hlen = 6;
    arp.plen = 4;
    arp.op = ARP_OP_REPLY;
    arp.sha = iface->mac;
    arp.spa = iface->ip;
    arp.tha = *target_mac;
    arp.tpa = *target_ip;
    
    /* Send unicast reply */
    return eth_send(iface, target_mac, ETH_TYPE_ARP,
                   (uint8_t*)&arp, sizeof(arp));
}

/*
 * Lookup MAC address for IP
 */
bool arp_lookup(ipv4_addr_t* ip, mac_addr_t* mac) {
    if (!ip || !mac) return false;
    
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && 
            memcmp(&arp_cache[i].ip, ip, sizeof(ipv4_addr_t)) == 0) {
            *mac = arp_cache[i].mac;
            return true;
        }
    }
    
    return false;
}

/*
 * Add entry to ARP cache
 */
void arp_add_entry(ipv4_addr_t* ip, mac_addr_t* mac) {
    if (!ip || !mac) return;
    
    /* Find existing entry or empty slot */
    int slot = -1;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && 
            memcmp(&arp_cache[i].ip, ip, sizeof(ipv4_addr_t)) == 0) {
            slot = i;
            break;
        }
        if (slot < 0 && !arp_cache[i].valid) {
            slot = i;
        }
    }
    
    if (slot >= 0) {
        arp_cache[slot].ip = *ip;
        arp_cache[slot].mac = *mac;
        arp_cache[slot].valid = true;
        arp_cache[slot].timestamp = 0;  /* Could use real timestamp */
    }
}

/*
 * Clear ARP cache
 */
void arp_clear_cache(void) {
    memset(arp_cache, 0, sizeof(arp_cache));
}

/*
 * Get ARP cache entry
 */
arp_cache_entry_t* arp_get_cache(void) {
    return arp_cache;
}
