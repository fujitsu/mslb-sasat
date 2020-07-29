/**
 * file    pol_lookup.c
 * brief   振り分けテーブル検索処理
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

#include <netinet/in.h>

#include "option.h"
#include "policy.h"
#include "val.h"
#include "util_inline.h"
#include "checksum.h"

static lb_pol_cache_v4_t *get_pol_slow_v4(struct in_addr, struct lb_hash_head4 *);
static lb_pol_cache_v4_t *get_free_pol_cache4(void);
static lb_pol_v4_t*policy_lookup4(struct in_addr saddr);
static inline void free_pol_cache4(lb_pol_cache_v4_t *entry);

static lb_pol_cache_v6_t *get_free_pol_cache6(void);
static lb_pol_cache_v6_t *get_pol_slow_v6(struct in6_addr *, struct lb_hash_head6 *);
static lb_pol_v6_t *policy_lookup6(struct in6_addr *saddr);
static inline void free_pol_cache6(lb_pol_cache_v6_t *entry);

/*
    @brief IPv4 振り分けキャッシュテーブル取得
    @param saddr
    @return policyキャッシュ(NULLのとき破棄する)
*/
lb_pol_cache_v4_t *get_pol_v4(struct in_addr saddr)
{
    lb_pol_cache_v4_t *entry;
    uint hash = ip_hash_code4(saddr.s_addr, LB_POL_MASK);
    struct lb_hash_head4 *head;

    head = &lb_policy_info.lb_pol_hash_v4[hash];

    TAILQ_FOREACH (entry, head, lb_list) {
        if (cmp_ipv4(&entry->lb_src_ip, &saddr) == 0) {
#if 0
            if (unlikely(entry->pol_no != lb_policy_info.pol_no)) {
                /* 振り分けテーブルが古い場合 */
                free_pol_cache4(entry);
                break;
            }
#endif
            entry->hit++;
            (*entry->pol_hit)++;
            (*entry->svr_hit)++;

            return entry->op;
        }
    }
    return get_pol_slow_v4(saddr, head);
}

/*
    @brief  振り分けテーブルを検索して、キャッシュテーブルを作成
 */
static lb_pol_cache_v4_t *get_pol_slow_v4(struct in_addr saddr,
    struct lb_hash_head4 *head)
{
    lb_pol_cache_v4_t *entry;
    lb_pol_v4_t *f;

    /* ソースアドレスから振り分けテーブルを検索 */
    f = policy_lookup4(saddr);
    if (f == NULL) {
        /* 振り分けテーブルが存在しないため破棄する */
        return NULL;
    }

    f->use_count++;

    entry = get_free_pol_cache4();

    entry->lb_src_ip = saddr;
    entry->lb_dst_ip = ((struct sockaddr_in*)&f->svr->svr_ip)->sin_addr;
    copy_mac(entry->lb_dst_mac, f->svr->dst_mac);
#if 0
    entry->pol_no = lb_policy_info.pol_no;
#endif
    entry->lb_stat = f->svr->status;    /* OK or DROP */ 

    entry->pol_hit = &f->hit_count;
    entry->svr_hit = &f->svr->srv_stat.hit;
    entry->hit++;
    (*entry->pol_hit)++;
    (*entry->svr_hit)++;

    if (likely(entry->lb_cache_type == 0)) {
        /* 一時的なキャッシュではない場合 */
        entry->timestamp = rdtsc();
        TAILQ_INSERT_HEAD(head, entry, lb_list);
    }

    if (entry->lb_stat == SVR_DROP) {
        /* 破棄設定 */ 
        entry->op = NULL;
        return NULL;
    }

    /* checksum差分を保存 */
    entry->chksum_delta = calc_chksum_delta(&if_ingress->vip4,
        &entry->lb_dst_ip);
    entry->op = entry;
    return entry;
}

/*
    @brief 振り分けテーブル検索（IPv4)
*/
static lb_pol_v4_t*policy_lookup4(struct in_addr saddr)
{
    lb_pol_v4_t *entry;

    TAILQ_FOREACH(entry, &lb_policy_info.lb_pol_head4, lb_list) {
        if ((saddr.s_addr & entry->mask_v4.s_addr) == entry->addr_v4.s_addr) {
            server_tbl_t *svr = entry->svr;
            int i = 0;
            do {
                if (likely(svr->status != SVR_INIT)) {
                    return entry;
                }
                resolve_target_mac(svr);
            } while ( i++ < 3 );
            break;
        }
    }
    return NULL;
}

/*
    @brief 振り分けキャッシュをフリーキューから取り出す
*/
static lb_pol_cache_v4_t *get_free_pol_cache4(void)
{
    lb_pol_cache_v4_t *entry;

    entry = TAILQ_FIRST(&lb_policy_info.lb_pol_free4);

    if (unlikely(entry == NULL)) {
        start_patrol(DEL_CACHE4);
        entry = &lb_policy_info.fix4;
    } else {
        TAILQ_REMOVE(&lb_policy_info.lb_pol_free4, entry, lb_list);
        entry->hit = 0;
    }
    return entry;
}

/*
    @brief 振り分けキャッシュテーブルをフリーキューへ戻す
*/
static inline void free_pol_cache4(lb_pol_cache_v4_t *entry)
{
    entry->lb_stat = 0;
    TAILQ_INSERT_TAIL(&lb_policy_info.lb_pol_free4, entry, lb_list);
}

