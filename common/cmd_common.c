/**
 * file    cmd_common.c
 * brief   コマンド処理(共通処理)
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "cmd_common.h"

static void ssat_handler_hup(int sig);
static void proc_ud_request(int soc);
static void proc_sig_check(void);
static void trace_control(uchar ctl);

/*
    @brief コマンド等処理
*/
int
command_proc(void)
{
    struct timeval tv;
    int soc;
#ifdef PROFILE
    uint64_t start, end;
#endif

    /* unix domain socket */
    if ((soc = init_ud_socket()) < 0 ) {
        return -1;
    }

    /* pidファイル処理 */
    if (init_pid() < 0) {
        close(soc);
        return -1;
    }

    signal (SIGHUP, ssat_handler_hup);

    timeout_init(&tv);

#ifdef PROFILE
    start = rdtsc();
#endif
    for ( ;; ) {
        fd_set fds;
        int retval;

        FD_ZERO(&fds);
        FD_SET(soc, &fds);

        retval = select(soc + 1, &fds, NULL, NULL, &tv);

        if (likely(retval > 0)) {
            proc_ud_request(soc);
        } else {
            /* signal check */
            proc_sig_check();
        }
        timeout_init(&tv);
#ifdef PROFILE
        end = rdtsc();
        if ( get_sec(end - start) > PROFILE_SEC) {
            break;
        }
#endif
    }
    return 0;
}

/*
    外部からの通信処理
*/
static void 
proc_ud_request(int soc)
{
    struct sockaddr_un ud_addr;
    int len;
    socklen_t buf_size;
    ud_request_t *ud_req;
    ud_resp_t *ud_resp;
    unsigned char buf[sizeof(ud_resp_t)+8];
    int result = NET_REQ_NG;
    int req_id, r_code = 0;

    /* 要求メッセージの解読 */
    buf_size = sizeof(struct sockaddr_un);
    if ((len = recvfrom
         (soc, buf, sizeof(buf), 0, (struct sockaddr *) &ud_addr,
         &buf_size)) < 0) {
        if (errno == EAGAIN) {
            return;
        }
    }

    if (len != sizeof(ud_request_t)) {
        SASAT_STAT(cmd_illegal);
        evtlog("udr1", len, 0, (uchar*)buf);
        return;
    }

    ud_req = (ud_request_t *) buf;

    if (ud_req->magic_no != UD_MAGIC_NO) {
        SASAT_STAT(cmd_illegal);
        evtlog("udr2", len, 0, (uchar*)buf);
        return;
    }

    evtlog("udr0", len, 0, (uchar*)buf);

    req_id = ud_req->req_id;

    /* 指示された処理毎の振り分け */
    switch (req_id) {
    case UD_DUMP_REQ:
        /* logダンプ処理 */
        SASAT_STAT(cmd_dump_req);
        log_dump(ud_req->req_data);
        break;

#ifdef FRONT_T
    case UD_POLICY_UPD:
        /* 振り分けファイルの更新 */
        SASAT_STAT(cmd_upd_policy);
        update_policy();
        break;
#endif
    case UD_EVTR:
        /* イベントトレースの取得on/off */
        SASAT_STAT(cmd_trace);
        if (ud_req->req_data > 1) {
            r_code = 2;
        } else {
            trace_control(ud_req->req_data);
        }
        break;
    default:
        SASAT_STAT(cmd_illegal);
        r_code = 1;
        break;
    }

    /* 応答メッセージの送信 */
    if (!r_code) {
        result = NET_REQ_OK;
    }

    ud_resp = (ud_resp_t *)buf;

    ud_resp->result = result;
    ud_resp->reason_code = r_code;

    if ((sendto
            (soc, ud_resp, sizeof(ud_resp_t), 0,
            (struct sockaddr *) &ud_addr, buf_size)) < 0) {
        evtlog("cmd0",soc, errno, NULL);
    }
    return;
}

/*
    @brief event trace on/offコントロール
*/
static void
trace_control(uchar ctl)
{
    static const char *str[] = {"off", "on"};
    evtlog_ctl.tr_on = ctl;

    mlog("event trace is %s", str[ctl]);
}

/*
    @brief signal(HUP) 振り分け設定再読み込み(front), log(backend)
*/
void
proc_sig_check(void)
{
    if (sig_flg == 1) {
        sig_flg = 0;
#ifdef FRONT_T
        SASAT_STAT(cmd_upd_policy);
        update_policy();
#else
        SASAT_STAT(cmd_dump_req);
        log_dump(0xff);
#endif
    }
}

/*
    @brief signal handler(HUP)
*/
static void
ssat_handler_hup(int sig)
{
    signal(sig, SIG_IGN);
    sig_flg = 1;
    signal(SIGHUP, ssat_handler_hup);
}

/* end */
