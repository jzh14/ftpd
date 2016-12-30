#ifndef NETWORK_H
#define NETWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

static const int CONST_YES = 1;
static const int PORT_LENGTH = 6;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int sendall(int s, const char *buf, int len, int flag) {
    int total = 0;        // how many bytes we've sent
    int bytesleft = len; // how many we have left to send
    int n;

    while(total < len) {
        n = send(s, buf + total, bytesleft, flag);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    len = total; // return number actually sent here
    return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}

unsigned long getPublicIP(const char *iface = "eth0") {
    struct ifaddrs * ifAddrStruct = NULL, * ifa = NULL;
    void * tmpAddrPtr = NULL;

    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa ->ifa_addr->sa_family == AF_INET) { // Check it is IPv4
            char mask[INET_ADDRSTRLEN];
            void* mask_ptr = &((struct sockaddr_in*) ifa->ifa_netmask)->sin_addr;
            inet_ntop(AF_INET, mask_ptr, mask, INET_ADDRSTRLEN);
            if (strcmp(mask, "255.0.0.0") != 0) {
                // Is a valid IPv4 Address
                if (strcmp(ifa->ifa_name, iface) == 0) {
                    return ((struct sockaddr_in *) ifa->ifa_addr)->sin_addr.s_addr;
                }
            }
        }
    }
    if (ifAddrStruct != NULL)
        freeifaddrs(ifAddrStruct);
    return 0;
}

int getAddr(const char* port, struct addrinfo*& ai, const char* host = NULL) {
    struct addrinfo hints;
    int rv;

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(host, port, &hints, &ai)) != 0) {
        //if ((rv = getaddrinfo("101.6.163.59", port, &hints, &ai)) != 0) {
        fprintf(stderr, "ftpd: %s\n", gai_strerror(rv));
        return -1;
    }
    return 0;
}

int bindSocket(const char *port) {
    struct addrinfo *ai, *p;
    int listener;

    if (int result = getAddr(port, ai) < 0) {
        return result;
    }


    for (p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &CONST_YES, sizeof(CONST_YES));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "ftpd: failed to bind\n");
        return -2;
    }

    freeaddrinfo(ai); // all done with this

    return listener;
}
#endif
