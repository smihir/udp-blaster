#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/errno.h>
#include <sys/time.h>
#include "packet.h"

#define BUFFER_SIZE (50 * 1024) + 9 //50KB
#define BLASTEE_RX_TIMEOUT 5
#define MAX_TIME 10

void blastee_usage(void)
{
    printf("usage: blastee -p <port> -c <echo 0|1>\n");
    exit(1);
}

void print_data(char *preamble, char *data, int len)
{
    int i;

    printf("%s", preamble);
    for (i = 0; i < len; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

double elapsed_time(struct timeval *tv_first, struct timeval *tv_end)
{
    double elapsed_msec;

    elapsed_msec = (tv_end->tv_sec - tv_first->tv_sec) * 1000;
    elapsed_msec += (tv_end->tv_usec - tv_first->tv_usec) / 1000;

    return elapsed_msec;
}

void print_summarystats(struct timeval *tv_first, struct timeval *tv_end,
                        unsigned int numpkts_rx, unsigned int numbytes_rx)
{
    double elapsed_msec;

    elapsed_msec = elapsed_time(tv_first, tv_end);

    printf("packets received: %u \nbytes_received: %u \n"
           "average packets per second: %f \naverage bytes per second: %f \n"
           "duration (ms): %f \n",
           numpkts_rx, numbytes_rx, numpkts_rx / (elapsed_msec / 1000),
           numbytes_rx / (elapsed_msec / 1000), elapsed_msec);
}

void run_blastee(char* port, unsigned long int echo)
{
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET_ADDRSTRLEN];
    int socketfd;
    char *buffer;
    int flag = 0;
    int sz = BUFFER_SIZE;
    struct timeval tv;
    struct sockaddr src_addr;
    socklen_t addrlen;
    int do_break = 0;
    unsigned int numpkts_rx = 0, numbytes_rx = 0;
    struct timeval tv1, tv_first, tv_end;
    char preamble[50];

    buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    if (buffer == NULL) {
        printf("Memory allocation failed\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
        printf("getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    for (p = res; p != NULL; p = p->ai_next) {
        void *addr;
        char *ipver;

        if (p->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *) p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }

        // convert the IP to a string and print it:
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        printf("%s: %s\n", ipver, ipstr);
        break;
    }

    socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (socketfd < 0) {
        perror("Socket open failed: ");
    }

    //SPEC: exit if no packet for 5 secs
    tv.tv_sec = BLASTEE_RX_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error");
    }

    if (setsockopt(socketfd, SOL_SOCKET, SO_SNDBUF, &sz, sizeof(sz)) < 0) {
        perror("Error");
    }

    if (bind(socketfd, p->ai_addr, p->ai_addrlen) != 0) {
        perror("Socket binding failed");
        return;
    }

    // in case test ends without receiving any packet
    // we should print positive test runtime while
    // printing the summay stats
    gettimeofday(&tv_first, NULL);

    while(1) {
        int pktlen;
        struct sockaddr_in *sin;
        char *ipver;
        void *addr;
        struct packet_header *hdr;
        char *data;
        struct tm *tm;

        pktlen = recvfrom(socketfd, buffer, BUFFER_SIZE, flag, &src_addr, &addrlen);

        if (pktlen != -1) {
            gettimeofday(&tv1, NULL);

            numpkts_rx++;
            numbytes_rx += pktlen;

            if (numpkts_rx == 1) {
                tv_first = tv1;
            }

            sin = (struct sockaddr_in *)&src_addr;

            if (p->ai_family == AF_INET) {
                struct sockaddr_in *ipv4 = (struct sockaddr_in *) p->ai_addr;
                addr = &(ipv4->sin_addr);
                ipver = "IPv4";
            } else {
                struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) p->ai_addr;
                addr = &(ipv6->sin6_addr);
                ipver = "IPv6";
            }

            inet_ntop(sin->sin_family, addr, ipstr, sizeof ipstr);

            hdr = (struct packet_header *)buffer;
            data = buffer + sizeof(struct packet_header);
            tm = localtime(&tv1.tv_sec);

            snprintf(preamble, sizeof(preamble),
                     "%s %s %d %u %d:%02d:%02d.%06d: ",
                     ipstr, port, pktlen, ntohl(hdr->sequence), tm->tm_hour,
                     tm->tm_min, tm->tm_sec, tv1.tv_usec);
            print_data(preamble, data, ntohl(hdr->length) < 4 ?
                                       ntohl(hdr->length) : 4);

            if (hdr->type == 'E')
                do_break = 1;
            hdr->type = 'C';
        }

        if (echo == 1 && pktlen != -1) {
            addrlen = sizeof(src_addr);
            if (sendto(socketfd, buffer, pktlen, flag, &src_addr, addrlen) == -1) {
                perror("Send Error:");
            }
        }

        //SPEC: exit if no packet for 5 secs
        if ((pktlen == -1 && errno == EAGAIN) || do_break == 1) {

            gettimeofday(&tv_end, NULL);

            print_summarystats(&tv_first, &tv_end, numpkts_rx, numbytes_rx);

            break;
        }
    }

    freeaddrinfo(res);
    free(buffer);
}

int main(int argc, char *argv[])
{
    unsigned long int port = UINT_MAX, echo = UINT_MAX;
    char *port_str;
    int ch;

    if (argc < 5) {
        blastee_usage();
    }

    while ((ch = getopt(argc, argv, "p:c:")) != -1) {
        switch (ch) {
            case 'p':
                port = strtoul(optarg, NULL, 10);
                if (port <= 1024 || port > 65536) {
                    printf("Invalid Port\n");
                    blastee_usage();
                }
                port_str = strdup(optarg);
                break;
            case 'c':
                echo = strtoul(optarg, NULL, 10);
                if (echo != 0 && echo != 1) {
                    blastee_usage();
                }

                if (echo == 0 && errno == EINVAL) {
                    blastee_usage();
                }
                break;
            case '?':
            default:
                blastee_usage();
            }
    }

    printf("Echo: %s, port: %lu\n", echo == 1 ? "yes": "no", port);

    run_blastee(port_str, echo);

    return 0;
}
