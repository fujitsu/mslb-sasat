/**
 * file    command_proc.c
 * brief   コマンド処理
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

#include <stdlib.h>
#include <time.h> 

#include "option.h"
#include "anycast.h"
#include "stat.h"
#include "init.h"
#include "util_inline.h"
#include "policy.h"
#include "val.h"
#include "stat.h"

static void timeout_init(struct timeval *tv);
static void log_dump(unsigned char);
static void update_policy(void);

/* static valiables */
static volatile int sig_flg;

void send_garp(struct ifdata*, uint64_t*, uint);

/* 共通ファイル読み込み */
#include "cmd_common.c"

/*
    @brief タイムアウト時間設定
*/
static void
timeout_init(struct timeval *tv)
{
    static uint64_t start_time;

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

        sprintf(buff, DUMP_MESSAGE_F" : %s", ctime_r(&time.tv_sec, bt));
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
        if (flag & LOG_STAT2) {
            write_log_stat2(fp, buff);
        }
        if (flag & LOG_SVR)  {
            write_log_svr(fp, buff);
        }
        free(buff);
        fclose(fp);
    }
}

/*
    @brief 振分け設定ファイル再読み込み
*/
static void
update_policy(void)
{
    int i, count;
    pthread_t ret;

    mlog("update policy table");

    count = nt_info.count;
    /* ネットワーク処理スレッドの停止 */
    for (i = 0; i < count; i++) {
        nt_info.th[i].cancel_end = 0;
        pthread_cancel(nt_info.th[i].tid);
    }

    /* 停止待ち */
    anycast_sleep(10);
    for (i = 0; i < count;) {
        if (nt_info.th[i].cancel_end == 1) {
            i++;            
        } else {
            anycast_sleep(0);
        }
    }

    destroy_policy_table();
    destroy_svr_tbl();

    init_svr_mng_table();
    init_policy_table(1);

    (void)get_interface_info(if_ingress, if_egress);

    get_policy();
    ret = create_net_thread(front_ingress1);
    if (!ret) {
        return;
    }
    nt_info.th[0].tid = ret;
}

/* end */
