#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>

void blastee_usage(void) {
    printf("usage: blastee -p <port> -c <echo 0|1>\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    unsigned long int port = UINT_MAX, echo = UINT_MAX;
    int ch;

    if (argc < 5) {
        blastee_usage();
    }
    while ((ch = getopt(argc, argv, "p:c:")) != -1) {
        switch (ch) {
            case 'p':
                port = strtoul(optarg, NULL, 10);
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
    printf("Echo: %s, port: %lu\n", echo == 1 ? "yes": "no", port);
    return 0;
}
