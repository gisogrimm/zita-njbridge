// ----------------------------------------------------------------------------
//
//  Copyright (C) 2011-2016 Fons Adriaensen <fons@linuxaudio.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ----------------------------------------------------------------------------

#include <fcntl.h>
#ifndef _WIN32
#include <netinet/tcp.h>
#endif
#include <stddef.h>
#include <stdio.h> // for perror()
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/ioctl.h>
#include <sys/un.h>
#endif
#include <unistd.h>
#ifdef __APPLE__
#include <netinet/in.h>
#endif
#include "zsockets.h"
#ifndef _WIN32
#include <arpa/inet.h>
#include <net/if.h>
#endif
#include <errno.h>

#ifdef _WIN32
#include <Iphlpapi.h>

typedef u_short sa_family_t;

#endif

Sockaddr::Sockaddr(int family)
{
    reset(family);
}

void Sockaddr::reset(int family)
{
    sockaddr* S = (sockaddr*)_data;
    memset(_data, 0, sizeof(struct sockaddr_storage));
    S->sa_family = family;
}

int Sockaddr::set_addr(int family, int socktype, int protocol,
                       const char* address)
{
    int len;

#ifndef _WIN32
    struct sockaddr_un* U;
    if(family == AF_UNIX)
    {
        U = (struct sockaddr_un*)_data;
        len =
            sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path);
        if((int)strlen(address) > len - 1)
            return -1;
        U->sun_family = AF_UNIX;
        strcpy(U->sun_path, address);
        return 0;
    }
#endif

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = family;
    hints.ai_socktype = socktype;
    hints.ai_protocol = protocol;
    int rv = getaddrinfo(address, NULL, &hints, &res);
    // rv = getaddrinfo(address, 0, &H, &A);
    if(rv)
        return rv;
    if(res == NULL)
        return -1;
    switch(res->ai_family)
    {
    case AF_INET:
        len = sizeof(struct sockaddr_in);
        break;
    case AF_INET6:
        len = sizeof(struct sockaddr_in6);
        break;
    default:
        len = 0;
    }
    if(len)
        memcpy(_data, (char*)(res->ai_addr), len);
    freeaddrinfo(res);
    return len ? 0 : -1;
}

void Sockaddr::set_port(int port)
{
    struct sockaddr* S = (struct sockaddr*)_data;
    switch(S->sa_family)
    {
    case AF_INET:
    {
        struct sockaddr_in* S4 = (struct sockaddr_in*)_data;
        S4->sin_port = htons(port);
    }
    case AF_INET6:
    {
        struct sockaddr_in6* S6 = (struct sockaddr_in6*)_data;
        S6->sin6_port = htons(port);
    }
    }
}

int Sockaddr::get_addr(char* address, int len) const
{
    struct sockaddr* S = (struct sockaddr*)_data;
    *address = 0;
    switch(S->sa_family)
    {
#ifndef _WIN32
    case AF_UNIX:
    {
        struct sockaddr_un* U = (struct sockaddr_un*)_data;
        strcpy(address, U->sun_path);
        return 0;
    }
#endif
    case AF_INET:
    {
        struct sockaddr_in* S4 = (struct sockaddr_in*)_data;
        return (inet_ntop(AF_INET, &(S4->sin_addr.s_addr), address, len) == 0)
                   ? -1
                   : 0;
    }
    case AF_INET6:
    {
        struct sockaddr_in6* S6 = (struct sockaddr_in6*)_data;
        return (inet_ntop(AF_INET6, &(S6->sin6_addr.s6_addr), address, len) ==
                0)
                   ? -1
                   : 0;
    }
    }
    return 0;
}

int Sockaddr::get_port(void) const
{
    struct sockaddr* S = (struct sockaddr*)_data;
    switch(S->sa_family)
    {
    case AF_INET:
    {
        struct sockaddr_in* S4 = (struct sockaddr_in*)_data;
        return ntohs(S4->sin_port);
    }
    case AF_INET6:
    {
        struct sockaddr_in6* S6 = (struct sockaddr_in6*)_data;
        return ntohs(S6->sin6_port);
    }
    }
    return 0;
}

int Sockaddr::family(void) const
{
    struct sockaddr* S = (struct sockaddr*)_data;
    return S->sa_family;
}

