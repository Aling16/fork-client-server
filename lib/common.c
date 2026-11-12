#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include "common.h"
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

ssize_t send_all(int fd, const void *buf, size_t n)
{
    const char *p = buf;
    size_t left = n;
    while (left > 0) {
        ssize_t w = send(fd, p, left, MSG_NOSIGNAL);  /* 不觸發 SIGPIPE */
        if (w < 0) {
            if (errno == EINTR) continue;             /* 被信號打斷就重試 */
            return -1;
        }
        p += w;
        left -= w;
    }
    return n;
}

ssize_t recv_all(int fd, void *buf, size_t n)
{
    char *p = buf;
    size_t left = n;
    while (left > 0) {
        ssize_t r = recv(fd, p, left, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0)                                   /* 對方關閉連線 */
            return (left == n) ? 0 : -1;              /* 0=乾淨關閉；中途斷=錯誤 */
        p += r;
        left -= r;
    }
    return n;
}

int send_msg(int fd, const char *cmd, const void *payload, uint32_t len)
{
    msg_hdr_t h;
    LOG_DBG("send_msg cmd=%s len=%u", cmd, len);
    memset(&h, 0, sizeof(h));
    strncpy(h.cmd, cmd, CMD_LEN - 1);
    h.len = htonl(len);
    if (send_all(fd, &h, sizeof(h)) < 0) return -1;
    if (len > 0 && send_all(fd, payload, len) < 0) return -1;
    return 0;
}

int recv_msg(int fd, msg_hdr_t *hdr, char *payload, size_t cap)
{
    ssize_t r = recv_all(fd, hdr, sizeof(*hdr));
    if (r == 0) return 1;
    if (r < 0)  return -1;

    uint32_t len = ntohl(hdr->len);
    hdr->len = len;
    LOG_DBG("recv_msg cmd=%.*s len=%u", CMD_LEN, hdr->cmd, len);

    #ifdef ROBUST
        if (len > cap) return -2;      /* 機制 3：驗證長度欄位 */
    #else
        (void)cap;                     /* 無防護版本：刻意不檢查，避免 unused 警告 */
    #endif

    if (len > 0) {
        r = recv_all(fd, payload, len);
        if (r <= 0) return -1;
    }
    return 0;
}

int get_sysinfo(char *buf, size_t cap)
{
    struct utsname u;
    struct sysinfo si;
    if (uname(&u) < 0 || sysinfo(&si) < 0)
        return -1;

    time_t now = time(NULL);
    struct tm tm;
    char tbuf[64];
    localtime_r(&now, &tm);
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);

    unsigned long unit = si.mem_unit ? si.mem_unit : 1;
    unsigned long total_mb = (si.totalram * unit) >> 20;
    unsigned long free_mb  = (si.freeram  * unit) >> 20;

    long up = si.uptime;
    int n = snprintf(buf, cap,
        "Hostname   : %s\n"
        "OS / Kernel: %s %s\n"
        "Arch       : %s\n"
        "Uptime     : %ldd %ldh %ldm\n"
        "CPU cores  : %ld\n"
        "RAM        : %lu MB total, %lu MB free\n"
        "Load avg   : %.2f %.2f %.2f\n"
        "Server time: %s\n"
        "Served by  : pid %d\n",
        u.nodename, u.sysname, u.release, u.machine,
        up / 86400, (up % 86400) / 3600, (up % 3600) / 60,
        sysconf(_SC_NPROCESSORS_ONLN),
        total_mb, free_mb,
        si.loads[0] / 65536.0, si.loads[1] / 65536.0, si.loads[2] / 65536.0,
        tbuf, getpid());

    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

/* ===== log ===== */
static int g_log_level = 0;       /* 0=只有錯誤 1=info 2=debug */

void log_set_level(int lvl)
{
    if (lvl < 0) lvl = 0;
    if (lvl > 2) lvl = 2;
    g_log_level = lvl;
}

void log_init_from_env(void)
{
    const char *e = getenv("LOG_LEVEL");
    if (e) log_set_level(atoi(e));
}

void log_write(int lvl, const char *file, int line, const char *fmt, ...)
{
    if (lvl > g_log_level) return;            /* 執行期過濾 */
    static const char *tag[] = { "ERR", "INF", "DBG" };

    fprintf(stderr, "[%s pid=%d %s:%d] ", tag[lvl], getpid(), file, line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
