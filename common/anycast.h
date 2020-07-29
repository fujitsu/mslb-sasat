/**
 * file    anycast.h
 * brief   全体定義
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef __ANYCASTING__
#define __ANYCASTING__

#include <stdint.h>
#include <pthread.h>
#include <linux/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip6.h>

typedef unsigned char	uchar;

/* 構成（一本腕、通過）*/
enum {
    TYPE_ONE_ARM = 0,
    TYPE_TWO_ARM,

#ifdef FRONT_T
    MAX_NET_THREAD = 1,
#else
    MAX_NET_THREAD = 2,
#endif
};

/* network構成 */
enum {
    L2_MODE = 0,
    L3_MODE,
};

/*
    network処理スレッド管理
*/
struct net_thread_info {
    int count;                      /* 数 */
    struct {
        pthread_t tid;
        volatile int cancel_end;    /* cancel時通信フラグ */
    } th[MAX_NET_THREAD];
};

/*
    translatorが使用するインターフェース情報
*/
struct ifdata {
    int sockfd;                 /* socket */
    
    uint8_t mac[ETH_ALEN];
    uint8_t v4_enable;
    uint8_t v6_enable;

    uint8_t fmmac[ETH_ALEN];    /* v6 multicast filter mac */
    uint8_t _rsv[2];

    struct in_addr  sip4;       /* 実IPv4 */
    struct in_addr  vip4;       /* 仮想IP */

    struct in6_addr sip6;       /* 実IPv6 */
    struct in6_addr vip6;       /* 仮想IP */

    int ping4_fd;
    int ping6_fd;

    char ifname[16];            /* ifname */
};

/* 要求メッセージ 失敗理由コード */
enum {
    NET_REQ_SOCKET_ERR = 1,     /* socketの生成失敗 */
    NET_REQ_GIFINDEX_ERR,       /* ioctlの失敗(IF情報の取得) */
    NET_REQ_GIFFLAGS_ERR,       /* ioctlの失敗(IFフラグの取得) */
    NET_REQ_SIFFLAGS_ERR,       /* ioctlの失敗(IFフラグのセット) */
    NET_REQ_BIND_ERR,           /* bindの失敗 */
    NET_REQ_SO_BINDTODEVICE_ERR,    /* sockopt(SO_BINDTODEVICE)の失敗 */
    NET_REQ_FCNTL_ERR,          /* fcntlの失敗 */
    NET_REQ_ADD_MEMBERSHIP_ERR, /* multicast */

    NET_REQ_OK = 0,
    NET_REQ_NG = -1,
};

#define PID_PATH    "/var/run/sasat.pid"
#define LOG_PATH    "/var/opt/sasat/log/"


#endif
