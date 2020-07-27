/**
 * file    svr_tbl.c
 * brief   サーバ管理処理 (front) 
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

#include <stdlib.h>
#include <arpa/inet.h>

#include "option.h"
#include "anycast.h"
#include "server.h"
#include "util_inline.h"
#include "val.h"

int get_target_mac(struct sockaddr *target_addr,
                   char *ifname,
                   struct sockaddr *nexthop,
                   unsigned char *dstmac,
                   int gw);
void send_ping(struct sockaddr *, struct ifdata*);

static server_tbl_t *
find_svr_tbl(uint32_t *addr, sa_family_t family);

/*
    @brief サーバ管理テーブル検索
    @param name ipアドレス（文字列)
    @param family 
*/
server_tbl_t *
get_svr_table(char *name, sa_family_t family)
{
    server_tbl_t *svr;
    uint32_t addr[4];
    int ret;    

    ret = inet_pton(family, name, addr);
    svr = find_svr_tbl(addr, family);
    
    if ((svr != NULL) && (svr->status == SVR_INIT)) {
        /* サーバ管理テーブルが存在かつMACアドレス不明な場合 */
        resolve_target_mac(svr);
    }
    return svr;        
}

/*
    @brief backendトランスレータへ送信する場合のMACアドレスを解決
*/
inline void
resolve_target_mac(server_tbl_t *svr)
{
    int i, ret;

    for (i = 0; i < 3; i++)  {
        /* routing socket検索 */
        ret = get_target_mac((struct sockaddr*)&svr->svr_ip, 
                             if_egress->ifname, 
                             (struct sockaddr*)&svr->gw_ip, 
                             svr->dst_mac,
                             0); 
        if (ret < 0) {
            /* エラー */
            continue;
        }
        /* 正常終了 */
        if (cmp_mac(zerodata, svr->dst_mac) == 0) {
            /* MACが不明の場合、pingを打つ */
            send_ping((struct sockaddr*)&svr->gw_ip, if_egress);
            anycast_sleep(10);
        } else {
            svr->status = SVR_OK;
            break;
        }
    }
}

/*
    サーバテーブルを検索
    無かったら作る
*/
static server_tbl_t *
find_svr_tbl(uint32_t *addr, sa_family_t family)
{
    server_tbl_t *svr_tbl = NULL;

    if (family == AF_INET) {
        struct sockaddr_in *sa;
        SLIST_FOREACH(svr_tbl, &svr_mng_tbl.head4, list) {
            sa = (struct sockaddr_in*)&svr_tbl->svr_ip;
            if (sa->sin_addr.s_addr == *addr) {
                return svr_tbl;
            }
        }
        svr_tbl = (server_tbl_t*)calloc(sizeof(server_tbl_t), 1);
        if (!svr_tbl) {
            mlog("find_svr_tbl malloc error %d", sizeof(server_tbl_t));
            return NULL;
        }

        svr_tbl->family = family;
        svr_tbl->status = SVR_INIT;

        sa = (struct sockaddr_in*)&svr_tbl->svr_ip;
        memcpy(&sa->sin_addr, addr, sizeof(struct in_addr));
        sa->sin_family = AF_INET;

        sa = (struct sockaddr_in*)&svr_tbl->gw_ip;
        sa->sin_family = AF_INET;

        // svr_tbl->time = rdtsc();    /* 作成時間 */

        SLIST_INSERT_HEAD(&svr_mng_tbl.head4, svr_tbl, list);
        svr_mng_tbl.server_num++;

        if (is_zero_ip((struct sockaddr*)&svr_tbl->svr_ip) == 0) {
            /* 宛先IPが0の場合、破棄設定 */
            svr_tbl->status = SVR_DROP;
        }
    } else /* if (family == AF_INET6)*/ {
        struct sockaddr_in6 *sa;
        SLIST_FOREACH(svr_tbl, &svr_mng_tbl.head6, list) {
            sa = (struct sockaddr_in6*)&svr_tbl->svr_ip;
            if (cmp_ipv6(&sa->sin6_addr, (struct in6_addr*)addr) == 0) {
                return svr_tbl;
            }
        }
        svr_tbl = (server_tbl_t*)calloc(sizeof(server_tbl_t), 1);
        if (!svr_tbl) {
            mlog("find_svr_tbl malloc error %d", sizeof(server_tbl_t));
            return NULL;
        }
        svr_tbl->family = family;
        svr_tbl->status = SVR_INIT;

        sa = (struct sockaddr_in6 *)&svr_tbl->svr_ip;
        memcpy(&sa->sin6_addr, addr, sizeof(struct in6_addr));
        sa->sin6_family = AF_INET6;

        sa = (struct sockaddr_in6 *)&svr_tbl->gw_ip;
        sa->sin6_family = AF_INET6;

        // svr_tbl->time = rdtsc();

        SLIST_INSERT_HEAD(&svr_mng_tbl.head6, svr_tbl, list);
        svr_mng_tbl.server_num++;

        if (is_zero_ip((struct sockaddr*)&svr_tbl->svr_ip) == 0) {
            svr_tbl->status = SVR_DROP;
        } 
    }
    return svr_tbl; 
}

/*
    サーバテーブルは振り分けテーブルから参照されているので
    参照が無くなってから削除する
*/
void
destroy_svr_tbl(void)
{
    server_tbl_t *svr_tbl, *svr_tbl_next;

    for (svr_tbl = SLIST_FIRST(&svr_mng_tbl.head4);
         svr_tbl;
         svr_tbl = svr_tbl_next) {
        svr_tbl_next = SLIST_NEXT(svr_tbl, list);
        SLIST_REMOVE_HEAD(&svr_mng_tbl.head4, list);

        free(svr_tbl);
    }

    for (svr_tbl = SLIST_FIRST(&svr_mng_tbl.head6);
         svr_tbl;
         svr_tbl = svr_tbl_next) {
        svr_tbl_next = SLIST_NEXT(svr_tbl, list);
        SLIST_REMOVE_HEAD(&svr_mng_tbl.head6, list);

        free(svr_tbl);
    }
}

void
init_svr_mng_table(void)
{
    memset(&svr_mng_tbl, 0, sizeof(svr_mng_tbl));

    SLIST_INIT(&svr_mng_tbl.head4);
    SLIST_INIT(&svr_mng_tbl.head6);
}

/* end */
