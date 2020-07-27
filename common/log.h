/**
 * file    log.h
 * brief   log関連
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

#ifndef __ANC_LOG_H__
#define __ANC_LOG_H__

#include <stdio.h> 
#include <stdint.h>
#include <stdarg.h>
#include <linux/types.h>

/* prototype */
void mlog_init(void);
void mlog(const char *fmt, ...);
void evtlog_init(void);
void evtlog(char *, unsigned long, unsigned long, unsigned char *);
void write_log_stat(FILE *, const char*);
void write_log_mlog(FILE *, const char*);
void write_log_evtlog(FILE *, const char*);
void write_log_cli(FILE *, const char*);
void write_log_stat2(FILE *, const char*);
void write_log_svr(FILE *, const char*);
FILE *open_log(time_t);

/*
    log識別文字列(dump)
*/
#define DUMP_MESSAGE_F        "Front Translator Log"
#define DUMP_MESSAGE_B        "Backend Translator Log"
#define DUMP_STAT_NAME        "STATISTICS:"
#define DUMP_MLOG_NAME        "MLOG:"
#define DUMP_EVTLOG_NAME      "EVENTLOG:"
#define DUMP_CLIENT_NAME      "CLIENT INFO:"
#define DUMP_STAT2_NAME       "POLICY STATUS:"
#define DUMP_SERVER_NAME      "BACKEND TRANSLATOR INFO:"

#define MLOG_DATA_LEN   112
#define MAX_MLOG        256

#define ELOG_DATA_LEN 56
#define MAX_ELOG 2048

#define TMP_SIZE (32*1024)

/*
    evtlog (event log)
*/
struct evtlog_data{
    unsigned int seqno;     /* sequence number */
    uint32_t trace_id;      /* trace id */
    uint64_t time;          /* tsc timestamp */
    unsigned long info1;    /* info1 */
    unsigned long info2;    /* info2 */
    unsigned char e_data[ELOG_DATA_LEN];    /* additional data */
};

/*
    mlog (message log)
*/
struct mlogdata {
    unsigned int seqno;     /* sequence number */
    uint _pad;
    uint64_t time;          /* tsc timestamp */
    char m_data[MLOG_DATA_LEN];     /* message */
};

/*
    log control data
*/
struct log_ctl {
    unsigned char tr_on;    /* trace control */
    unsigned char _pad;
    unsigned short pos;     /* log position */ 
    unsigned int seqno;     /* log sequence number */

    time_t starttime;       /* start time(秒）*/
    uint64_t tsc;           /* 起動時tsc値 */
    // double cpu_clock;    /* tsc clock(外部で処理する場合必要) */
};

#endif
