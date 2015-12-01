#include <stdio.h>
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
#include "packet.h"

#define BUFFER_SIZE 1500
int rx = 1;

void blaster_usage(void) {
    printf("usage: blaster -s <hostname> -p <port> -r <rate> -n <num> -q <seq_no> -l <length> -c <echo>\n");
    exit(1);
}

void *blaster_echo_rx(void *arg)
{
    int sockfd = *(int *)arg;
	char *buffer=(char *)malloc(sizeof(char)*BUFFER_SIZE);
    int flag = 0;
	struct sockaddr src_addr;
	socklen_t addrlen;
    struct timeval tv;

    tv.tv_sec = 10;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
            perror("Error");
    }

	while(rx){
        int pktlen = 0;
		pktlen = recvfrom(sockfd, buffer, BUFFER_SIZE, flag, &src_addr, &addrlen);
        if (pktlen == 0)
		    printf("rx packet timeout\n");
        else if (pktlen == -1)
            perror("rx packet error: ");
        else
            printf("rx packet from: \n");
	}

    return NULL;
}

void run_blaster(char* hostname, char *port, int numpkts, int pktlen,
        int rate, int echo)
{
	struct addrinfo hints, *res, *p;
	int status;
	char ipstr[INET_ADDRSTRLEN];
	int socketfd;
	char *buffer=(char *)malloc(sizeof(char)*pktlen);
	int flag=0;
    pthread_t rx_thread;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_DGRAM;

	if ((status = getaddrinfo(hostname, port, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		return;
	}

	for (p = res; p != NULL; p = p->ai_next) {
		void *addr;
		char *ipver;

		// get the pointer to the address itself,
		// different fields in IPv4 and IPv6:
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

	socketfd=socket(p->ai_family, p->ai_socktype, p->ai_protocol);
	if(socketfd < 0 ){
		perror("Socket open failed: ");
	}

	useconds_t time=1000000 / rate;
	memcpy(buffer,"test",5);

    if (echo == 1) {
        pthread_create(&rx_thread, NULL, blaster_echo_rx, &socketfd);
    }

	while(numpkts--){
		int ret;
		ret=sendto(socketfd, buffer, pktlen, flag, p->ai_addr, p->ai_addrlen);
		if(ret == -1){
			perror("Send error: ");
		}
		printf("Send packet to: %s\n",ipstr);
		usleep(time);
	}

    rx = 0;

    if (echo == 1) {
        pthread_join(rx_thread, NULL);
    }
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
                port_str=strdup(optarg);
                break;
            case 'r':
                rate = strtoul(optarg, NULL, 10);
                break;
            case 'n':
                numpkts = strtoul(optarg, NULL, 10);
                break;
            case 'q':
                startseq = strtoul(optarg, NULL, 10);
                break;
            case 'l':
                pktlen = strtoul(optarg, NULL, 10);
                break;
            case 'c':
                echo = strtoul(optarg, NULL, 10);
                if (echo != 0 && echo != 1) {
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

    run_blaster(hostname, port_str, numpkts, pktlen, rate, echo);

    free(hostname);

    return 0;
}
