/**
 * file    back_init.c
 * brief   初期化処理(backend)
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT) Yagi
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h> 
#include <pthread.h>
#include <arpa/inet.h>

#include "option.h"
#include "init.h"
#include "val.h"
#include "back_properties.h"
#include "checksum.h"
#include "anycast.h"

/* 共通処理読み込み */
#include "common_init.c"

int resolve_mac(struct sockaddr *, struct ifdata *, uchar *, int);
int get_mask(struct sockaddr *);

/*
    @brief デフォルトGWのMACアドレスを取得
    @param ifdata インターフェース情報
    @param v4 ipv4指定
    @param v6 ipv6指定
*/
void
get_default_gw(struct ifdata *ifdata, int v4, int v6)
{
    struct sockaddr_storage ip;
    
    memset(&ip, 0, sizeof(ip));
    
    if (v4) {
        ip.ss_family = AF_INET; 
        gw_mac_v4_valid = resolve_mac((struct sockaddr*)&ip, 
            ifdata, gw_mac_v4, 1);
    } 
    if (v6) {
        ip.ss_family = AF_INET6;
        gw_mac_v6_valid = resolve_mac((struct sockaddr*)&ip, 
            ifdata, gw_mac_v6, 1);
    }
}

/*
    @brief サーバのIP情報を読み込み、MACアドレスを取得
*/
void
get_svr_info(void)
{
    memset(&svr_info, 0 , sizeof(svr_info));     
    const char *str;

    str = anycast_get_properties(KEY_SVR_IP4);
    if (str && 
            (inet_pton(AF_INET, str, &svr_info.svr_ip4.sin_addr) == 1)) { 
        /* v4 addess有効 */
        svr_info.v4_enable = 1;
        svr_info.svr_ip4.sin_family = AF_INET;
        svr_info.stat = resolve_mac(
                (struct sockaddr*)&svr_info.svr_ip4,
                if_egress, svr_info.svr_mac, 0); 

        svr_info.checksum_delta = calc_chksum_delta(&if_ingress->vip4, 
            &svr_info.svr_ip4.sin_addr);
    } else {
        evtlog("gft0", 0, 0, NULL);
    }

    str = anycast_get_properties(KEY_SVR_IP6);
    if (str && 
            (inet_pton(AF_INET6, str, &svr_info.svr_ip6.sin6_addr) == 1)) {
        /* v6 addess有効 */
        svr_info.v6_enable = 1;
        svr_info.svr_ip6.sin6_family = AF_INET6;
        if (svr_info.stat == SVR_INIT) {
            svr_info.stat = resolve_mac(
                (struct sockaddr*)&svr_info.svr_ip6,
                if_egress, svr_info.svr_mac, 0);
        }
    } else {
        evtlog("gft1", 0, 0, NULL); 
    }
}

/*
    @brief v4.v6の情報が十分かチェック
*/
void
marge_info(void)
{
    if (if_ingress->v4_enable && if_egress->v4_enable && svr_info.v4_enable) {
        mlog("backend v4 enable");
    } else {
        mlog("backend v4 disable");
        if_ingress->v4_enable = if_egress->v4_enable = svr_info.v4_enable = 0;
    }

    if (if_ingress->v6_enable && if_egress->v6_enable && svr_info.v6_enable) {
        mlog("backend v6 enable");
    } else {
        mlog("backend v6 disable");
        if_ingress->v6_enable = if_egress->v6_enable = svr_info.v6_enable = 0;
    }
}

/*
    @brief 振り分けスレッド起動
*/
int
create_back_thread(void*(*func)(void*), volatile int *wait)
{
    pthread_t tid;

    if (pthread_create(&tid, NULL, func, (void*)wait) < 0) {
        strerror_r(errno, ebuf1, ELOG_DATA_LEN);
        syslog(LOG_ERR,
            "network thread create error %s",ebuf1);
        return -1;
    }
    return 0;
}

/*  @brief スレッド状態 */
enum  {
    THREAD_INIT = 0,
    THREAD_WAKE,
    THREAD_GO,
};

