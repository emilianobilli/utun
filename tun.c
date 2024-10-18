// tun_osx.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#if defined(__linux__)

#include <linux/if.h>
#include <linux/if_tun.h>


#elif defined(__APPLE__)
#include <sys/kern_control.h>
#include <sys/socket.h>
#include <sys/sys_domain.h>
#include <net/if_utun.h>
#include <net/if_dl.h>
#include <sys/sysctl.h>

#define UTUN_CONTROL_NAME "com.apple.net.utun_control"
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/if.h>
#include <net/route.h>

char *bcast(in_addr_t ip, in_addr_t mask) {
    struct in_addr in;
    in.s_addr = (ip & mask) | ~mask;
    return inet_ntoa(in);
}

static int addrset(struct sockaddr_in *addr, const char *cp) {
    memset(addr, 0, sizeof(struct sockaddr_in));
    if (cp == NULL)
        return 1;
#if defined (__APPLE__)
    addr->sin_len = sizeof(struct sockaddr_in);
#endif
    addr->sin_family = AF_INET;
    return inet_pton(AF_INET, cp, &addr->sin_addr);
}

char *sys_error() {
	return strerror(errno);
}

int set_mtu(const char *iface_name, int mtu) {
    struct ifreq ifr;
    int sockfd;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface_name, IFNAMSIZ);
    ifr.ifr_ifru.ifru_mtu = mtu;
	if (ioctl(sockfd, SIOCSIFMTU, &ifr) < 0) {
        close(sockfd);
        return -1;
    }
    close(sockfd);
    return 0;
}

#if defined(__APPLE__)
int add_route(const char *dest, const char *gateway, const char *netmask, const char *iface) {
    int sockfd;
    struct {
        struct rt_msghdr hdr;
        struct sockaddr_in dst;
        struct sockaddr_in gw;
        struct sockaddr_in netmask;
        struct sockaddr_dl ifp;
    } msg;

    memset(&msg, 0, sizeof(msg));

    // Abrir un socket de ruta (routing socket)
    sockfd = socket(PF_ROUTE, SOCK_RAW, AF_INET);
    if (sockfd < 0) {
        return -1;
    }

    if (!addrset(&msg.dst, dest)) {
        return -1;
    }
    if (!addrset(&msg.netmask, netmask)) {
        return -1;
    }
    if (!addrset(&msg.gw, gateway)) {
        return -1;
    }


    // Llenar la estructura rt_msghdr
    msg.hdr.rtm_msglen = sizeof(msg);
    msg.hdr.rtm_version = RTM_VERSION;
    msg.hdr.rtm_type = RTM_ADD;
    msg.hdr.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC | RTF_IFSCOPE;
    msg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK | RTA_IFP;
    msg.hdr.rtm_seq = 1;
    msg.hdr.rtm_pid = getpid();


    // Si la máscara es 255.255.255.255, es una ruta a un host
    if (msg.netmask.sin_addr.s_addr == 0xffffffff) {
        msg.hdr.rtm_flags |= RTF_HOST;
    }

    // Interfaz (iface)
    msg.ifp.sdl_len = sizeof(struct sockaddr_dl);
    msg.ifp.sdl_family = AF_LINK;
    msg.ifp.sdl_nlen = strlen(iface);
    strncpy((char *)msg.ifp.sdl_data, iface, strlen(iface));

    // Enviar el mensaje al kernel
    if (write(sockfd, &msg, sizeof(msg)) < 0) {
        close(sockfd);
        return -1;
    }

    // Cerrar el socket
    close(sockfd);
    return 0;
}
#elif defined(__linux__)
int add_route(const char *dest, const char *gateway, const char *netmask, const char *iface) {
    int sockfd;
    struct rtentry route;
    struct sockaddr_in *addr;

    // Crear un socket para interactuar con las rutas
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error abriendo el socket");
        return -1;
    }

    // Limpiar la estructura de la ruta
    memset(&route, 0, sizeof(route));

    // Establecer la dirección de destino (red de destino)
    addr = (struct sockaddr_in *)&route.rt_dst;
    addrset(addr, dest);  // Por ejemplo, "10.0.0.0"

    // Establecer la máscara de red (para una red completa)
    addr = (struct sockaddr_in *)&route.rt_genmask;
    addrset(addr, netmask);  // Por ejemplo, "255.255.255.0" para /24

    // Establecer el gateway
    addr = (struct sockaddr_in *)&route.rt_gateway;
    addrset(addr, gateway);  // Por ejemplo, "10.0.0.1"

    // Configurar la ruta
    route.rt_flags = RTF_UP | RTF_GATEWAY;  // Ruta activa y con gateway
    route.rt_dev = (char *)iface;  // Interfaz de salida (por ejemplo, "eth0")

    // Llamada a ioctl para agregar la ruta
    if (ioctl(sockfd, SIOCADDRT, &route) < 0) {
        perror("Error al agregar la ruta");
        close(sockfd);
        return -1;
    }

    // Cerrar el socket
    close(sockfd);
    return 0;
}
#endif

