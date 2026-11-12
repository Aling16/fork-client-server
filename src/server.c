#include "common.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

/* 回收已結束的子行程，避免殭屍行程 */
static void sigchld_handler(int sig)
{
    (void)sig;
    int saved_errno = errno;               /* 保存 errno，避免干擾主程式 */
    while (waitpid(-1, NULL, WNOHANG) > 0) /* 一次可能有多個子行程結束，所以用迴圈 */
        ;
    errno = saved_errno;
}

static void handle_client(int fd)
{
    msg_hdr_t hdr;
    char buf[MAX_PAYLOAD];
    char cmd[CMD_LEN + 1];

    for (;;) {
        int r = recv_msg(fd, &hdr, buf, sizeof(buf));
        if (r != 0) break;

        memcpy(cmd, hdr.cmd, CMD_LEN);
        cmd[CMD_LEN] = '\0';

        if (strcmp(cmd, "ECHO") == 0) {
            send_msg(fd, "OK", buf, hdr.len);
        }else if (strcmp(cmd, "SYSINFO") == 0) {
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
    int port = (argc > 1) ? atoi(argv[1]) : 8888;

    /* 安裝 SIGCHLD 處理函式 */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;  /* 被打斷的系統呼叫自動重啟 */
    if (sigaction(SIGCHLD, &sa, NULL) < 0) { perror("sigaction"); return 1; }

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

    for (;;) {
        struct sockaddr_in cli;
        socklen_t cl = sizeof(cli);
        int cfd = accept(lfd, (struct sockaddr *)&cli, &cl);
        if (cfd < 0) {
            if (errno == EINTR) continue;     /* 被信號打斷，重新 accept */
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {                        /* fork 失敗：關掉連線，繼續服務其他人 */
            perror("fork");
            close(cfd);
            continue;
        }

        if (pid == 0) {
            /* ===== 子行程 ===== */
            close(lfd);                       /* 子行程不需要 listen socket */
            printf("[child %d] client %s:%d connected\n",
                   getpid(), inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));
            handle_client(cfd);
            close(cfd);
            printf("[child %d] client disconnected\n", getpid());
            _exit(0);                         /* 用 _exit，避免重複沖刷父行程的 stdio 緩衝 */
        }

        /* ===== 父行程 ===== */
        close(cfd);                           /* 連線交給子行程，父行程一定要關 */
    }
}
