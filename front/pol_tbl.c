/**
 * file    pol_tbl.c
 * brief   振り分けテーブル作成処理
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <syslog.h>

#include "option.h"
#include "policy.h"
#include "server.h"
#include "util_inline.h"
#include "val.h"

static FILE *open_prop_file(void);
static void parse_policy(FILE *file);
static void parse_policy_line(char *buff);
static void create_policy_table(server_tbl_t *svr_tbl, char *saddr, char *netmask, char*buff);
static sa_family_t get_family(char *p);

/*
    振り分けテーブルファイル読み込み処理
*/
void get_policy(void)
{
    FILE *file = open_prop_file();

    /* ファイルが無い場合、記録だけ行なう */
    if (file == NULL) {
        mlog("can not open policy file");
        return;
    }

    parse_policy(file);

    fclose(file);
}

/*
    ファイルオープン
*/
static FILE *open_prop_file(void)
{
    char cwd[128];
    char path[256];

    /*ディレクトリパスを設定*/
    strncpy(cwd, POL_DIRNAME, 128);
    sprintf(path, "%s/%s", cwd, POL_FILENAME);

    return fopen(path, "r");
}

/*
    ファイル読み出し
*/
static void
parse_policy(FILE *file)
{
    for ( ;; ) {
        char buff[256];

        fgets(buff, 256, file);
        if (feof(file)) {
            break;
        }
        if ((strlen(buff) == 1) && (buff[0] == '\n')) {
            continue;
        }
        if ((buff[0] == '#') || (buff[0] == '[')) {
            continue;
        }
        parse_policy_line(buff);
    }
}

/*
    行処理
    フォーマットは以下
    src ip    mask       server ip
    10.0.0.1, 255.0.0.3, 192.168.0.1
*/
#define MAX_PART 3
#define PART_SIZE 64
#define POL_DELIMITER ','
static void
parse_policy_line(char *buff)
{
    char *dp, *sp;
    int i, l;
    char ip[MAX_PART][PART_SIZE];
    sa_family_t family[MAX_PART];
    server_tbl_t *svr_tbl;

    memset(ip, 0, sizeof(ip));

    /* 改行を消す */
    if (likely((dp = strchr(buff, '\n')) != NULL)) {
        *dp = '\0';
    }

    /* 分離記号(,)を検索して終端 */
    for (i = 0; (i < MAX_PART) && buff; i++) {
        dp = skip_space(buff);
        if ((buff = strchr(buff, POL_DELIMITER)) != NULL) {
            *buff++ = '\0';
        }

        /* 各パートをコピー */
        sp = ip[i];
        for (l = strlen(dp); l; l--) {
            if (isblank(*dp)) {
                break;
            }
            *sp++ = *dp++;
        }
        *sp = '\0';

        /* プロトコルファミリーのチェック */
        if ((family[i] = get_family(ip[i])) == AF_UNSPEC) {
            /* v4,v6以外の場合、無視する */
            mlog("policy family error? (%s)", ip[i]);
            return;
        }

        if (i > 0) {
            if (family[0] != family[i]) {
                /* 表記が混在していたらその行を無視する */
                mlog("policy family mismatch (%s/%s)", ip[0], ip[i]);
                return;
            }
        }
    }
    if ( ((family[0] == AF_INET) && !if_ingress->v4_enable) || 
         ((family[0] == AF_INET6) && !if_ingress->v6_enable)) {
        mlog("policy skipped (interface not available %d)",family[0]);
        return;
    }

    /* サーバ管理テーブルの検索（なかったら作成） */
    svr_tbl = get_svr_table(ip[2], family[0]);
    /* 振り分けテーブルの作成 */
    create_policy_table(svr_tbl, ip[0], ip[1], ip[2]);
}

/*
    @brief 振り分けテーブル作成

    @param svr_tbl
    @param saddr
    @param mask
*/
static void
create_policy_table(server_tbl_t *svr_tbl, char *saddr, 
    char *netmask, char *svr)
{
    if (svr_tbl->family == AF_INET) {
        lb_pol_v4_t *pol_v4;
        struct in_addr ip, mask;

        pol_v4 = (lb_pol_v4_t*)calloc(sizeof(lb_pol_v4_t), 1);
        if (!pol_v4) {
            mlog("create policy malloc error %d", sizeof(lb_pol_v4_t));
            return;
        }

        inet_pton(AF_INET, saddr, &ip);
        inet_pton(AF_INET, netmask, &mask);

        ip.s_addr &= mask.s_addr;
        pol_v4->addr_v4 = ip;
        pol_v4->mask_v4 = mask;

        pol_v4->svr = svr_tbl;

        snprintf(pol_v4->line, sizeof(pol_v4->line),
            "%s, %s, %s", saddr, netmask, svr);

        mlog("create v4 policy table (%s)", pol_v4->line);

        TAILQ_INSERT_TAIL(&lb_policy_info.lb_pol_head4, pol_v4, lb_list);

    } else /* svr_tbl->family == AF_INET6) */ {
        lb_pol_v6_t *pol_v6;
        struct in6_addr ip, mask;

        pol_v6 = (lb_pol_v6_t*)calloc(sizeof(lb_pol_v6_t), 1);
        if (!pol_v6) {
            mlog("create policy malloc error %d", sizeof(lb_pol_v6_t));
            return;
        }

        inet_pton(AF_INET6, saddr,    &ip);
        inet_pton(AF_INET6, netmask,  &mask);

        mask_ipv6(&ip, &mask);

        pol_v6->addr_v6 = ip;
        pol_v6->mask_v6 = mask;

        pol_v6->svr = svr_tbl;

        snprintf(pol_v6->line, sizeof(pol_v6->line),
            "%s, %s, %s", saddr, netmask, svr);

        mlog("create v6 policy table (%s)", pol_v6->line);

        TAILQ_INSERT_TAIL(&lb_policy_info.lb_pol_head6, pol_v6, lb_list);
    }
}

