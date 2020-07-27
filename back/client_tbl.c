/**
 * file    client_tbl.c
 * brief   クライアント情報テーブル作成処理
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT) Yagi
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

/* include */
#include <stdint.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/queue.h>

#include "option.h"
#include "val.h"
#include "anycast.h"
#include "client_tbl.h"
#include "util_inline.h"

/* prototype */
static ci_v4_t *
cre_ci_up4(struct in_addr *saddr, struct chash4 *head);
static ci_v4_t *
cre_ci_dwn4(struct in_addr *saddr, struct chash4 *head);
static ci_v4_t *get_free_ci4(struct cfree4 *free_hd);
ci_v6_t *
cre_ci_up6(struct in6_addr *saddr, struct chash6 *head);
ci_v6_t *
cre_ci_dwn6(struct in6_addr *saddr, struct chash6 *head);
static ci_v6_t *get_free_ci6(struct cfree6 *free_hd);
void
clr_ci(const char *fm, void *top, int sz);

/*
    @brief IPv4 クライアント情報統計（上りパケット受信時に使用）
    @param saddr
*/
void
get_ci_up4(struct in_addr *saddr)
{
#ifndef NO_CL_INFO
    ci_v4_t *entry;
    uint hash = ip_hash_code4(saddr->s_addr, CLI_MASK);
    struct chash4 *head;

    head = &client_info.ch_up4[hash];

    SLIST_FOREACH (entry, head, list) {
        if (cmp_ipv4(&entry->cli_ip, saddr) == 0) {
            goto hit;
        }
    }
    entry = cre_ci_up4(saddr, head);

 hit:
    entry->hit++;
    entry->timestamp = rdtsc();
#endif
}

/*
    @brief クライアント情報（上り）作成
*/
static ci_v4_t * 
cre_ci_up4(struct in_addr *saddr, struct chash4 *head)
{
    ci_v4_t *entry = get_free_ci4(&client_info.cf_up4);

    if (unlikely(entry == NULL)) {
        int i;
        clr_ci("/tmp/clup4_", client_info.up_init4, sizeof(ci_v4_t)*CLI_CACHE);
        SLIST_INIT(&client_info.cf_up4);
        entry = client_info.up_init4;
        for (i = 0; i < CLI_CACHE; i++) {
            SLIST_INSERT_HEAD(&client_info.cf_up4, entry, list);
            entry++;
        }
        entry = get_free_ci4(&client_info.cf_up4);
    }

    entry->cli_ip = *saddr;
    // entry->stat = 1;

    SLIST_INSERT_HEAD(head, entry, list);

    return entry;
}

/*
    @brief IPv4 クライアント情報統計（下り)
    @param saddr
    @return 
*/
void get_ci_dwn4(struct in_addr *saddr)
{
#ifndef NO_CLI_INFO
    ci_v4_t *entry;
    uint hash = ip_hash_code4(saddr->s_addr, CLI_MASK);
    struct chash4 *head;

    head = &client_info.ch_dwn4[hash];

    SLIST_FOREACH (entry, head, list) {
        if (cmp_ipv4(&entry->cli_ip, saddr) == 0) {
            goto hit;
        }
    }

    if (( entry = cre_ci_dwn4(saddr, head) ) == NULL) {
        return;
    }
hit:
    entry->hit++;
    entry->timestamp = rdtsc();
#endif
}

/*
    @brief クライアント情報（下り）情報作成
*/
static ci_v4_t *
cre_ci_dwn4(struct in_addr *saddr, struct chash4 *head)
{
    ci_v4_t *entry = get_free_ci4(&client_info.cf_dwn4);

    if (unlikely(entry == NULL)) {
        int i;
        clr_ci("/tmp/cldwn4_",client_info.dwn_init4,sizeof(ci_v4_t)*CLI_CACHE);
        SLIST_INIT(&client_info.cf_dwn4);
        entry = client_info.dwn_init4;
        for (i = 0; i < CLI_CACHE; i++) {
            SLIST_INSERT_HEAD(&client_info.cf_dwn4, entry, list);
            entry++;
        }
        entry = get_free_ci4(&client_info.cf_dwn4);
    }

    entry->cli_ip = *saddr;

    SLIST_INSERT_HEAD(head, entry, list);
    return entry;
}

/*
    @brief フリーキューから取り出す
*/
static ci_v4_t *get_free_ci4(struct cfree4 *free_hd)
{
    ci_v4_t *entry;

    entry = SLIST_FIRST(free_hd);

    if (likely(entry != NULL)) {
        SLIST_REMOVE_HEAD(free_hd,list);
    }
    return entry;
}

/*
    @brief IPv6 クライアント情報テーブル（上り）
           統計処理のため
    @param saddr
*/
void get_ci_up6(struct in6_addr *saddr)
{
#ifndef NO_CLI_INFO
    ci_v6_t *entry;
    uint hash = ip_hash_code6(saddr->s6_addr32, CLI_MASK);
    struct chash6 *head;

    head = &client_info.ch_up6[hash];

    SLIST_FOREACH (entry, head, list) {
        if (cmp_ipv6(&entry->cli_ip, saddr) == 0) {
            goto hit;
        }
    }
    entry = cre_ci_up6(saddr, head);

hit:
    entry->hit++;
    entry->timestamp = rdtsc();
#endif
}

/*
    @brief クライアント情報作成
*/
ci_v6_t *
cre_ci_up6(struct in6_addr *saddr, struct chash6 *head)
{
    ci_v6_t *entry = get_free_ci6(&client_info.cf_up6);

    if (unlikely(entry == NULL)) {
        int i;
        clr_ci("/tmp/clup6_", client_info.up_init6, sizeof(ci_v6_t)*CLI_CACHE);
        SLIST_INIT(&client_info.cf_up6);
        entry = client_info.up_init6;
        for (i = 0; i < CLI_CACHE; i++) {
            SLIST_INSERT_HEAD(&client_info.cf_up6, entry, list);
            entry++;
        }
        entry = get_free_ci6(&client_info.cf_up6);
    }
    entry->cli_ip = *saddr;
    // entry->stat = 1;
    SLIST_INSERT_HEAD(head, entry, list);

    return entry;
}