int configure_interface(const char* iface_name, const char* ip_address, const char* netmask, const char *peer_address) {
    struct ifreq ifr;
    int sockfd;
    struct sockaddr_in ip;
    struct sockaddr_in mask;
    struct sockaddr_in peer;

    if (!addrset(&ip, ip_address) || !addrset(&mask, netmask))
        return -1;


    // Crear socket para realizar la configuración
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    // Limpiar la estructura ifreq y establecer el nombre de la interfaz
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface_name, IFNAMSIZ);

    // Configurar la dirección IP
    memcpy(&(ifr.ifr_ifru.ifru_addr), &ip, sizeof(struct sockaddr_in));
    if (ioctl(sockfd, SIOCSIFADDR, &ifr) < 0) {
        close(sockfd);
        return -1;
    }


    if (peer_address != NULL && addrset(&peer, peer_address)) {
        memcpy(&(ifr.ifr_ifru.ifru_dstaddr), &peer, sizeof(struct sockaddr_in));
        if (ioctl(sockfd, SIOCSIFDSTADDR, &ifr) < 0) {
            close(sockfd);
            return -1;
        }
    }

    memcpy(&(ifr.ifr_ifru.ifru_addr), &mask, sizeof(struct sockaddr_in));
    if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) {
        close(sockfd);
        return -1;
    }

#if defined (__linux__)
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
    ioctl(sockfd, SIOCSIFFLAGS, &ifr);
#endif

    close(sockfd);
    return add_route(bcast(ip.sin_addr.s_addr, mask.sin_addr.s_addr), ip_address, netmask, iface_name);
}

#if defined(__APPLE__)
int open_utun() {
    struct ctl_info ctl_info;
    struct sockaddr_ctl sc;
    int fd;

    // Crear el socket de control
    fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
    if (fd < 0) {
        return -1;
    }

    // Obtener la información del control
    memset(&ctl_info, 0, sizeof(ctl_info));
    strncpy(ctl_info.ctl_name, UTUN_CONTROL_NAME, sizeof(ctl_info.ctl_name));
    if (ioctl(fd, CTLIOCGINFO, &ctl_info) == -1) {
        close(fd);
        return -1;
    }

    // Configurar la dirección del socket de control
    memset(&sc, 0, sizeof(sc));
    sc.sc_id = ctl_info.ctl_id;
    sc.sc_len = sizeof(sc);
    sc.sc_family = AF_SYSTEM;
    sc.ss_sysaddr = AF_SYS_CONTROL;
    sc.sc_unit = 0; // Deja que el sistema asigne una interfaz (utun0, utun1, etc.)

    // Conectar al socket
    if (connect(fd, (struct sockaddr*)&sc, sizeof(sc)) == -1) {
        close(fd);
        return -1;
    }

    return fd; // Retorna el file descriptor
}

char *get_utun_name(int fd) {
	static char ifname[IFNAMSIZ];
	    // Obtener el nombre de la interfaz creada
    socklen_t ifname_len = IFNAMSIZ;
    if (getsockopt(fd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, ifname, &ifname_len) == -1) {
        close(fd);
        return NULL;
    }

    return ifname;  // Retorna el nombre de la interfaz
}
#elif defined(__linux__)

char *alloc_ifname() {
	return calloc(1,IFNAMSIZ);
}

int tun_alloc(char *dev) {
    struct ifreq ifr;
    int fd, err;

    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    // Si el nombre de la interfaz es proporcionado, cópialo en ifr_name
    if (*dev) {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ-1);
    }

    // Llama a ioctl para configurar la interfaz TUN/TAP
    if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
        close(fd);
        return err;
    }

    strncpy(dev, ifr.ifr_name, IFNAMSIZ);

    return fd;
}
#endif