bool Sockaddr::is_multicast(void) const
{
    struct sockaddr* S = (struct sockaddr*)_data;
    switch(S->sa_family)
    {
    case AF_INET:
    {
        struct sockaddr_in* S4 = (struct sockaddr_in*)_data;
        return IN_MULTICAST(ntohl(S4->sin_addr.s_addr)) ? true : false;
    }
    case AF_INET6:
    {
        struct sockaddr_in6* S6 = (struct sockaddr_in6*)_data;
        return IN6_IS_ADDR_MULTICAST(&(S6->sin6_addr)) ? true : false;
    }
    }
    return false;
}

int Sockaddr::sa_len(void) const
{
    struct sockaddr* S = (struct sockaddr*)_data;
    switch(S->sa_family)
    {
#ifndef _WIN32
    case AF_UNIX:
        return sizeof(struct sockaddr_un);
#endif
    case AF_INET:
        return sizeof(struct sockaddr_in);
    case AF_INET6:
        return sizeof(struct sockaddr_in6);
    }
    return sizeof(struct sockaddr_storage);
}

//int sock_open_active(Sockaddr* remote, Sockaddr* local)
//{
//    int fd, ipar;
//    socklen_t len;
//    sa_family_t fam;
//
//    fam = remote->family();
//    len = remote->sa_len();
//#ifdef _WIN32
//    if(fam != AF_INET && fam != AF_INET6)
//#else
//    if(fam != AF_INET && fam != AF_INET6 && fam != AF_UNIX)
//#endif
//        return -1;
//
//    fd = socket(fam, SOCK_STREAM, 0);
//    if(fd < 0)
//        return -1;
//#ifndef _WIN32
//    if(fam != AF_UNIX)
//#endif
//    {
//        ipar = 1;
//        if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&ipar, sizeof(int)))
//        {
//            close(fd);
//            return -1;
//        }
//    }
//    if(local && (local->family() == fam))
//    {
//        if(bind(fd, local->sa_ptr(), len))
//        {
//            close(fd);
//            return -1;
//        }
//    }
//    if(connect(fd, remote->sa_ptr(), len))
//    {
//        close(fd);
//        return -1;
//    }
//    if(local && local->family() == AF_UNSPEC)
//    {
//        if(getsockname(fd, local->sa_ptr(), &len))
//        {
//            close(fd);
//            return -1;
//        }
//    }
//    return fd;
//}

//int sock_open_passive(Sockaddr* local, int qlen)
//{
//    int fd;
//    sa_family_t fam;
//    socklen_t len;
//
//    fam = local->family();
//    len = local->sa_len();
//#ifdef _WIN32
//    if(fam != AF_INET && fam != AF_INET6)
//#else
//    if(fam != AF_INET && fam != AF_INET6 && fam != AF_UNIX)
//#endif
//      return -1;
//
//    fd = socket(fam, SOCK_STREAM, 0);
//    if(fd < 0)
//        return -1;
//    if(bind(fd, local->sa_ptr(), len) || (listen(fd, qlen) < 0))
//    {
//        close(fd);
//        return -1;
//    }
//    return 0;
//}

//int sock_accept(int fd, Sockaddr* remote, Sockaddr* local)
//{
//    int newfd;
//    sa_family_t fam;
//    socklen_t len;
//
//    fam = remote->family();
//    len = remote->sa_len();
//#ifdef _WIN32
//    if(fam != AF_INET && fam != AF_INET6)
//#else
//    if(fam != AF_INET && fam != AF_INET6 && fam != AF_UNIX)
//#endif
//        return -1;
//
//    newfd = accept(fd, remote->sa_ptr(), &len);
//    if(newfd < 0)
//        return -1;
//    if(local && getsockname(newfd, local->sa_ptr(), &len))
//    {
//        close(newfd);
//        return -1;
//    }
//    return newfd;
//}