/*
    @brief IPv6 振り分けキャッシュテーブル取得
    @param saddr
    @return policyキャッシュ NULLのとき、破棄する
*/
lb_pol_cache_v6_t *get_pol_v6(struct in6_addr *saddr)
{
    lb_pol_cache_v6_t *entry;
    uint hash = ip_hash_code6(saddr->s6_addr32, LB_POL_MASK);
    struct lb_hash_head6 *head;

    head = &lb_policy_info.lb_pol_hash_v6[hash];

    TAILQ_FOREACH (entry, head, lb_list) {
        //宛先が一致したらヒット
        if (cmp_ipv6(saddr, &entry->lb_src_ip) == 0) {
#if 0
            if (unlikely(entry->pol_no != lb_policy_info.pol_no)) {
                /* 振り分けテーブルが古い場合 */
                free_pol_cache6(entry);
                break;
            }
#endif
            /* 統計 */
            entry->hit++;
            (*entry->pol_hit)++;
            (*entry->svr_hit)++;

            return entry->op;
        }
    }

    return get_pol_slow_v6(saddr, head);
}

/*
    @brief 振り分けテーブルを検索して、キャッシュテーブルを作成
*/
static lb_pol_cache_v6_t *get_pol_slow_v6(struct in6_addr *saddr,
    struct lb_hash_head6 *head)
{
    lb_pol_cache_v6_t *entry;
    lb_pol_v6_t *f;

    /* ソースアドレスから振り分けテーブルを検索 */
    f = policy_lookup6(saddr);
    if (f == NULL) {
        /* 破棄する場合 */
        return NULL;
    }

    f->use_count++;

    entry = get_free_pol_cache6();

    entry->lb_src_ip = *saddr;
    entry->lb_dst_ip = ((struct sockaddr_in6*)&f->svr->svr_ip)->sin6_addr;
    copy_mac(entry->lb_dst_mac, f->svr->dst_mac);
#if 0
    entry->pol_no = lb_policy_info.pol_no;
#endif
    entry->lb_stat = f->svr->status;

    entry->pol_hit = &f->hit_count;
    entry->svr_hit = &f->svr->srv_stat.hit;
    entry->hit++;
    (*entry->pol_hit)++;
    (*entry->svr_hit)++;

    if (likely(entry->lb_cache_type == 0)) {
        /* 一時的キャッシュではない場合 */
        entry->timestamp = rdtsc();
        TAILQ_INSERT_HEAD(head, entry, lb_list);
    }

    if (entry->lb_stat == SVR_DROP) {
        /* 破棄設定の場合 */
        entry->op = NULL;
        return NULL;
    }

    entry->op = entry;
    return entry;
}

/*
    @brief 振り分けテーブル検索（IPv6)
*/
static lb_pol_v6_t *policy_lookup6(struct in6_addr *saddr)
{
    lb_pol_v6_t *entry;
    struct in6_addr addr;

    TAILQ_FOREACH(entry, &lb_policy_info.lb_pol_head6, lb_list) {
        addr = *saddr;
        mask_ipv6(&addr, &entry->mask_v6);
        if (cmp_ipv6(&addr, &entry->addr_v6) == 0) {
            server_tbl_t *svr = entry->svr;
            int i = 0;
            do {
                if (likely(svr->status != SVR_INIT)) {
                    return entry;
                }
                resolve_target_mac(svr);
            } while ( i++ < 3 );
            break;
        }
    }
    return NULL;
}

/*
    @brief 振り分けキャッシュテーブルをフリーキューから取得
*/
static lb_pol_cache_v6_t *get_free_pol_cache6(void)
{
    lb_pol_cache_v6_t *entry;

    entry = TAILQ_FIRST(&lb_policy_info.lb_pol_free6);

    if (unlikely(entry == NULL)) {
        /* パトロール機能を開始 */
        start_patrol(DEL_CACHE6);
        entry = &lb_policy_info.fix6;
    } else {
        TAILQ_REMOVE(&lb_policy_info.lb_pol_free6, entry, lb_list);
        entry->hit = 0;
    }
    return entry;
}


/*
    @brief 振り分けキャッシュテーブルをフリーキューへ戻す
*/
static inline void free_pol_cache6(lb_pol_cache_v6_t *entry)
{
    entry->lb_stat = 0;
    TAILQ_INSERT_TAIL(&lb_policy_info.lb_pol_free6, entry, lb_list);
}

/*
    処理要求フラグを設定
*/
void start_patrol(int flag)
{
    patrol_flag |= flag;
}

int
clear_v4_cache(int ptr)
{
    int i = 0;
    struct lb_hash_head4 *head;
    lb_pol_cache_v4_t *entry, *entry_next;

    while (!i && (ptr != LB_POL_DIVISOR)) {
        head = &lb_policy_info.lb_pol_hash_v4[ptr];
        for (entry = TAILQ_FIRST(head);
             entry != NULL;
             entry = entry_next) {
            entry_next = TAILQ_NEXT(entry, lb_list);
            TAILQ_REMOVE(head, entry, lb_list);
            free_pol_cache4(entry);
            i++;
        }
        ptr++;
    }
    return ptr;
}
    
int 
clear_v6_cache(int ptr)
{
    int i = 0;
    struct lb_hash_head6 *head;
    lb_pol_cache_v6_t *entry, *entry_next;

    while (!i && (ptr != LB_POL_DIVISOR)) {
        head = &lb_policy_info.lb_pol_hash_v6[ptr];
        for (entry = TAILQ_FIRST(head);
             entry != NULL;
             entry = entry_next) {
            entry_next = TAILQ_NEXT(entry, lb_list);
            TAILQ_REMOVE(head, entry, lb_list);
            free_pol_cache6(entry);
            i++;
        }
        ptr++;
    }
    return ptr;
}

/* end */
