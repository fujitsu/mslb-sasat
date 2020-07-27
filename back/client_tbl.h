/**
 * file    client.h
 * brief   クライアントテーブル関係
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT) Yagi
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <sys/queue.h>

#define CLI_DIVISOR  1024
#define CLI_MASK     (CLI_DIVISOR - 1)
#define CLI_CACHE    256

/*
    クライアント情報テーブル（IPv4)
*/
/* 上り */
typedef struct ci_v4_s
{
    SLIST_ENTRY(ci_v4_s) list;
    uint32_t hit;                   /* クライアントからのパケットが通過した回数 */
    struct in_addr cli_ip;          /* クライアント ip */
#if 0
    uint8_t stat;
    uint8_t _rsv[3];
#endif
    uint64_t timestamp;             /* 最終通過時間（TSC値）*/
} ci_v4_t;


/*
    クライアント情報テーブル（IPv6)
*/
/* 上り */
typedef struct ci_v6_s
{
    SLIST_ENTRY(ci_v6_s) list;
    uint32_t hit;                   /* クライアントからのパケットが通過した回数 */
    struct in6_addr cli_ip;         /* クライアント ip */
#if 0
    uint8_t stat;
    uint8_t _rsv[3];
#endif
    uint64_t timestamp;             /* 最終通過時間(TSC値) */
} ci_v6_t;

/*
    まとめたもの
*/
typedef struct client_info_s {
    SLIST_HEAD(cfree4, ci_v4_s) cf_up4, cf_dwn4;
    SLIST_HEAD(chash4, ci_v4_s) ch_up4[CLI_DIVISOR], ch_dwn4[CLI_DIVISOR];
    ci_v4_t *up_init4;   
    ci_v4_t *dwn_init4;

    /* 上り IPv6 */
    SLIST_HEAD(cfree6, ci_v6_s) cf_up6, cf_dwn6;
    SLIST_HEAD(chash6, ci_v6_s) ch_up6[CLI_DIVISOR], ch_dwn6[CLI_DIVISOR];
    ci_v6_t *up_init6;   
    ci_v6_t *dwn_init6;   

    time_t starttime;   /* 起動時 */
    uint64_t tsc;       /* 起動時 */
} client_info_t;

void init_client_table(void);
void get_ci_up4(struct in_addr *saddr);
void get_ci_dwn4(struct in_addr *saddr);
void get_ci_up6(struct in6_addr *saddr);
void get_ci_dwn6(struct in6_addr *saddr);


#endif
