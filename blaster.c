#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

void blaster_usage(void) {
    printf("usage: blaster -s <hostname> -p <port> -r <rate> -n <num> -q <seq_no> -l <length> -c <echo>\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    unsigned long port = UINT_MAX, echo = UINT_MAX;
    unsigned long rate, numpkts, startseq, pktlen;
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

    free(hostname);

    return 0;
}
