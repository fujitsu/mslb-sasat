/**
 * file    policy.h
 * brief   振り分けに関する定義
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef _POLICY_H_
#define _POLICY_H_

#include <sys/queue.h>
#include "server.h"

#define LB_POL_DIVISOR  4096
#define LB_POL_MASK     (LB_POL_DIVISOR - 1)
#define LB_MAX_CACHE    1024

#define POL_DIRNAME     "/var/opt/sasat/etc"
#define POL_FILENAME    "sasat.policy"

/*
    振り分け結果キャッシュテーブル（IPv4)
*/
typedef struct lb_pol_cache_v4_s
{
    TAILQ_ENTRY(lb_pol_cache_v4_s) lb_list;
    
    struct in_addr lb_src_ip;       /* src ip */
    struct in_addr lb_dst_ip;       /* 変換ip */

#if 0
    uint    pol_no;
#endif

    uint8_t lb_dst_mac[ETH_ALEN];   /* 変換宛先MAC */
    uint8_t lb_cache_type;          /* type 0 -- normal, 1 -- fix */
    uint8_t lb_stat;                /* 状態 0 使用していない, 1 使用中(通常), 2 破棄 */
    
    uint16_t chksum_delta;          /* IPチェックサム差分 */
    uint8_t _rsv[2];

    /* 統計情報 */
    uint32_t hit;                   /* このテーブルを使用した回数 */
    uint32_t *pol_hit;              /* 振り分けヒット数 */
    uint32_t *svr_hit;              /* サーバヒット数 */

    struct lb_pol_cache_v4_s *op;   /* 自分のアドレス */

    uint64_t timestamp;             /* tsc value */
} lb_pol_cache_v4_t;

/*
    振り分け結果キャッシュテーブル（IPv6)
*/
typedef struct lb_pol_cache_v6_s
{
    TAILQ_ENTRY(lb_pol_cache_v6_s) lb_list;
    
    struct in6_addr lb_src_ip;      /* src ip */
    struct in6_addr lb_dst_ip;      /* server ip */

#if 0
    uint    pol_no;
#endif

    uint8_t lb_dst_mac[ETH_ALEN];   /* backend MAC */
    uint8_t lb_cache_type;          /* type 0 -- normal, 1 -- fix */
    uint8_t lb_stat;

    /* 統計情報 */
    uint32_t hit;                   /* このテーブルを使用した回数 */
    uint32_t *pol_hit;              /* 振り分けヒット数 */
    uint32_t *svr_hit;              /* サーバヒット数 */

    struct lb_pol_cache_v6_s *op;   /* 自分のアドレス */

    uint64_t timestamp;             /* tsc value */
} lb_pol_cache_v6_t;


/*
    振り分け（ポリシ）テーブル        
*/
typedef struct lb_pol_v4_s 
{
	TAILQ_ENTRY(lb_pol_v4_s) lb_list;

    struct in_addr  addr_v4;  
    struct in_addr  mask_v4;

    server_tbl_t *svr;          /* 振り分け先サーバ */
    uint32_t use_count;         /* 参照キャッシュ数 */
    uint32_t hit_count;         /* パケット数 */
                            
    char line[64];              /* コマンド行をコピー */

} lb_pol_v4_t;

typedef struct lb_pol_v6_s 
{
	TAILQ_ENTRY(lb_pol_v6_s) lb_list;

    struct in6_addr addr_v6;  
    struct in6_addr mask_v6;

    server_tbl_t *svr;          /* 振り分け先サーバ */
    uint32_t use_count;         /* 参照キャッシュ数 */
    uint32_t hit_count;         /* パケット数 */

    char line[128];

} lb_pol_v6_t;

/*
    振り分けテーブルの管理
*/
struct lb_pol_info {
    uint pol_no;                /* 番号（更新されるとインクリメント) */

    /* 振り分けテーブルのリスト */
    TAILQ_HEAD(, lb_pol_v4_s)   lb_pol_head4;
    TAILQ_HEAD(, lb_pol_v6_s)   lb_pol_head6;

    /* 振り分けキャッシュテーブルのフリーリスト */
    TAILQ_HEAD(,lb_pol_cache_v4_s) lb_pol_free4;
    TAILQ_HEAD(,lb_pol_cache_v6_s) lb_pol_free6;

    /* 先頭アドレス（ダンプ用) */
    lb_pol_cache_v4_t *init4;   
    lb_pol_cache_v6_t *init6;
    
    /* 使用中の振り分けキャッシュテーブルのハッシュ */
    TAILQ_HEAD(lb_hash_head4, lb_pol_cache_v4_s) lb_pol_hash_v4[LB_POL_DIVISOR];
    TAILQ_HEAD(lb_hash_head6, lb_pol_cache_v6_s) lb_pol_hash_v6[LB_POL_DIVISOR];

    /* 空きがなくなったときのため */
    lb_pol_cache_v4_t fix4;
    lb_pol_cache_v6_t fix6;

    time_t starttime;
    uint64_t tsc;
};

/*
    定期処理種別
*/
enum  {
    DEL_CACHE  = 1,
    DEL_CACHE4 = (1 << 0),
    DEL_CACHE6 = (1 << 1)
};


/* prototype */
void init_policy_table(int);
void get_policy(void);
void destroy_policy_table(void);
void start_patrol(int);
lb_pol_cache_v4_t *get_pol_v4(struct in_addr saddr);
lb_pol_cache_v6_t *get_pol_v6(struct in6_addr *saddr);
int clear_v4_cache(int);
int clear_v6_cache(int);

#endif
