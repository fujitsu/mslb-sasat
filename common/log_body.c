/**
 * file    log_body.c 
 * brief   log処理
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h> 
#include <arpa/inet.h>

#include "log.h"
#include "val.h"
#include "util_inline.h"
#include "stat.h"
#include "stat_common.h"
#ifdef FRONT_T
#include "policy.h"
#endif

/*
    @brief MLOG初期化
*/
void mlog_init(void)
{
    struct log_ctl *ctl;
    struct timespec time;

    ctl = &mlog_ctl;
    ctl->tr_on = 1;
    ctl->seqno = 0;
    ctl->pos = 0;
    clock_gettime(CLOCK_REALTIME, &time);
    ctl->starttime = time.tv_sec;
    ctl->tsc = rdtsc();
    /* ctl->cpu_clock = get_clock(); */
}

/*
    @brief mlog処理
*/
void mlog(const char *fmt, ...)
{
    struct mlogdata *tr;   /* trace area    */
    va_list args;          /* value list    */

    va_start(args, fmt);
    tr = &mlog_data[mlog_ctl.pos];
    vsnprintf(&tr->m_data[0], MLOG_DATA_LEN, fmt, args);
    tr->time = rdtsc();
    tr->seqno = ++mlog_ctl.seqno;
    
    mlog_ctl.pos++;
    if (unlikely(mlog_ctl.pos >= MAX_MLOG)) {
        mlog_ctl.pos = 0;
    }
    va_end(args);
}

/*
    @brief event log初期化
*/
void evtlog_init(void)
{
    struct timespec time;

    evtlog_ctl.tr_on = EVTLOG_DEFAULT;
    evtlog_ctl.seqno = 0;
    clock_gettime(CLOCK_REALTIME, &time);
    evtlog_ctl.starttime = time.tv_sec;
    evtlog_ctl.tsc = rdtsc();
    /* evtlog_ctl.cpu_clock = get_clock(); */
}

/*
    @brief event log取得
*/
void
evtlog(char *traceid, ulong info1, ulong info2, uchar *data)
{
#ifndef NO_EVTLOG
    if (evtlog_ctl.tr_on) {
        struct evtlog_data *tr;
    
        tr = &evtlog_data[evtlog_ctl.pos];

        tr->seqno = ++evtlog_ctl.seqno;
        tr->trace_id = *(uint32_t*)traceid;
        tr->time = rdtsc();
        tr->info1 = info1;
        tr->info2 = info2;
        if (likely(data != NULL)) {
            memcpy(tr->e_data, data, ELOG_DATA_LEN);
        } else {
            *(uint*)tr->e_data = 0x20202020;
        }
        evtlog_ctl.pos++;
        if (unlikely(evtlog_ctl.pos >= MAX_ELOG)) {
            evtlog_ctl.pos = 0;
        }
    }
#endif
}

/*
    @brief log用時間表示

    @param start 開始時のtsc値
    @param curr  ログ取得時のtsc値
    @param ssec  開始時の秒（epoc)
    @param buff  出力先(24文字以上)
*/
static inline void 
get_time(uint64_t start, uint64_t curr, time_t ssec, char *buff)
{
    struct tm tm_time;
    uint64_t diff = get_ms(curr - start);
    int msec = diff % 1000;
    time_t sec = diff / 1000;
    sec += ssec;
 
    localtime_r(&sec, &tm_time);

    sprintf(buff, "%d-%02d-%02d %02d:%02d:%02d.%03d",
        tm_time.tm_year+1900, /* 年 */
        tm_time.tm_mon+1,     /* 月 */
        tm_time.tm_mday,      /* 日 */
        tm_time.tm_hour,      /* 時 */
        tm_time.tm_min,       /* 分 */
        tm_time.tm_sec,       /* 秒 */
        msec                  /* ミリ秒 */
    );
}

/*
    @brief logファイルオープン
           形式：sasat03-01_12-20-00.log
*/
FILE *
open_log(time_t sec)
{
    struct tm tm;
    char name[128];

    localtime_r(&sec, &tm);
    sprintf(name, LOG_PATH"sasat%02d-%02d_%02d-%02d-%02d.log",
        tm.tm_mon+1,     /* 月 */
        tm.tm_mday,      /* 日 */
        tm.tm_hour,      /* 時 */
        tm.tm_min,       /* 分 */
        tm.tm_sec        /* 秒 */
    );

    return (fopen(name, "w"));
}

#define SEPARATOR   "----------------"

#define WRITE_LOG_MIDDLE(x) \
    if (unlikely(tlen > (TMP_SIZE-(x))) ) { \
        fwrite(buff, tlen, 1, fp); \
        bufp = (char*)buff; \
        tlen = 0; \
        anycast_sleep(0); \
    }\

