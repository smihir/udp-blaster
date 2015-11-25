// Server.

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

#define BUFFER_SIZE 1500

void run_blastee(char*  port, int echo);


void blastee_usage(void) {
    printf("usage: blastee -p <port> -c <echo 0|1>\n");
    exit(1);
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
                port_str=strdup(optarg);
                break;
            case 'c':
                echo = strtoul(optarg, NULL, 10);
                if (echo != 0 && echo != 1) {
                    blastee_usage();
                }
                break;
            case '?':
            default:
                blastee_usage();
            }
    }

    run_blastee(port_str, echo);

    printf("Echo: %s, port: %lu\n", echo == 1 ? "yes": "no", port);
    return 0;
}


void run_blastee(char* port, int echo){

	struct addrinfo hints, *res, *p;
	int status;
	char ipstr[INET_ADDRSTRLEN];
	int socketfd;
	char *buffer=(char *)malloc(sizeof(char)*BUFFER_SIZE);
	int flag=0;
//	unsigned long pktlen;

	struct sockaddr src_addr;
	socklen_t addrlen;


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_DGRAM;

	if ((status = getaddrinfo(NULL,port, &hints, &res)) != 0) {
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
	if(socketfd < 0){
		perror("Socket open failed: ");
	}

	if(bind(socketfd, p->ai_addr, p->ai_addrlen)!=0){
		perror("Socket binding failed");
		return;
	}

	while(1){
		addrlen=sizeof(struct sockaddr);
		recvfrom(socketfd, buffer, BUFFER_SIZE, flag, &src_addr, &addrlen);


		printf("Recived packet from: %s::%d\n",buffer,((struct sockaddr_in *)&src_addr)->sin_family);
		inet_ntop(AF_INET,&(((struct sockaddr_in *)&src_addr)->sin_addr), ipstr, sizeof ipstr);
		printf("IP: %s\n", ipstr);
		if(echo == 1){
			int ret=sendto(socketfd, buffer, BUFFER_SIZE, flag, &src_addr, addrlen);
			if(ret == -1){
				perror("Send error: ");
			}
			printf("ECHO: %s\n",buffer);

		}

	}

	freeaddrinfo(res);

}