int sock_open_dgram(Sockaddr* remote, Sockaddr* local)
{
    int fd;
    sa_family_t fam;
    socklen_t len;

    fam = AF_UNSPEC;
    len = 0;
    if(remote)
    {
        fam = remote->family();
        len = remote->sa_len();
    } else if(local)
    {
        fam = local->family();
        len = local->sa_len();
    }

    if(local && remote && local->family() != remote->family())
        return -1;

    fd = socket(fam, SOCK_DGRAM, 0);
    if(fd < 0)
        return -1;

    if(local && bind(fd, local->sa_ptr(), len))
    {
        close(fd);
        return -1;
    }
    if(remote && connect(fd, remote->sa_ptr(), len))
    {
        close(fd);
        return -1;
    }
    return fd;
}

int sock_open_mcsend(Sockaddr* addr, const char* iface, int loop, int hops)
{
#ifndef _WIN32
    int fd, ipar;

    sa_family_t fam = addr->family();
    socklen_t len = addr->sa_len();
    if(fam != AF_INET && fam != AF_INET6)
        return -1;

    fd = socket(fam, SOCK_DGRAM, 0);
    if(fd < 0)
        return -1;
    ipar = 1;
    if(fam == AF_INET6)
    {
      // IPv6:
        ipar = if_nametoindex(iface);
        if(setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop,
                      sizeof(loop)) ||
           setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops,
                      sizeof(hops)) ||
           setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char*)&ipar,
                      sizeof(int)))
        {
            close(fd);
            return -1;
        }
    } else
    {
      // IPv4:
        struct ifreq ifr;
        strncpy(ifr.ifr_name, iface, 16);
        ifr.ifr_name[15] = 0;
        ifr.ifr_addr.sa_family = AF_INET;
        if(ioctl(fd, SIOCGIFADDR, &ifr))
        {
            close(fd);
            return -1;
        }
        in_addr ifa;
        ifa = ((struct sockaddr_in*)(&ifr.ifr_addr))->sin_addr;
        if(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) ||
           setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &hops, sizeof(hops)) ||
           setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (char*)&ifa,
                      sizeof(ifa)))
        {
            close(fd);
            return -1;
        }
    }
    if(connect(fd, addr->sa_ptr(), len))
    {
        close(fd);
        return -1;
    }
    return fd;
#else
    SOCKET fd;
    DWORD ipar;

    int fam = addr->family();
    int len = addr->sa_len();
    if(fam != AF_INET && fam != AF_INET6)
        return -1;

    fd = socket(fam, SOCK_DGRAM, 0);
    if(fd == INVALID_SOCKET)
        return -1;

    ipar = 1;
    if(fam == AF_INET6)
    {
      // IPv6:
        ipar = if_nametoindex(iface);
        if(setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (char*)&loop,
                      sizeof(loop)) != 0 ||
           setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char*)&hops,
                      sizeof(hops)) != 0 ||
           setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char*)&ipar,
                      sizeof(ipar)) != 0)
        {
            closesocket(fd);
            return -1;
        }
    } else
    {
      // IPv4:
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        inet_pton(AF_INET, iface, &sin.sin_addr);

        if(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loop,
                      sizeof(loop)) != 0 ||
           setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&hops,
                      sizeof(hops)) != 0 ||
           setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (char*)&sin.sin_addr,
                      sizeof(sin.sin_addr)) != 0)
        {
            closesocket(fd);
            return -1;
        }
    }

    if(connect(fd, addr->sa_ptr(), len))
    {
        closesocket(fd);
        return -1;
    }
    return fd;
#endif
}

int sock_open_mcrecv(Sockaddr* addr, const char* iface)
{
#ifdef _WIN32
    SOCKET fd;
    int ipar;
#else
    int fd, ipar;
#endif

    sa_family_t fam = addr->family();
    if(fam != AF_INET && fam != AF_INET6)
        return -1;

    fd = socket(fam, SOCK_DGRAM, 0);
    if(fd < 0)
        return -1;

    ipar = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&ipar, sizeof(int)))
    {
        close(fd);
        return -1;
    }

    if(fam == AF_INET6)
    {
        // IPv6
        struct ipv6_mreq mcreq;
        struct sockaddr_in6 W6, *A6;

        A6 = (struct sockaddr_in6*)addr->sa_ptr();
        memset(&W6, 0, sizeof(sockaddr_in6));
        W6.sin6_family = AF_INET6;
        W6.sin6_port = A6->sin6_port;
        if(bind(fd, (sockaddr*)&W6, sizeof(sockaddr_in6)))
        {
            close(fd);
            return -1;
        }
        memcpy(&mcreq.ipv6mr_multiaddr, &(A6->sin6_addr),
               sizeof(struct in6_addr));
        mcreq.ipv6mr_interface = if_nametoindex(iface);
#ifdef __APPLE__
        if(setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char*)&mcreq,
                      sizeof(mcreq)))