/*
    @brief IPv6 クライアント情報テーブル（下り）
           統計処理のため
    @param saddr
*/
void get_ci_dwn6(struct in6_addr *saddr)
{
#ifndef NO_CLI_INFO
    ci_v6_t *entry;
    uint hash = ip_hash_code6(saddr->s6_addr32, CLI_MASK);
    struct chash6 *head;

    head = &client_info.ch_dwn6[hash];

    SLIST_FOREACH (entry, head, list) {
        if (cmp_ipv6(&entry->cli_ip, saddr) == 0) {
            goto hit;
        }
    }
    if ((entry = cre_ci_dwn6(saddr, head)) == NULL) {
        return;
    }
hit:
    entry->hit++;
    entry->timestamp = rdtsc();
#endif
}

/*
    @brief クライアント情報作成
*/
ci_v6_t *
cre_ci_dwn6(struct in6_addr *saddr, struct chash6 *head)
{
    ci_v6_t *entry = get_free_ci6(&client_info.cf_dwn6);

    if (unlikely(entry == NULL)) {
        int i;
        clr_ci("/tmp/cldwn6_",client_info.dwn_init6,sizeof(ci_v6_t)*CLI_CACHE);
        SLIST_INIT(&client_info.cf_dwn6);
        entry = client_info.dwn_init6;
        for (i = 0; i < CLI_CACHE; i++) {
            SLIST_INSERT_HEAD(&client_info.cf_dwn6, entry, list);
            entry++;
        }
        entry = get_free_ci6(&client_info.cf_dwn6);
    }
    entry->cli_ip = *saddr;

    SLIST_INSERT_HEAD(head, entry, list);
    return entry;
}

/*
    @brief フリーキューから取り出す
*/
static ci_v6_t *get_free_ci6(struct cfree6 *free_hd)
{
    ci_v6_t *entry;

    entry = SLIST_FIRST(free_hd);

    if (likely(entry != NULL)) {
        SLIST_REMOVE_HEAD(free_hd, list);
    }
    return entry;
}

void
clr_ci(const char *fm, void *top, int sz)
{
#if 0
    /* ファイル書き込み */
    static int i;
    FILE *fp;
    char fname[32]; 

    sprintf(fname,"%s%d", fm, i++);

    fp = fopen(fname, "w");
    if (fp == NULL) {
        mlog();
        return;
    }
    fwrite(top, sz, 1, fp);
    fclose(fp);
#endif

    memset(top, 0, sz);
}

/*
    @brief クライアント情報テーブル初期化
*/
void
init_client_table(void)
{
    int i;
    ci_v4_t *ci4;
    ci_v6_t *ci6;
    struct timespec time;

    SLIST_INIT(&client_info.cf_up4);
    SLIST_INIT(&client_info.cf_dwn4);
    SLIST_INIT(&client_info.cf_up6);
    SLIST_INIT(&client_info.cf_dwn6);

    for( i = 0; i < CLI_DIVISOR; i++) {
        SLIST_INIT(&client_info.ch_up4[i]);
        SLIST_INIT(&client_info.ch_dwn4[i]);
        SLIST_INIT(&client_info.ch_up6[i]);
        SLIST_INIT(&client_info.ch_dwn6[i]);
    }

    ci4 = calloc((sizeof(ci_v4_t) * CLI_CACHE), 1);
    if (!ci4) {
        syslog(LOG_ERR, "init client table malloc %d",
            (sizeof(ci_v4_t) * CLI_CACHE));
        exit(1);
    }
    client_info.up_init4 = ci4;
    for (i = 0; i < CLI_CACHE; i++) {
        SLIST_INSERT_HEAD(&client_info.cf_up4, ci4, list);
        ci4++;
    }

    ci4 = calloc((sizeof(ci_v4_t) * CLI_CACHE), 1);
    if (!ci4) {
        syslog(LOG_ERR, "init client table malloc %d",
            (sizeof(ci_v4_t) * CLI_CACHE));
        exit(1);
    }
    client_info.dwn_init4 = ci4;
    for (i = 0; i < CLI_CACHE; i++) {
        SLIST_INSERT_HEAD(&client_info.cf_dwn4, ci4, list);
        ci4++;
    }

    ci6 = calloc((sizeof(ci_v6_t) * CLI_CACHE), 1);
    if (!ci6) {
        syslog(LOG_ERR, "init client table malloc %d",
            (sizeof(ci_v6_t) * CLI_CACHE));
        exit(1);
    }
    client_info.up_init6 = ci6;
    for (i = 0; i < CLI_CACHE; i++) {
        SLIST_INSERT_HEAD(&client_info.cf_up6, ci6, list);
        ci6++;
    }

    ci6 = calloc((sizeof(ci_v6_t) * CLI_CACHE), 1);
    if (!ci6) {
        syslog(LOG_ERR, "init client table malloc %d",
            (sizeof(ci_v6_t) * CLI_CACHE));
        exit(1);
    }
    client_info.dwn_init6 = ci6;
    for (i = 0; i < CLI_CACHE; i++) {
        SLIST_INSERT_HEAD(&client_info.cf_dwn6, ci6, list);
        ci6++;
    }

    clock_gettime(CLOCK_REALTIME, &time);
    client_info.starttime = time.tv_sec;
    client_info.tsc = rdtsc();
}

/* end */