static const char IPV6[] = "IPv6\n";
static const char IPV4[] = "IPv4\n";

/*
    @brief statistics情報の書き出し
    @param fp FILE*
    @param buffer
*/
void
write_log_stat(FILE *fp, const char *buff)
{
    char *bufp;
    int i, tlen;

    bufp = (char*)buff;

    sprintf(bufp, "\n"SEPARATOR"\n"DUMP_STAT_NAME"\n");
    tlen = strlen(bufp);
    bufp += tlen; 
    
    for (i = 0; i < STAT_MAX; i++) {
        convert_stat((struct stat_print*)bufp, &sstat[i]);
        int len = strlen(bufp);
        tlen += len;
        bufp += len; 
        WRITE_LOG_MIDDLE(sizeof(struct stat_print));
    }

    fwrite(buff, 1, tlen, fp);
    anycast_sleep(10);
}

/*
    @brief mlogの書き出し
    @param fp FILE*
    @param buffer
*/
void
write_log_mlog(FILE *fp, const char *buff)
{
    struct mlogdata *mlog_first, *mlog_last, *mlog;
    // double clock;
    uint64_t stime;
    time_t ssec;
    int i, seqno, tlen;
    char tb[64];
    char *bufp = (char*)buff;

    sprintf(bufp, "\n"SEPARATOR"\n"DUMP_MLOG_NAME"\n%s", 
        ctime_r(&mlog_ctl.starttime, tb));
    tlen = strlen(bufp);
    bufp += tlen;

    mlog = (struct mlogdata*)malloc(sizeof(struct mlogdata) * MAX_MLOG);
    if (mlog == NULL)  {
        return;
    }

    seqno = mlog_ctl.seqno;
    mlog_first = mlog;
    mlog_last = mlog + (MAX_MLOG-1);

    memcpy(mlog, mlog_data, sizeof(struct mlogdata) * MAX_MLOG);

    /* 最も若い番号を検索 */
    for (i = 0; i < MAX_MLOG; i++) {
        /* 最新のログの一つ先 */
        if (mlog->seqno == seqno) {
            mlog++;
            if (mlog > mlog_last) {
                mlog = mlog_first;
            }
            break;                    
        }
        mlog++;
    }
    
    ssec  = mlog_ctl.starttime;
    stime = mlog_ctl.tsc;
    // clock = mlog_ctl.cpu_clock;

    /* 順番にデータ書き込み */
    for (i = 0; i < MAX_MLOG; i++) {
        /* 有効なログかどうか */
        if (mlog->time) {
            get_time(stime, mlog->time, ssec, tb); 
            sprintf(bufp,"\nseq :%u\ntime:%s\nmsg :%s\n", mlog->seqno,
                tb, mlog->m_data);
            int len = strlen(bufp);
            tlen += len;
            bufp += len;
            WRITE_LOG_MIDDLE(256);
        }
        mlog++;
        if (mlog > mlog_last) {
            mlog = mlog_first;
        }
    }

    fwrite(buff, tlen, 1, fp); 
    free(mlog_first);
    anycast_sleep(10);
}

