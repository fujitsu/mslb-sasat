/**
 * file    cmd_common.h
 * brief   コマンド処理(共通処理)
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef __CMD_COMMON_H__
#define __CMD_COMMON_H__

/* 通信フォーマット */
typedef struct ud_request_s {
    int magic_no;
    unsigned char req_id;
    unsigned char req_data;
    unsigned char _rsv[2];
} ud_request_t;

typedef struct ud_resp_s {
    int magic_no;
    unsigned char req_id;
    unsigned char req_data;
    unsigned char result;
    unsigned char reason_code;
} ud_resp_t;

/* unix domainソケット通信関連定義 */
enum {
    UD_MAGIC_NO = 0x46726e74,
    RESULT_OK = 0,
    RESULT_NG = 1,

    /* command code */
    UD_POLICY_UPD = 1,
    UD_DUMP_REQ,
    UD_EVTR
};

/*
    logコマンドビットマップ
*/
enum {
    LOG_STAT    = 1,
    LOG_MLOG    = 1 << 1,
    LOG_EVTLOG  = 1 << 2,
    LOG_CLI     = 1 << 3,
    LOG_STAT2   = 1 << 4,
    LOG_SVR     = 1 << 5,
};

#endif
