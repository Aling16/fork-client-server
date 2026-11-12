#include "common.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef ROBUST
/* 機制 1：回收已結束的子行程，避免殭屍行程 */
static void sigchld_handler(int sig)
{
    (void)sig;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
    errno = saved_errno;
}
#endif

static void handle_client(int fd)
{
    msg_hdr_t hdr;
    char buf[MAX_PAYLOAD];
    char cmd[CMD_LEN + 1];

    for (;;) {
        int r = recv_msg(fd, &hdr, buf, sizeof(buf));
        if (r == 1) {
            LOG_INFO("client closed connection");
            break;
        }
        if (r == -2) {                         /* 機制 3 觸發：長度不合理 */
            LOG_ERR("payload too large (len=%u > %d), rejecting",
                    hdr.len, MAX_PAYLOAD);
            const char *e = "payload too large";
            send_msg(fd, "ERR", e, strlen(e));
            break;
        }
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)   /* 機制 2 觸發 */
                LOG_INFO("idle timeout (%ds), closing connection", IDLE_TIMEOUT_SEC);
            else
                LOG_ERR("recv failed: %s", strerror(errno));
            break;
        }

        memcpy(cmd, hdr.cmd, CMD_LEN);
        cmd[CMD_LEN] = '\0';
        LOG_DBG("dispatch cmd='%s' len=%u", cmd, hdr.len);

        if (strcmp(cmd, "ECHO") == 0) {
            send_msg(fd, "OK", buf, hdr.len);
        } else if (strcmp(cmd, "SYSINFO") == 0) {
            char out[1024];
            if (get_sysinfo(out, sizeof(out)) == 0) {
                send_msg(fd, "OK", out, strlen(out));
            } else {
                const char *e = "sysinfo failed";
                send_msg(fd, "ERR", e, strlen(e));
            }
        } else if (strcmp(cmd, "QUIT") == 0) {
            send_msg(fd, "BYE", NULL, 0);
            break;
        } else {
            const char *e = "unknown command";
            send_msg(fd, "ERR", e, strlen(e));
        }
    }
}

int main(int argc, char **argv)
{
    int port = 8888, verbose = 0, opt;

    log_init_from_env();
    while ((opt = getopt(argc, argv, "p:v")) != -1) {
        switch (opt) {
        case 'p': port = atoi(optarg); break;
        case 'v': verbose++;           break;
        default:
            fprintf(stderr, "usage: %s [-v|-vv] [-p port | port]\n", argv[0]);
            return 1;
        }
    }
    if (optind < argc) port = atoi(argv[optind]);
    if (verbose > 0) log_set_level(verbose);

#ifdef ROBUST
    /* 機制 1：安裝 SIGCHLD 處理函式 */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) < 0) { perror("sigaction"); return 1; }
#endif

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) { perror("socket"); return 1; }

    int on = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(lfd, 128) < 0) { perror("listen"); return 1; }

    printf("[server] parent pid=%d listening on port %d\n", getpid(), port);
#ifdef ROBUST
    printf("[server] ROBUST mode: ON (idle timeout %ds)\n", IDLE_TIMEOUT_SEC);
#else
    printf("[server] ROBUST mode: OFF\n");
#endif
    fflush(stdout);

    for (;;) {
        struct sockaddr_in cli;
        socklen_t cl = sizeof(cli);
        int cfd = accept(lfd, (struct sockaddr *)&cli, &cl);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            LOG_ERR("accept failed: %s", strerror(errno));
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            LOG_ERR("fork failed: %s", strerror(errno));
            close(cfd);
            continue;
        }

        if (pid == 0) {
            /* ===== 子行程 ===== */
            close(lfd);
#ifdef ROBUST
            /* 機制 2：設定接收逾時 */
            struct timeval tv = { .tv_sec = IDLE_TIMEOUT_SEC, .tv_usec = 0 };
            if (setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
                LOG_ERR("setsockopt SO_RCVTIMEO: %s", strerror(errno));
#endif
            LOG_INFO("client %s:%d connected",
                     inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));
            handle_client(cfd);
            close(cfd);
            LOG_INFO("client disconnected");
            _exit(0);
        }

        /* ===== 父行程 ===== */
        close(cfd);
    }
}