/*
    @brief 更新前に古いものを削除する
*/
void
destroy_policy_table(void)
{
    lb_pol_v4_t *entry4, *entry4_next;

    lb_policy_info.pol_no++;

    for (entry4 = TAILQ_FIRST(&lb_policy_info.lb_pol_head4);
         entry4 != NULL;
         entry4 = entry4_next) {

        entry4_next = TAILQ_NEXT(entry4, lb_list);
        TAILQ_REMOVE(&lb_policy_info.lb_pol_head4, entry4, lb_list);

        free(entry4);
    }

    lb_pol_v6_t *entry6, *entry6_next;
    for (entry6 = TAILQ_FIRST(&lb_policy_info.lb_pol_head6);
         entry6 != NULL;
         entry6 = entry6_next) {

        entry6_next = TAILQ_NEXT(entry6, lb_list);
        TAILQ_REMOVE(&lb_policy_info.lb_pol_head6, entry6, lb_list);
        
        free(entry6);
    }
}

/*
    @brief V4, V6判定処理(簡易)
    @param 文字列
    @return AF_INET ot AF_INET6 異常時はAF_UNSPEC
*/
static sa_family_t
get_family(char *p) 
{
    int i;
    char *cp;

    if (strchr(p, ':')) {
        return AF_INET6;
    }

    for (cp=p, i=0; *cp; cp++) {
        if (*cp <= '9' && *cp >= '0') {
            continue;
        }
        if (*cp == '.' && ++i <= 3) {
            continue;
        }
        if (*cp == '\n') {
            break;
        }
        return AF_UNSPEC;
    }
    return AF_INET;
}

/*
    @brief 振り分けテーブル初期化
    @param reset 再読み込み時は１
*/
void
init_policy_table(int reset)
{
    int i;
    lb_pol_cache_v4_t *pol_cache4;
    lb_pol_cache_v6_t *pol_cache6;
    struct timespec time;

    lb_policy_info.pol_no = 0;

    TAILQ_INIT(&lb_policy_info.lb_pol_head4);
    TAILQ_INIT(&lb_policy_info.lb_pol_head6);

    for( i = 0; i < LB_POL_DIVISOR; i++) {
        TAILQ_INIT(&lb_policy_info.lb_pol_hash_v4[i]);
        TAILQ_INIT(&lb_policy_info.lb_pol_hash_v6[i]);
    }

    TAILQ_INIT(&lb_policy_info.lb_pol_free4);
    TAILQ_INIT(&lb_policy_info.lb_pol_free6);

    if (reset) {
        pol_cache4 = lb_policy_info.init4;
    } else {
        pol_cache4 = malloc(sizeof(lb_pol_cache_v4_t) * LB_MAX_CACHE);
        if (!pol_cache4) {
            syslog(LOG_ERR, "init policy table malloc error %d",
                (sizeof(lb_pol_cache_v4_t) * LB_MAX_CACHE));
            exit(1);
        }
        lb_policy_info.init4 = pol_cache4;
    }

    for (i = 0; i < LB_MAX_CACHE; i++) {
        pol_cache4->lb_cache_type = 0;
        pol_cache4->lb_stat = 0;
        TAILQ_INSERT_TAIL(&lb_policy_info.lb_pol_free4, pol_cache4, lb_list);
        pol_cache4++;
    }

    if (reset) {
        pol_cache6 = lb_policy_info.init6;
    } else {
        pol_cache6 = malloc(sizeof(lb_pol_cache_v6_t) * LB_MAX_CACHE);
        if (!pol_cache6) {
            syslog(LOG_ERR, "init policy table malloc error %d",
                (sizeof(lb_pol_cache_v6_t) * LB_MAX_CACHE));
            exit(1);
        }
        lb_policy_info.init6 = pol_cache6;
    }
    for (i = 0; i < LB_MAX_CACHE; i++) {
        pol_cache6->lb_cache_type = 0;
        pol_cache6->lb_stat = 0;
        TAILQ_INSERT_TAIL(&lb_policy_info.lb_pol_free6, pol_cache6, lb_list);
        pol_cache6++;
    }

    lb_policy_info.fix4.lb_cache_type = 1;
    lb_policy_info.fix6.lb_cache_type = 1;

    clock_gettime(CLOCK_REALTIME, &time);
    lb_policy_info.starttime = time.tv_sec;
    lb_policy_info.tsc = rdtsc(); 
}

/* end */