/*
    @brief event logの書き出し
    @param fp FILE*
    @param buffer
*/
void
write_log_evtlog(FILE *fp, const char *buff)
{
    char tb[64], idb[8];
    struct evtlog_data *elog_first, *elog_last, *elog;
    // double clock;
    uint64_t stime;
    time_t ssec;
    int i, x, tlen;
    uint seqno;
    char *bufp = (char*)buff;

    sprintf(bufp, "\n"SEPARATOR"\n"DUMP_EVTLOG_NAME"\n%s", 
        ctime_r(&evtlog_ctl.starttime, tb));
    tlen = strlen(bufp);
    bufp += tlen;

    /* コピーする領域を確保 */
    elog = (struct evtlog_data*)malloc(sizeof(struct evtlog_data) * MAX_ELOG);
    if (elog == NULL) {
        return;
    }
    
    seqno = evtlog_ctl.seqno;
    elog_first = elog;
    elog_last  = elog + (MAX_ELOG-1);
    /* 実体をコピー */
    memcpy(elog, evtlog_data, sizeof(struct evtlog_data) * MAX_ELOG);

    /* 最も若い番号を検索 */
    for (i = 0; i < MAX_ELOG; i++) {
        if (elog->seqno == seqno) {
            elog++;
            if (unlikely(elog > elog_last)) {
                elog = elog_first;
            }
            break;                    
        }
        elog++;
    }
    
    ssec  = evtlog_ctl.starttime;
    stime = evtlog_ctl.tsc;
    // clock = evtlog_ctl.cpu_clock;
    memset(idb, 0, 8);

    /* 順番にデータ書き込み */
    for (i = 0; i < MAX_ELOG; i++) {
        /* 有効なログかどうか */
        if (elog->time) {
            get_time(stime, elog->time, ssec, tb);
            *(uint32_t*)idb = elog->trace_id;
            sprintf(bufp, "\nseq :%u\ntid :%s\ntime:%s\ndata:%lx/%lx\n", 
                elog->seqno, idb, tb, elog->info1, elog->info2);
            int len = strlen(bufp);
            tlen += len;            
            bufp += len;

            unsigned char *p = elog->e_data;
            for ( x = 0; x < (ELOG_DATA_LEN/8); x++, p +=8) {
                sprintf(bufp, "     %02x %02x %02x %02x %02x %02x %02x %02x\n"
                    ,p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
                int len = strlen(bufp);
                tlen += len;            
                bufp += len;
            }
            WRITE_LOG_MIDDLE(ELOG_DATA_LEN*3+128);
        }
        elog++;
        if (unlikely(elog > elog_last)) {
            elog = elog_first;
        }
    }

    if (likely(tlen != 0)) {
        fwrite(buff, tlen, 1, fp);
    }
    free(elog_first);
    
    anycast_sleep(10);
}

/*
    @brief client log書き出し
    @param fp FILE*
    @param buffer
*/
#ifdef FRONT_T
void
write_log_cli(FILE *fp, const char *buff)
{
    lb_pol_cache_v4_t *pc4;
    lb_pol_cache_v6_t *pc6;
    void *top;
    int num, i, tlen;
    char tmp[INET6_ADDRSTRLEN], tbuf[64];
    char *bufp = (char*)buff;
    time_t ssec = lb_policy_info.starttime;
    uint64_t stime = lb_policy_info.tsc;
    
    sprintf(bufp, "\n"SEPARATOR"\n"DUMP_CLIENT_NAME"\n");
    tlen = strlen(bufp);
    bufp += tlen;

    top = calloc(sizeof(lb_pol_cache_v6_t) * LB_MAX_CACHE, 1);

    /* V6 */
    pc6 = top;
    memcpy(pc6, lb_policy_info.init6, sizeof(lb_pol_cache_v6_t) * LB_MAX_CACHE);

    sprintf(bufp, IPV6);
    tlen += strlen(IPV6);
    bufp += strlen(IPV6);

    for (num = i = 0; i < LB_MAX_CACHE; i++) {
        if (pc6->lb_stat) {
            if (inet_ntop(AF_INET6, &pc6->lb_src_ip, tmp, INET6_ADDRSTRLEN) 
                    != NULL) {
                get_time(stime, pc6->timestamp, ssec, tbuf);
                sprintf(bufp, "%4d) %s (%u packets) create: %s\n", 
                    ++num, tmp, pc6->hit, tbuf);
                int len = strlen(bufp);
                tlen += len;
                bufp += len;
                WRITE_LOG_MIDDLE(128);
            }
        }
        pc6++;
    }

    /* V4 */
    pc4 = top;
    memcpy(pc4, lb_policy_info.init4, sizeof(lb_pol_cache_v4_t) * LB_MAX_CACHE);

    sprintf(bufp, IPV4);
    tlen += strlen(IPV4);
    bufp += strlen(IPV4);

    for (num = i = 0; i < LB_MAX_CACHE; i++) {
        if (pc4->lb_stat) {
            if (inet_ntop(AF_INET, &pc4->lb_src_ip, tmp, INET_ADDRSTRLEN) 
                    != NULL) {
                get_time(stime, pc4->timestamp, ssec, tbuf);
                sprintf(bufp,  "%4d) %s (%u packets) create: %s\n", 
                    ++num, tmp, pc4->hit, tbuf);
                int len = strlen(bufp);
                tlen += len;
                bufp += len;
                WRITE_LOG_MIDDLE(128);
            }
        }
        pc4++;
    }

    fwrite(buff, tlen, 1, fp);
    free(top);

    anycast_sleep(10);
}
#else
/* backend */
void
write_log_cli(FILE *fp, const char *buff)
{
    ci_v4_t *ci4;
    ci_v6_t *ci6;
    void *top;
    char *bufp = (char*)buff;
    int i, num, len, tlen;
    char tmp[INET6_ADDRSTRLEN], tbuf[64];
    time_t ssec = client_info.starttime;
    uint64_t stime = client_info.tsc;

    sprintf(bufp, "\n"SEPARATOR"\n"DUMP_CLIENT_NAME"\n");
    tlen = strlen(bufp);
    bufp += tlen;
    
    top = malloc(sizeof(ci_v6_t) * CLI_CACHE);

    /* v6 */
    sprintf(bufp, "\nIPv6 client info (client to server)\n");
    len = strlen(bufp);
    tlen += len;
    bufp += len;

    ci6 = top;
    memcpy(ci6, client_info.up_init6, sizeof(ci_v6_t) * CLI_CACHE);

    for (num = i = 0; i < CLI_CACHE; i++ ) {
        if (ci6->timestamp &&
            (inet_ntop(AF_INET6, &ci6->cli_ip, tmp, INET6_ADDRSTRLEN) 
                != NULL)) {
            get_time(stime, ci6->timestamp, ssec, tbuf);
            sprintf(bufp,  "%4d) %s (%u packets) last access: %s\n", 
                ++num, tmp, ci6->hit, tbuf);
            len = strlen(bufp);
            tlen += len;
            bufp += len;
            WRITE_LOG_MIDDLE(INET6_ADDRSTRLEN+32);
        }
        ci6++;
    }
    anycast_sleep(0);

    /* v6 */
    sprintf(bufp,"\nIPv6 client info (server to client)\n");
    len = strlen(bufp);
    tlen += len;
    bufp += len;

    ci6 = top;
    memcpy(ci6, client_info.dwn_init6, sizeof(ci_v6_t) * CLI_CACHE);

    for (num = i = 0; i < CLI_CACHE; i++ ) {
        if (ci6->timestamp &&
            (inet_ntop(AF_INET6, &ci6->cli_ip, tmp, INET6_ADDRSTRLEN) 
                != NULL)) {
            get_time(stime, ci6->timestamp, ssec, tbuf);
            sprintf(bufp,  "%4d) %s (%u packets) last access: %s\n", 
                ++num, tmp, ci6->hit, tbuf);
            len = strlen(bufp);
            tlen += len;
            bufp += len;
            WRITE_LOG_MIDDLE(INET6_ADDRSTRLEN+32);
        }
        ci6++;
    }
    anycast_sleep(0);

    /* v4 */
    sprintf(bufp, "\nIPv4 client info (client to server)\n");
    len = strlen(bufp);
    tlen += len;
    bufp += len;

    ci4 = top;
    memcpy(ci4, client_info.up_init4, sizeof(ci_v4_t) * CLI_CACHE);
 
    for (num = i = 0; i < CLI_CACHE; i++ ) {
        if (ci4->timestamp &&
            (inet_ntop(AF_INET, &ci4->cli_ip, tmp, INET_ADDRSTRLEN) 
                != NULL)) {
            get_time(stime, ci4->timestamp, ssec, tbuf);
            sprintf(bufp, "%4d) %s (%u packets) last access: %s\n", 
                ++num, tmp, ci4->hit, tbuf);
            len = strlen(bufp);
            tlen += len;
            bufp += len;
            WRITE_LOG_MIDDLE(INET_ADDRSTRLEN+32);
        }
        ci4++;
    }
    anycast_sleep(0);

    /* v4 */
    sprintf(bufp, "\nIPv4 client info (server to client)\n");
    len = strlen(bufp);
    tlen += len;
    bufp += len;

    ci4 = top;
    memcpy(ci4, client_info.dwn_init4, sizeof(ci_v4_t) * CLI_CACHE);

    for (num = i = 0; i < CLI_CACHE; i++ ) {
        if (ci4->timestamp &&  
            (inet_ntop(AF_INET, &ci4->cli_ip, tmp, INET_ADDRSTRLEN) 
                != NULL)) {
            get_time(stime, ci4->timestamp, ssec, tbuf);
            sprintf(bufp,  "%4d) %s (%u packets) last access: %s\n", 
                ++num, tmp, ci4->hit, tbuf);
            len = strlen(bufp);
            tlen += len;
            bufp += len;
            WRITE_LOG_MIDDLE(INET_ADDRSTRLEN+32);
        }
        ci4++;
    }

    if (tlen) {
        fwrite(buff, tlen, 1, fp);
    }
    free(top);
    anycast_sleep(10);
}
#endif

#ifdef FRONT_T
/*
    @brief 振分け統計
    @param fp FILE*
    @param buffer
*/
void
write_log_stat2(FILE *fp, const char *buff)
{
    lb_pol_v4_t *entry4;
    lb_pol_v6_t *entry6;
    int tlen, num;
    char *bufp = (char*)buff;

    sprintf(bufp, "\n"SEPARATOR"\n"DUMP_STAT2_NAME"\n");
    tlen = strlen(bufp);
    bufp += tlen;

    /* v6 */
    sprintf(bufp, IPV6);
    tlen += strlen(IPV6);
    bufp += strlen(IPV6);

    num = 0;

    TAILQ_FOREACH(entry6, &lb_policy_info.lb_pol_head6, lb_list) {
        sprintf(bufp, "%4d) %s (hit:%u client:%u)\n", ++num, entry6->line,
            entry6->hit_count, entry6->use_count);
        int len = strlen(bufp);
        tlen += len;
        bufp += len;
        WRITE_LOG_MIDDLE(128);
    }

    /* v4 */
    sprintf(bufp, IPV4);
    tlen += strlen(IPV4);
    bufp += strlen(IPV4);

    num = 0;
    TAILQ_FOREACH(entry4, &lb_policy_info.lb_pol_head4, lb_list) {
        sprintf(bufp, "%4d) %s (hit:%u client:%u)\n", ++num, entry4->line,
            entry4->hit_count, entry4->use_count);
        int len = strlen(bufp);
        tlen += len;
        bufp += len;
        WRITE_LOG_MIDDLE(128);
    }
    
    fwrite(buff, tlen, 1, fp);
    anycast_sleep(10);
}
#endif

/*
    @brief サーバ（backend)情報
    @param fp FILE*
    @param buffer
*/
#ifdef FRONT_T
void
write_log_svr(FILE *fp, const char *buff)
{
    server_tbl_t *svr_tbl;
    char addr[INET6_ADDRSTRLEN];
    char gw[INET6_ADDRSTRLEN];
    int tlen, num;
    char *bufp = (char*)buff;

    sprintf(bufp, "\n"SEPARATOR"\n"DUMP_SERVER_NAME"\n");
    tlen = strlen(bufp);
    bufp += tlen;

    /* v6 */
    sprintf(bufp, IPV6);
    tlen += strlen(IPV6);
    bufp += strlen(IPV6);
    num = 0;

    SLIST_FOREACH(svr_tbl, &svr_mng_tbl.head6, list) {
        if (likely(svr_tbl->status != SVR_DROP)) {
            struct sockaddr_in6 *sa1 = (struct sockaddr_in6*)&svr_tbl->svr_ip;
            struct sockaddr_in6 *sa2 = (struct sockaddr_in6*)&svr_tbl->gw_ip;
            inet_ntop(AF_INET6, &sa1->sin6_addr, addr, INET6_ADDRSTRLEN);
            if (cmp_ipv6(&sa1->sin6_addr, &sa2->sin6_addr) != 0) {
                /* GW経由の場合、GWアドレスを表示 */
                inet_ntop(AF_INET6, &sa2->sin6_addr, gw, INET6_ADDRSTRLEN);
                sprintf(bufp, "%4d) %s via %s (%u packets)\n",
                    ++num, addr, gw, svr_tbl->srv_stat.hit);
            } else {
                sprintf(bufp, "%4d) %s (%u packets)\n",
                    ++num, addr, svr_tbl->srv_stat.hit);
            }
            int len = strlen(bufp);
            tlen += len;
            bufp += len;
            WRITE_LOG_MIDDLE(128);
        }
    }

    /* v4 */
    sprintf(bufp, IPV4);
    tlen += strlen(IPV4);
    bufp += strlen(IPV4);
    num = 0;

    SLIST_FOREACH(svr_tbl, &svr_mng_tbl.head4, list) {
        if (likely(svr_tbl->status != SVR_DROP)) {
            struct sockaddr_in *sa1 = (struct sockaddr_in*)&svr_tbl->svr_ip;
            struct sockaddr_in *sa2 = (struct sockaddr_in*)&svr_tbl->gw_ip;
            inet_ntop(AF_INET, &sa1->sin_addr, addr, INET_ADDRSTRLEN);
            if (cmp_ipv4(&sa1->sin_addr, &sa2->sin_addr) != 0) {
                inet_ntop(AF_INET, &sa2->sin_addr, gw, INET_ADDRSTRLEN);
                sprintf(bufp, "%4d) %s via %s (%u packets)\n",
                    ++num, addr, gw, svr_tbl->srv_stat.hit);
            } else {
                sprintf(bufp, "%4d) %s (%u packets)\n",
                    ++num, addr, svr_tbl->srv_stat.hit);
            }
            int len = strlen(bufp);
            tlen += len;
            bufp += len;
            WRITE_LOG_MIDDLE(128);
        }
    }

    fwrite(buff, tlen, 1, fp);
    anycast_sleep(10);
}

#endif
/* end */
