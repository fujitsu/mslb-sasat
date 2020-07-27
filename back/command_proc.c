/**
 * file    command_proc.c
 * brief   コマンド処理
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT) Yagi
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "option.h"
#include "stat.h"
#include "init.h"
#include "util_inline.h"
#include "val.h"
#include "anycast.h"

static void timeout_init(struct timeval *tv);
static void log_dump(unsigned char);

/* static valiables */
static volatile int sig_flg;

void send_garp(struct ifdata*, uint64_t*, uint);

#include "cmd_common.c"

/*
    @brief タイムアウト時間設定
*/
static void
timeout_init(struct timeval *tv)
{
    static uint64_t start_time;

    /* 定期的にGARPを送信する */
    send_garp(if_ingress, &start_time, 40);
    timeout_set(tv, 10);
}

/*
    @brief logおよび統計情報をファイルに書き出す
    @param flag ビットマップ
*/
static void
log_dump(unsigned char flag)
{
    if (flag) {
        FILE *fp;
        char *buff;
        struct timespec time;
        char bt[32];

        clock_gettime(CLOCK_REALTIME, &time);

        fp = open_log(time.tv_sec);
        if (fp == NULL) {
            return;
        }

        /* 作業用バッファ確保 */
        buff = malloc(TMP_SIZE);
        if (buff == NULL) {
            fclose(fp);
            return;
        }

        sprintf(buff, DUMP_MESSAGE_B" : %s", ctime_r(&time.tv_sec, bt));
        fwrite(buff, strlen(buff), 1, fp);

        if (flag & LOG_STAT) {
            write_log_stat(fp, buff);
        }
        if (flag & LOG_MLOG) {
            write_log_mlog(fp, buff);
        }
        if (flag & LOG_EVTLOG) {
            write_log_evtlog(fp, buff);
        }
        if (flag & LOG_CLI) {
            write_log_cli(fp, buff);
        }

        free(buff);
        fclose(fp);
    }
}

/* end */

