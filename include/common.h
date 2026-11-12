#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define CMD_LEN      16
#define MAX_PAYLOAD  4096

/* 通訊協定：固定 20 bytes 標頭 + 可變長度 payload */
typedef struct __attribute__((packed)) {
    uint32_t len;            /* payload 長度（網路位元組序） */
    char     cmd[CMD_LEN];   /* 命令字串，例如 "ECHO" */
} msg_hdr_t;

/* 一定送滿 / 收滿 n bytes */
ssize_t send_all(int fd, const void *buf, size_t n);
ssize_t recv_all(int fd, void *buf, size_t n);

/* 送出 / 接收一個完整訊息
 * recv_msg 回傳：0 成功，1 對方正常關閉，-1 系統錯誤，-2 協定錯誤 */
int send_msg(int fd, const char *cmd, const void *payload, uint32_t len);
int recv_msg(int fd, msg_hdr_t *hdr, char *payload, size_t cap);

/* 取得系統資訊並格式化成文字，成功回傳 0，失敗回傳 -1 */
int get_sysinfo(char *buf, size_t cap);

#endif
