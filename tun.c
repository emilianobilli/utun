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
#include <net/if.h>
#include <net/if_utun.h>

#define UTUN_CONTROL_NAME "com.apple.net.utun_control"
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

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

int configure_interface(const char* iface_name, const char* ip_address, const char* netmask) {
    struct ifreq ifr;
    int sockfd;
    struct sockaddr_in addr;
    
    // Crear socket para realizar la configuración
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    // Limpiar la estructura ifreq y establecer el nombre de la interfaz
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface_name, IFNAMSIZ);

    addr.sin_addr.s_addr = inet_addr(ip_address);
    addr.sin_family = AF_INET;
#if defined (__APPLE__)
    addr.sin_len = sizeof(struct sockaddr_in);
#endif
    // Configurar la dirección IP
    memcpy(&(ifr.ifr_ifru.ifru_addr), &addr, sizeof(struct sockaddr_in));
    if (ioctl(sockfd, SIOCSIFADDR, &ifr) < 0) {
        close(sockfd);
        return -1;
    }

    // Configurar la máscara de red
    addr.sin_addr.s_addr = inet_addr(netmask);
    memcpy(&(ifr.ifr_ifru.ifru_addr), &addr, sizeof(struct sockaddr_in));
    if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) {
        close(sockfd);
        return -1;
    }

#if defined (__linux__)
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
    ioctl(sockfd, SIOCSIFFLAGS, &ifr);
#endif
    close(sockfd);
    return 0;
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
