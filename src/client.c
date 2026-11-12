#include "common.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int verbose = 0, opt;
    log_init_from_env();
    while ((opt = getopt(argc, argv, "v")) != -1) {
        if (opt == 'v') verbose++;
        else { fprintf(stderr, "usage: %s [-v|-vv] [host] [port]\n", argv[0]); return 1; }
    }
    if (verbose > 0) log_set_level(verbose);

    const char *host = (optind < argc)     ? argv[optind]       : "127.0.0.1";
    int port         = (optind + 1 < argc) ? atoi(argv[optind+1]) : 8888;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid address: %s\n", host);
        return 1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); return 1;
    }
    printf("[client] connected. type text to echo, or 'quit'.\n");

    char line[1024], buf[MAX_PAYLOAD];
    msg_hdr_t hdr;

    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\r\n")] = '\0';     /* 同時去掉 \r，避免 Windows 風格換行 */
	if (line[0] == '\0') continue;           /* 空行不送給 server */
        const char *cmd;
        if      (strcmp(line, "quit")    == 0) cmd = "QUIT";
        else if (strcmp(line, "sysinfo") == 0) cmd = "SYSINFO";
        else                                   cmd = "ECHO";

        uint32_t plen = (strcmp(cmd, "ECHO") == 0) ? (uint32_t)strlen(line) : 0;
        if (send_msg(fd, cmd, line, plen) < 0) { perror("send"); break; }

        if (recv_msg(fd, &hdr, buf, sizeof(buf) - 1) != 0) {
            printf("[client] server closed connection\n");
            break;
        }
        buf[hdr.len] = '\0';
        printf("[client] <%.*s>\n%s\n", CMD_LEN, hdr.cmd, buf);
        if (strcmp(cmd, "QUIT") == 0) break;
    }
    close(fd);
    return 0;
}

