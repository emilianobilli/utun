#ifndef __SDTL_TUN_
#define __SDTL_TUN_ 1

#if defined(__APPLE__)

extern int open_utun();
extern char *get_utun_name(int fd);

#elif defined(__linux__)
extern char *alloc_ifname();
extern int tun_alloc(char *dev);

#endif

extern char *sys_error();
extern int configure_interface(const char* iface_name, const char* ip_address, const char* netmask);
extern int set_mtu(const char *iface_name, int mtu);
#endif 