/*
    @brief スレッド同期初期化
*/
void
sync_init(volatile int *flag, int cnt)
{
    int i;

    for (i = 0; i < cnt; i++) {
        flag[i] = THREAD_INIT;
    }    
}

/*
    @brief 子スレッドが同期を取る
*/   
void
sync_thread(volatile int *flag)
{
    *flag = THREAD_WAKE;

    for ( ;; ) {
        anycast_sleep(10);

        if (*flag == THREAD_GO) {
            break;
        }
    }
}

/*
    @brief 親スレッドが全子スレッドの起動を待ち合わせる
*/
void
wait_thread(volatile int *flag, int cnt)
{
    int i = 0;

    /* 全ての子スレッドの立ち上がりを待つ */
    while (i < cnt) {
        if (flag[i] == THREAD_WAKE) {
            i++;
        } else {
            anycast_sleep(10);
        }
    }

    /* 同期が取れたら、動作開始を指示する */
    for (i = 0; i < cnt; i++) {
        flag[i] = THREAD_GO;
    }
}

#ifdef L_MODE
/*
    @brief 同じネットワークのアドレスか判定する
    @param s1 アドレス１
    @param s2 アドレス２
    @param mlen マスクbit長
    @param len アドレスバイト長
    @return 0 不一致 1 一致
*/
static int
is_same_net(uint32_t *s1, uint32_t *s2, int mlen, int len)
{
    int i;

    for (i = 0; i < (len/4); i++) {
        if (mlen >= 32) {
            if (s1[i] != s2[i]) {
                return 0;
            }
            if ((mlen = (mlen -32)) == 0) {
                break;
            }
        } else {
            uint32_t mask = htonl(0xffffffff << (32-mlen));
            if ((s1[i] & mask) != (s2[i] & mask)) {
                return 0;
            }
            break;
        }
    }
    return 1;
}

/*
    @brief L2接続かL3接続か判定する
    @return L2_MODE or L3_MODE
*/
int
get_connect_mode(void)
{
    int mask_in, mask_eg;

    if (if_ingress->v4_enable) {
        struct sockaddr_in sa_in, sa_eg;
        sa_in.sin_addr = if_ingress->sip4;
        sa_in.sin_family = AF_INET;
        /* クライアント側マスク長取得 */
        mask_in = get_mask((struct sockaddr*)&sa_in);

        sa_eg.sin_addr = if_egress->sip4;
        sa_eg.sin_family = AF_INET;
        /* サーバ側マスク長取得 */ 
        mask_eg = get_mask((struct sockaddr*)&sa_eg);

        if ((mask_in > 0) && (mask_eg > 0)) {
            if (mask_in == mask_eg) {
                int ret = is_same_net(&sa_in.sin_addr.s_addr,
                    &sa_eg.sin_addr.s_addr, mask_in, sizeof(struct in_addr));
                if (ret) {
                    return L2_MODE;
                }
            }
            return L3_MODE;
        }
    }
    if (if_ingress->v6_enable) {
        struct sockaddr_in6 sa_in, sa_eg;
        sa_in.sin6_addr = if_ingress->sip6;
        sa_in.sin6_family = AF_INET6;
        /* クライアント側マスク長取得 */
        mask_in = get_mask((struct sockaddr*)&sa_in);

        sa_eg.sin6_addr = if_egress->sip6;
        sa_eg.sin6_family = AF_INET;
        /* サーバ側マスク長取得 */
        mask_eg = get_mask((struct sockaddr*)&sa_eg);

        if ((mask_in > 0) && (mask_eg > 0)) {
            if (mask_in == mask_eg) {
                int ret = is_same_net(sa_in.sin6_addr.s6_addr32,
                  sa_eg.sin6_addr.s6_addr32, mask_in, sizeof(struct in6_addr));
                if (ret) {
                    return L2_MODE;
                }
            }
            return L3_MODE;
        }
    }
    return L2_MODE;
}

#endif

/* end */
