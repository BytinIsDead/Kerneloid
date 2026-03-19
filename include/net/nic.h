/*
 * Kerneloid - Network Driver Interface
 * Abstract network interface for NIC drivers
 */

#ifndef KERNELOID_NIC_H
#define KERNELOID_NIC_H

#include <stdint.h>
#include <stdbool.h>

/* MAC address */
typedef struct {
    uint8_t addr[6];
} mac_addr_t;

/* IPv4 address */
typedef struct {
    uint8_t addr[4];
} ipv4_addr_t;

/* Network interface flags */
#define NIC_FLAG_UP         (1 << 0)
#define NIC_FLAG_RUNNING   (1 << 1)
#define NIC_FLAG_PROMISC   (1 << 2)
#define NIC_FLAG_LOOPBACK  (1 << 3)

/* NIC driver operations */
typedef struct nic_driver {
    /* Driver name */
    const char* name;
    
    /* Initialize NIC
     * Returns: 0 on success, -1 on error
     */
    int (*init)(void);
    
    /* Shutdown NIC */
    void (*shutdown)(void);
    
    /* Transmit packet
     * data: packet data
     * len: packet length
     * Returns: 0 on success, -1 on error
     */
    int (*transmit)(const uint8_t* data, uint16_t len);
    
    /* Check if packet received
     * Returns: number of packets available, or -1 if none
     */
    int (*poll)(void);
    
    /* Receive packet
     * buffer: buffer to store packet
     * max_len: maximum packet length
     * Returns: packet length, or -1 on error
     */
    int (*receive)(uint8_t* buffer, uint16_t max_len);
    
    /* Get MAC address */
    void (*get_mac)(mac_addr_t* mac);
    
    /* Get MTU */
    int (*get_mtu)(void);
    
    /* Set interface flags */
    void (*set_flags)(uint32_t flags);
    
    /* Get interface flags */
    uint32_t (*get_flags)(void);
    
    /* Link status */
    bool (*is_link_up)(void);
    
    /* Next driver in chain */
    struct nic_driver* next;
} nic_driver_t;

/* Network interface */
typedef struct {
    char name[32];
    nic_driver_t* driver;
    mac_addr_t mac;
    ipv4_addr_t ip;
    ipv4_addr_t netmask;
    ipv4_addr_t gateway;
    uint32_t flags;
    int mtu;
    bool initialized;
} net_interface_t;

/* Packet buffer */
typedef struct {
    uint8_t* data;
    uint16_t length;
    uint16_t capacity;
    void* private;  /* Driver-specific data */
} packet_buffer_t;

/*
 * Initialize network subsystem
 */
void net_init(void);

/*
 * Register NIC driver
 */
int net_register_driver(nic_driver_t* driver);

/*
 * Get network interface by name
 */
net_interface_t* net_get_interface(const char* name);

/*
 * Get first network interface
 */
net_interface_t* net_get_first_interface(void);

/*
 * Get all interfaces
 */
int net_get_interfaces(net_interface_t** ifaces, int max_count);

/*
 * Transmit packet on interface
 */
int net_transmit(net_interface_t* iface, const uint8_t* data, uint16_t len);

/*
 * Receive packet from interface
 */
int net_receive(net_interface_t* iface, uint8_t* buffer, uint16_t max_len);

/*
 * Poll for packets
 */
int net_poll(net_interface_t* iface);

/*
 * Set interface up/down
 */
int net_set_interface_up(net_interface_t* iface, bool up);

/*
 * Configure interface
 */
int net_configure_interface(net_interface_t* iface, ipv4_addr_t ip, 
                            ipv4_addr_t netmask, ipv4_addr_t gateway);

/*
 * Get MAC address string
 */
void net_mac_to_string(const mac_addr_t* mac, char* str);

/*
 * Get IPv4 address string
 */
void net_ipv4_to_string(const ipv4_addr_t* ip, char* str);

/*
 * Parse MAC address string
 */
int net_parse_mac(const char* str, mac_addr_t* mac);

/*
 * Parse IPv4 address string
 */
int net_parse_ipv4(const char* str, ipv4_addr_t* ip);

/*
 * Shutdown network subsystem
 */
void net_shutdown(void);

#endif /* KERNELOID_NIC_H */
