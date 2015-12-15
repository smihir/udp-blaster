#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include "packet.h"

#define BUFFER_SIZE (50 * 1024) + 9 //50KB
#define BLASTER_RX_TIMEOUT 5
int rx = 1;

void blaster_usage(void)
{
    printf("usage: blaster -s <hostname> -p <port> -r <rate> -n <num> -q <seq_no> -l <length> -c <echo>\n");
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

void *blaster_echo_rx(void *arg)
{
    int sockfd = *(int *)arg;
    char *buffer=(char *)malloc(sizeof(char)*BUFFER_SIZE);
    int flag = 0;
    struct sockaddr src_addr;
    socklen_t addrlen;
    struct timeval tv;
    char preamble[20];

    tv.tv_sec = BLASTER_RX_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error");
    }

    while(rx) {
        int pktlen = 0;

        pktlen = recvfrom(sockfd, buffer, BUFFER_SIZE, flag, &src_addr, &addrlen);

        if (pktlen == 0) {
#if defined(DEBUG)
            printf("rx packet timeout\n");
#endif
        } else if (pktlen == -1) {
#if defined(DEBUG)
            perror("rx packet error");
#endif
        } else {
            struct packet_header *hdr;
            char *data;

            hdr = (struct packet_header *)buffer;
            data = buffer + sizeof(struct packet_header);

            snprintf(preamble, sizeof(preamble), "echo %u: ",
                     ntohl(hdr->sequence));
            print_data(preamble, data, ntohl(hdr->length) < 4 ?
                                       ntohl(hdr->length) : 4);
        }
    }

    return NULL;
}

void run_blaster(char* hostname, char *port, int numpkts, int pktlen,
        int rate, int echo, unsigned int seq)
{
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET_ADDRSTRLEN];
    int socketfd;
    char *buffer;
    int flag=0;
    int sz = BUFFER_SIZE;
    useconds_t time = 1000000 / rate;
    pthread_t rx_thread;
    char preamble[20];

    buffer = (char *)malloc(sizeof(char) * pktlen +
                            sizeof(struct packet_header));
    if (buffer == NULL) {
        printf("Memory allocation failed\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((status = getaddrinfo(hostname, port, &hints, &res)) != 0) {
        printf("getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    for (p = res; p != NULL; p = p->ai_next) {
        void *addr;
        char *ipver;

        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *) p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }

        // convert the IP to a string and print it:
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        printf("  %s: %s\n", ipver, ipstr);
        break;
    }

    socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (socketfd < 0) {
        perror("Socket open failed: ");
    }

    if (setsockopt(socketfd, SOL_SOCKET, SO_SNDBUF, &sz, sizeof(sz)) < 0) {
        perror("Error");
    }

    if (echo == 1) {
        pthread_create(&rx_thread, NULL, blaster_echo_rx, &socketfd);
    }

    while(numpkts--) {
        int ret;
        struct packet_header *hdr;
        char *data;

        hdr = (struct packet_header *)buffer;
        if (numpkts == 0) {
            hdr->type = 'E';
        } else {
            hdr->type = 'D';
        }
        hdr->sequence = htonl(seq);
        hdr->length = htonl(pktlen);
        data = buffer + sizeof(struct packet_header);

        seq += pktlen;

        ret = sendto(socketfd, buffer, pktlen + sizeof(struct packet_header),
                     flag, p->ai_addr, p->ai_addrlen);
        if(ret == -1) {
            perror("Send error: ");
        } else {
            snprintf(preamble, sizeof(preamble), "%u: ",
                     ntohl(hdr->sequence));
            print_data(preamble, data, ntohl(hdr->length) < 4 ?
                                       ntohl(hdr->length) : 4);
        }

        usleep(time);
    }

    rx = 0;

#if defined(WAIT_FOR_RX)
    if (echo == 1) {
        pthread_join(rx_thread, NULL);
    }
#endif

    free(buffer);
}

int main(int argc, char *argv[])
{
    unsigned long port = UINT_MAX, echo = UINT_MAX;
    unsigned long rate, numpkts, startseq, pktlen;
    char *port_str;
    char *hostname;
    int ch;

    if (argc < 15) {
        blaster_usage();
    }

    while ((ch = getopt(argc, argv, "s:p:r:n:q:l:c:")) != -1) {
        switch (ch) {
            case 's':
                hostname = strdup(optarg);
                break;
            case 'p':
                port = strtoul(optarg, NULL, 10);
                if (port <= 1024 || port > 65536) {
                    printf("Invalid Port\n");
                    blaster_usage();
                }
                port_str=strdup(optarg);
                break;
            case 'r':
                rate = strtoul(optarg, NULL, 10);
                if (rate == 0 && errno == EINVAL) {
                    blaster_usage();
                }
                break;
            case 'n':
                numpkts = strtoul(optarg, NULL, 10);
                if (numpkts == 0 && errno == EINVAL) {
                    blaster_usage();
                }
                break;
            case 'q':
                startseq = strtoul(optarg, NULL, 10);
                if (startseq == 0 && errno == EINVAL) {
                    blaster_usage();
                }
                break;
            case 'l':
                pktlen = strtoul(optarg, NULL, 10);
                if (pktlen == 0 && errno == EINVAL) {
                    blaster_usage();
                }
                break;
            case 'c':
                echo = strtoul(optarg, NULL, 10);
                if (echo != 0 && echo != 1) {
                    blaster_usage();
                }

                if (echo == 0 && errno == EINVAL) {
                    blaster_usage();
                }
                break;
            case '?':
            default:
                blaster_usage();
            }
    }

    printf("hostname: %s, port: %lu, rate: %lu, numpkts: %lu, "
            "startseq: %lu, pktlen: %lu, echo: %s\n", hostname, port, rate,
            numpkts, startseq, pktlen, echo == 1 ? "yes": "no");

    // One extra packet for END
    run_blaster(hostname, port_str, numpkts + 1, pktlen, rate, echo, startseq);

    free(hostname);

    return 0;
}