#else
        if(setsockopt(fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char*)&mcreq,
                      sizeof(mcreq)))
#endif
        {
#ifdef _WIN32
            closesocket(fd);
#else
            close(fd);
#endif
            return -1;
        }
    } else
    {
        // IPv4
        struct ip_mreq mcreq;
        struct sockaddr_in W4, *A4;

        A4 = (struct sockaddr_in*)addr->sa_ptr();
        memset(&W4, 0, sizeof(sockaddr_in));
        W4.sin_family = AF_INET;
        W4.sin_port = A4->sin_port;
        if(bind(fd, (sockaddr*)&W4, sizeof(sockaddr_in)))
        {
#ifdef _WIN32
            closesocket(fd);
#else
            close(fd);
#endif
            return -1;
        }
#ifndef _WIN32
        // use named interface only when not on windows (on windows use
        // INADDR_ANY)
        struct ifreq ifreq;
        strncpy(ifreq.ifr_name, iface, 16);
        ifreq.ifr_name[15] = 0;
        ifreq.ifr_addr.sa_family = AF_INET;
        if(ioctl(fd, SIOCGIFADDR, &ifreq))
        {
            close(fd);
            return -1;
        }
        mcreq.imr_interface.s_addr =
            ((struct sockaddr_in*)(&ifreq.ifr_addr))->sin_addr.s_addr;
#else
        mcreq.imr_interface.s_addr = INADDR_ANY;
#endif
        mcreq.imr_multiaddr.s_addr = A4->sin_addr.s_addr;
        if(setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mcreq,
                      sizeof(mcreq)))
        {
#ifdef _WIN32
            closesocket(fd);
#else
            close(fd);
#endif
            return -1;
        }
    }
    return fd;
}

int sock_close(int fd)
{
#ifndef _WIN32
    shutdown(fd, SHUT_RDWR);
#else
    shutdown(fd, SD_BOTH);
#endif
    return close(fd);
}

//#ifndef _WIN32
//int sock_set_close_on_exec(int fd, bool flag)
//{
//    return (fcntl(fd, F_SETFD, flag ? FD_CLOEXEC : 0) < 0) ? -1 : 0;
//}
//#endif

//int sock_set_no_delay(int fd, bool flag)
//{
//    int ipar = flag ? 1 : 0;
//
//    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&ipar, sizeof(ipar));
//}

//int sock_set_write_buffer(int fd, size_t size)
//{
//    return setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&size, sizeof(size));
//}

//int sock_set_read_buffer(int fd, size_t size)
//{
//    return setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&size, sizeof(size));
//}

//int sock_write(int fd, void* data, size_t size, size_t min)
//{
//    int n;
//    size_t k;
//
//    if(min > size)
//        min = size;
//    for(k = 0; k < min; k += n)
//    {
//        n = write(fd, (char*)data + k, size - k);
//        if(n <= 0)
//            return n;
//    }
//    return k;
//}

//int sock_read(int fd, void* data, size_t size, size_t min)
//{
//    int n;
//    size_t k;
//
//    if(min > size)
//        min = size;
//    for(k = 0; k < min; k += n)
//    {
//        n = read(fd, (char*)data + k, size - k);
//        if(n <= 0)
//            return n;
//    }
//    return k;
//}

//int sock_sendto(int fd, void* data, size_t size, Sockaddr* addr)
//{
//    if(addr)
//        return sendto(fd, (char*)data, size, 0, addr->sa_ptr(), addr->sa_len());
//    return send(fd, (char*)data, size, 0);
//}

int sock_recvfm(int fd, void* data, size_t size, Sockaddr* addr)
{
    socklen_t len = sizeof(struct sockaddr_storage);

    if(addr)
        return recvfrom(fd, (char*)data, size, 0, addr->sa_ptr(), &len);
    return recv(fd, (char*)data, size, 0);
}

// Local Variables:
// compile-command: "make -f Makefile-linux"
// End:
