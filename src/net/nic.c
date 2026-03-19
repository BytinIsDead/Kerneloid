/*
 * Kerneloid - Network Driver Implementation
 * Network interface management
 */

#include "net/nic.h"
#include "kernel.h"
#include <string.h>
#include <stdio.h>

/* Network interfaces */
#define MAX_INTERFACES 4
static net_interface_t interfaces[MAX_INTERFACES];
static int interface_count = 0;

/* Registered drivers */
static nic_driver_t* driver_list = NULL;

/* Network initialized */
static int net_initialized = 0;

/*
 * Initialize network subsystem
 */
void net_init(void) {
    if (net_initialized) return;
    
    memset(interfaces, 0, sizeof(interfaces));
    interface_count = 0;
    net_initialized = 1;
    
    kprintf("[NET] Network subsystem initialized\n");
}

/*
 * Register NIC driver
 */
int net_register_driver(nic_driver_t* driver) {
    if (!driver) return -1;
    
    /* Add to driver list */
    driver->next = driver_list;
    driver_list = driver;
    
    kprintf("[NET] Registered driver: %s\n", driver->name);
    
    return 0;
}

/*
 * Create network interface from driver
 */
static net_interface_t* create_interface(nic_driver_t* driver) {
    if (interface_count >= MAX_INTERFACES) return NULL;
    
    net_interface_t* iface = &interfaces[interface_count++];
    
    strncpy(iface->name, driver->name, sizeof(iface->name) - 1);
    iface->driver = driver;
    iface->mtu = 1500;
    iface->flags = 0;
    iface->initialized = false;
    
    /* Get MAC address */
    if (driver->get_mac) {
        driver->get_mac(&iface->mac);
    }
    
    /* Initialize driver */
    if (driver->init) {
        if (driver->init() != 0) {
            kprintf("[NET] Failed to initialize driver: %s\n", driver->name);
            return NULL;
        }
    }
    
    iface->initialized = true;
    iface->flags |= NIC_FLAG_UP;
    
    kprintf("[NET] Created interface: %s\n", iface->name);
    
    return iface;
}

/*
 * Get network interface by name
 */
net_interface_t* net_get_interface(const char* name) {
    if (!name) return NULL;
    
    for (int i = 0; i < interface_count; i++) {
        if (strcmp(interfaces[i].name, name) == 0) {
            return &interfaces[i];
        }
    }
    
    return NULL;
}

/*
 * Get first network interface
 */
net_interface_t* net_get_first_interface(void) {
    if (interface_count > 0) {
        return &interfaces[0];
    }
    return NULL;
}

/*
 * Get all interfaces
 */
int net_get_interfaces(net_interface_t** ifaces, int max_count) {
    if (!ifaces) return 0;
    
    int count = (interface_count < max_count) ? interface_count : max_count;
    
    for (int i = 0; i < count; i++) {
        ifaces[i] = &interfaces[i];
    }
    
    return count;
}

/*
 * Transmit packet on interface
 */
int net_transmit(net_interface_t* iface, const uint8_t* data, uint16_t len) {
    if (!iface || !iface->driver || !data) return -1;
    if (!(iface->flags & NIC_FLAG_UP)) return -1;
    
    if (iface->driver->transmit) {
        return iface->driver->transmit(data, len);
    }
    
    return -1;
}

/*
 * Receive packet from interface
 */
int net_receive(net_interface_t* iface, uint8_t* buffer, uint16_t max_len) {
    if (!iface || !iface->driver || !buffer) return -1;
    if (!(iface->flags & NIC_FLAG_UP)) return -1;
    
    if (iface->driver->receive) {
        return iface->driver->receive(buffer, max_len);
    }
    
    return -1;
}

/*
 * Poll for packets
 */
int net_poll(net_interface_t* iface) {
    if (!iface || !iface->driver) return -1;
    
    if (iface->driver->poll) {
        return iface->driver->poll();
    }
    
    return -1;
}

/*
 * Set interface up/down
 */
int net_set_interface_up(net_interface_t* iface, bool up) {
    if (!iface) return -1;
    
    if (up) {
        iface->flags |= NIC_FLAG_UP;
    } else {
        iface->flags &= ~NIC_FLAG_UP;
    }
    
    return 0;
}

/*
 * Configure interface
 */
int net_configure_interface(net_interface_t* iface, ipv4_addr_t ip, 
                            ipv4_addr_t netmask, ipv4_addr_t gateway) {
    if (!iface) return -1;
    
    iface->ip = ip;
    iface->netmask = netmask;
    iface->gateway = gateway;
    
    char ip_str[16], nm_str[16], gw_str[16];
    net_ipv4_to_string(&ip, ip_str);
    net_ipv4_to_string(&netmask, nm_str);
    net_ipv4_to_string(&gateway, gw_str);
    
    kprintf("[NET] Configured %s: IP=%s, Mask=%s, GW=%s\n", 
            iface->name, ip_str, nm_str, gw_str);
    
    return 0;
}

/*
 * Get MAC address string
 */
void net_mac_to_string(const mac_addr_t* mac, char* str) {
    if (!mac || !str) return;
    
    sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac->addr[0], mac->addr[1], mac->addr[2],
            mac->addr[3], mac->addr[4], mac->addr[5]);
}

/*
 * Get IPv4 address string
 */
void net_ipv4_to_string(const ipv4_addr_t* ip, char* str) {
    if (!ip || !str) return;
    
    sprintf(str, "%d.%d.%d.%d",
            ip->addr[0], ip->addr[1], ip->addr[2], ip->addr[3]);
}

/*
 * Parse MAC address string
 */
int net_parse_mac(const char* str, mac_addr_t* mac) {
    if (!str || !mac) return -1;
    
    unsigned int a[6];
    if (sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x",
               &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]) != 6) {
        return -1;
    }
    
    for (int i = 0; i < 6; i++) {
        mac->addr[i] = (uint8_t)a[i];
    }
    
    return 0;
}

/*
 * Parse IPv4 address string
 */
int net_parse_ipv4(const char* str, ipv4_addr_t* ip) {
    if (!str || !ip) return -1;
    
    unsigned int a[4];
    if (sscanf(str, "%u.%u.%u.%u",
               &a[0], &a[1], &a[2], &a[3]) != 4) {
        return -1;
    }
    
    for (int i = 0; i < 4; i++) {
        if (a[i] > 255) return -1;
        ip->addr[i] = (uint8_t)a[i];
    }
    
    return 0;
}

/*
 * Shutdown network subsystem
 */
void net_shutdown(void) {
    /* Shutdown all drivers */
    nic_driver_t* driver = driver_list;
    while (driver) {
        if (driver->shutdown) {
            driver->shutdown();
        }
        driver = driver->next;
    }
    
    interface_count = 0;
    net_initialized = 0;
    
    kprintf("[NET] Network subsystem shutdown\n");
}

/*
 * Initialize loopback interface (for hosted/testing mode)
 */
int net_init_loopback(void) {
    net_init();
    
    /* Create a loopback interface structure */
    static nic_driver_t loopback_driver = {
        .name = "loopback",
    };
    
    /* Register as a loopback interface */
    net_interface_t* iface = create_interface(&loopback_driver);
    if (iface) {
        iface->flags |= NIC_FLAG_LOOPBACK;
        
        /* Set default loopback address */
        ipv4_addr_t ip = {{127, 0, 0, 1}};
        ipv4_addr_t mask = {{255, 0, 0, 0}};
        ipv4_addr_t gw = {{0, 0, 0, 0}};
        
        net_configure_interface(iface, ip, mask, gw);
        
        return 0;
    }
    
    return -1;
}
