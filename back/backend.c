/**
 * file     backend.c
 * brief    backend translator処理 
 * note     COPYRIGHT FUJITSU LIMITED 2010
 *          Yagi
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

/*  include */
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <pthread.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include "option.h"

#define VAL_SUBS
#include "anycast.h"
#include "util_inline.h"
#include "init.h"
#include "back_properties.h"
#include "stat.h"
#undef  VAL_SUBS

#include "checksum.h"
#include "client_tbl.h"

#define DATABUF_SIZE 1520
#define MAX_RECV 128 

/* 
    static variable 
*/
/* interface情報 */
static struct ifdata if_in;
static struct ifdata if_eg;

/* prototype */
static void proc_ingress_data(unsigned char *buf, int);
static void proc_egress_data(unsigned char *buf, int);
static void proc_v4_in(struct ethhdr *eth, struct ip *, int);
static void proc_v6_in(struct ethhdr *eth, struct ip6_hdr *, int);
static void proc_v4_eg_vip(struct ethhdr *eth, struct ip *, int);
static void proc_v6_eg_vip(struct ethhdr *eth, struct ip6_hdr *, int);
static void proc_v4_eg_novip(struct ethhdr *eth, struct ip *, int);
static void proc_v6_eg_novip(struct ethhdr *eth, struct ip6_hdr *, int);

void *back_ingress(void *arg);
void *back_egress(void *arg);

int command_proc(void);
void mac_resolve_egress(struct ethhdr *, uint16_t, int);
void mac_resolve_uc6_egress(struct ethhdr *, struct ip6_hdr *, struct icmp6_hdr *, int);
void mac_resolve(struct ethhdr *, uint16_t, struct ifdata *, int);
struct icmp6_hdr *get_icmp6_ns(struct ip6_hdr *, int);
void mac_resolve_uc6(struct ethhdr *, struct ip6_hdr *, struct icmp6_hdr *, struct ifdata*, int);
int resolve_mac(struct sockaddr *, struct ifdata *, uchar *, int);
int arp_reply(struct ethhdr *, struct ifdata *);
int arp_egress_reply(struct ethhdr *);

#define THREAD_NUM  2

/*
    @brief backend translator (-dオプションでデーモン化)
*/
int main(int argc, char *argv[])
{
    int ret;
    static volatile int flag[THREAD_NUM];

    if (argc >=2 && strcmp(argv[1], "-d") == 0) {
        /* デーモン */
        daemon(1,0);
        syslog(LOG_INFO, "Start SASAT backend tranlator as a daemon");
    } else {
        syslog(LOG_INFO, "Start SASAT backend translator");
    }

    nice(-20);

    /* "cpu MHz"値取得(タイムスタンプ用) */
    cpu_mhz = tsc_cycle_init();
 
    /* ログ初期化 */
    mlog_init();
    evtlog_init();

    /* クライアント情報テーブル初期化 */
    init_client_table();

    memset(&if_in, 0, sizeof(if_in));
    memset(&if_eg, 0, sizeof(if_eg));

    if_egress = &if_eg;
    if_ingress = &if_in;

    /* 設定ファイル読み出し */
    anycast_prop_init();

    /* インターフェース情報読み込み */
    get_interface_info(&if_in, &if_eg);

    /* サーバ情報読み込み */
    get_svr_info();

    /* v4, v6それぞれの情報が十分かチェックする */
    marge_info();

#ifdef L_MODE
    /* 
       L2orL3判定
       (2つのインターフェースのネットワークが同じかどうか) 
    */
    l_mode = get_connect_mode();
#endif

    /* デフォルトGW情報の取得 */
    get_default_gw(if_ingress, if_ingress->v4_enable,
        if_ingress->v6_enable);

    /* スレッド同期変数初期化 */
    sync_init((volatile int*)flag, THREAD_NUM);

    /* 下り(サーバ側)パケット処理スレッド起動 */
    ret = create_back_thread(back_egress, &flag[0]);
    if (ret < 0) {
        /* 起動失敗のため停止 */
        return -1;
    }
    /* 上り(クライアント側）パケット処理スレッド起動 */
    ret = create_back_thread(back_ingress, &flag[1]);
    if (ret < 0 ) {
        return -1;
    }

    /* スレッド同期 */
    wait_thread((volatile int*)flag, THREAD_NUM);

    /* コマンド待ち受け処理へ */
    if (command_proc() < 0) {
        return -1;
    }

    return 0;
}

/*
    @brief 書き換え処理 入力(クライアント）側
*/
void * 
back_ingress(void *arg)
{
    static unsigned char databuff[DATABUF_SIZE];
    int fd;

    pthread_detach(pthread_self());

    /* ソケット初期化 */
    if (init_socket_if(if_ingress, 0, &fd) != 0) {
        exit(1);
    }

    signal_block();

    if_ingress->sockfd = fd;

    /* 起動をメインスレッドへ通知 */
    sync_thread((volatile int*)arg);

    /*
        書き換え処理
    */
    for ( ;; ) {
        fd_set fds;
        int ret, len, i;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        /* 待ち受け */
        ret = select(fd + 1, &fds, NULL, NULL, NULL);
 
        if (likely(ret != 0)) {
            for (i = 0; i < MAX_RECV; i++) {
                if ((len = recv(fd, databuff, DATABUF_SIZE, 0)) > 0) {
                    if (likely(len > sizeof(struct ethhdr))) {
                        proc_ingress_data(databuff, len);
                    } else {
                        SASAT_STAT(rx_drop_short_in);
                    }
                } else {
                    break;
                }
            }
        }
    }
    return NULL;
}

/*
    @brief IPパケットのみ振り分け その他は破棄
    @param buf
    @param len
*/
static void
proc_ingress_data(unsigned char *buf, int len)
{
    struct ethhdr *eth = (struct ethhdr *)buf;
    uint16_t prot = ntohs(eth->h_proto);

    if (unlikely(cmp_mac(eth->h_source, if_ingress->mac) == 0)) {
        /* 送信元が自MACアドレスの場合処理しない（他プロセス送信）*/
        return;
    }
    if (unlikely(is_multicast(eth->h_dest))) {
        /* multicast(bcast)の場合、arp要求/NS処理を行う */
        SASAT_STAT(rx_packet_mc_in);
        mac_resolve(eth, prot, if_ingress, len);
    } else if (prot == ETH_P_IP) {
        /* IPv4 */
        SASAT_STAT(rx_packet_v4_in);
        if (likely(len >= (sizeof(struct ethhdr) + sizeof(struct ip)))) {
            proc_v4_in(eth, (struct ip*)(eth+1), len);
        } else {
            SASAT_STAT(rx_drop_short_in);
        }
    } else if (prot == ETH_P_IPV6) {
        /* IPv6 */
        SASAT_STAT(rx_packet_v6_in);
        struct ip6_hdr *ip6h = (struct ip6_hdr*)(eth+1);
        if (likely(len > (sizeof(struct ethhdr)+sizeof(struct ip6_hdr)))) {
            struct icmp6_hdr *icmp6h=NULL;
            if (unlikely((ip6h->ip6_nxt == IPPROTO_ICMPV6) &&
                    ((icmp6h = get_icmp6_ns(ip6h, len)) != NULL))) {
                if (vip_mode) {
                    /* unicastのNS処理 */
                    mac_resolve_uc6(eth, ip6h, icmp6h, if_ingress, len);
                }
            } else {
                proc_v6_in(eth, ip6h, len);
            }
        } else {
            SASAT_STAT(rx_drop_short_in);
        }
    } else if (prot == ETH_P_ARP) {
        /* unicast arp request応答 */
        if (if_ingress->v4_enable) {
            if (arp_reply(eth, if_ingress)) {
                SASAT_STAT(tx_arp_reply_in);
            }
        }
    } else {
        /* 破棄 */
        SASAT_STAT(rx_drop_noip_in);
    }
}

/*
    @brief ipv4処理
*/
static void
proc_v4_in(struct ethhdr *eth, struct ip *ip, int len)
{
    evtlog("pri4", len, 0, (uchar*)eth);

    if (unlikely(cmp_ipv4(&ip->ip_dst, &if_ingress->vip4) != 0)) {
        /* 宛先 */
        SASAT_STAT(rx_drop_addr_v4_in);
        return;
    }

    get_ci_up4(&ip->ip_src);
 
    if (unlikely(svr_info.stat == SVR_INIT)) {
        /* serverのMACアドレスが不明の場合、解決する */
        if (resolve_mac((struct sockaddr*)&svr_info.svr_ip4,
                if_egress, svr_info.svr_mac, 0) == 0) {
            SASAT_STAT(rx_drop_in);
            return;
        }
        svr_info.stat = SVR_OK;
    }

    copy_mac(eth->h_dest, svr_info.svr_mac);
    ip->ip_dst = svr_info.svr_ip4.sin_addr;
    ip->ip_sum = htons(add16forCheckSum(ntohs(ip->ip_sum),
        svr_info.checksum_delta));

    write(if_egress->sockfd , eth, len);

    SASAT_STAT(tx_packet_v4_in);
}

/*
    @brief ipv6処理
*/
static void
proc_v6_in(struct ethhdr *eth, struct ip6_hdr *ip, int len)
{
    evtlog("prc6", len, 0, (uchar*)eth);

    if (unlikely(cmp_ipv6(&ip->ip6_dst, &if_ingress->vip6) != 0)) {
        SASAT_STAT(rx_drop_addr_v6_in);
        return;
    }

    get_ci_up6(&ip->ip6_src);
 
    if (unlikely(svr_info.stat == SVR_INIT)) {
        if (resolve_mac((struct sockaddr*)&svr_info.svr_ip6,
                if_egress, svr_info.svr_mac, 0) == 0) {
            SASAT_STAT(rx_drop_in);
            return;
        }
        svr_info.stat = SVR_OK;
    }

    copy_mac(eth->h_dest, svr_info.svr_mac);
    ip->ip6_dst = svr_info.svr_ip6.sin6_addr;

    write(if_egress->sockfd, eth, len);

    SASAT_STAT(tx_packet_v6_in);
}

/*
    @brief vip_modeの設定により切り替える関数リスト
*/
static void (*v4_eg_list[])(struct ethhdr*, struct ip*, int) = {
    proc_v4_eg_novip, proc_v4_eg_vip};
static void (*v6_eg_list[])(struct ethhdr*, struct ip6_hdr*, int) = {
    proc_v6_eg_novip, proc_v6_eg_vip};

static void (*proc_v4_eg)(struct ethhdr*, struct ip*, int);
static void (*proc_v6_eg)(struct ethhdr*, struct ip6_hdr*, int);

/*
    @brief 書き換え処理 サーバ側インターフェース処理
*/
void *
back_egress(void *arg)
{
    static unsigned char databuff[DATABUF_SIZE];
    int fd;

    pthread_detach(pthread_self());

    /* ソケット初期化 */
    if (init_socket_if(if_egress, 1, &fd) != 0) {
        exit(1);
    }

    signal_block();

    if_egress->sockfd = fd;

    proc_v4_eg = v4_eg_list[vip_mode];
    proc_v6_eg = v6_eg_list[vip_mode];

    /* 起動を通知 */
    sync_thread((volatile int*)arg);

    /*
        書き換え処理
    */
    for ( ;; ) {
        fd_set fds;
        int ret, len, i;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        /* 待ち受け */
        ret = select(fd + 1, &fds, NULL, NULL, NULL);

        if (likely(ret != 0)) {
            for (i = 0; i < MAX_RECV; i++) {
                if ((len = recv(fd, databuff, DATABUF_SIZE, 0)) > 0) {
                    if (likely(len > sizeof(struct ethhdr))) {
                        proc_egress_data(databuff, len);
                    } else {
                        SASAT_STAT(rx_drop_short_eg);
                    }
                } else {
                    break;
                }
            }
        }
    }
    return NULL;
}

/*
    @brief IPパケットのみ振り分け
    その他は破棄
*/
static void
proc_egress_data(unsigned char *buf, int len)
{
    struct ethhdr *eth = (struct ethhdr *)buf;
    uint16_t prot = ntohs(eth->h_proto);

    if (unlikely(cmp_mac(eth->h_source, if_egress->mac) == 0)) {
        return;
    } 
    if (unlikely(is_multicast(eth->h_dest))) {
        SASAT_STAT(rx_packet_mc_eg);
        mac_resolve_egress(eth, prot, len);
    } else if (prot == ETH_P_IP) {
        /* IPv4 */
        SASAT_STAT(rx_packet_v4_eg);
        if (likely(len >= (sizeof(struct ethhdr) + sizeof(struct ip)))) {
            proc_v4_eg(eth, (struct ip*)(eth+1), len);
        }else {
            SASAT_STAT(rx_drop_short_eg);
        }
    } else if (prot == ETH_P_IPV6) {
        /* IPv6 */
        SASAT_STAT(rx_packet_v6_eg);
        struct ip6_hdr *ip6h = (struct ip6_hdr*)(eth+1);
        if (likely(len > (sizeof(struct ethhdr)+sizeof(struct ip6_hdr)))) {
            struct icmp6_hdr *icmp6h;
            if ((ip6h->ip6_nxt == IPPROTO_ICMPV6) &&
                    ((icmp6h = get_icmp6_ns(ip6h, len)) != NULL)) {
                mac_resolve_uc6_egress(eth, ip6h, icmp6h, len);
            } else {
                proc_v6_eg(eth, ip6h, len);
            }
        } else {
            SASAT_STAT(rx_drop_short_eg);
        }
    } else if (prot == ETH_P_ARP) {
        /* unicast arp request応答 */
        if (if_egress->v4_enable) {
            arp_egress_reply(eth);
        }
    } else {
        /* 破棄 */
        SASAT_STAT(rx_drop_noip_eg);
    }
}

/*
    @brief ipv4処理(仮想IPモード)
*/
static void
proc_v4_eg_vip(struct ethhdr *eth, struct ip *ip, int len)
{
    evtlog("pre4", len, 0, (uchar*)eth);

    (void)get_ci_dwn4(&ip->ip_dst);

    write(if_ingress->sockfd, eth, len);

    SASAT_STAT(tx_packet_v4_eg);
}

/*
    @brief ipv6処理(仮想IPモード)
*/
static void
proc_v6_eg_vip(struct ethhdr *eth, struct ip6_hdr *ip, int len)
{
    evtlog("pre6", len, 0, (uchar*)eth);

    (void)get_ci_dwn6(&ip->ip6_dst);

    write(if_ingress->sockfd, eth, len);

    SASAT_STAT(tx_packet_v6_eg);
}

static inline int
get_gw_mac4(void)
{
    get_default_gw(if_ingress, 1, 0);
    return gw_mac_v4_valid;
}

/*
    @brief ipv4処理（非仮想IPモーﾄﾞ)
*/
static void
proc_v4_eg_novip(struct ethhdr *eth, struct ip *ip, int len)
{
    evtlog("prn4", len, 0, (uchar*)eth);

    (void)get_ci_dwn4(&ip->ip_dst);

    if (cmp_mac(eth->h_dest, if_egress->mac) == 0) {
        if (unlikely(!gw_mac_v4_valid && (get_gw_mac4() == 0))) {
            SASAT_STAT(tx_drop_mac4);
            return;
        }
        copy_mac(eth->h_dest, gw_mac_v4);
    }

    write(if_ingress->sockfd, eth, len);

    SASAT_STAT(tx_packet_v4_eg);
}

static inline int
get_gw_mac6(void)
{
    get_default_gw(if_ingress, 0, 1);
    return gw_mac_v6_valid;
}

/*
    @brief ipv6処理（非仮想IPモード)
*/
static void
proc_v6_eg_novip(struct ethhdr *eth, struct ip6_hdr *ip, int len)
{
    evtlog("prn6", len, 0, (uchar*)eth);

    (void)get_ci_dwn6(&ip->ip6_dst);

    if (cmp_mac(eth->h_dest, if_egress->mac) == 0) {
        if (unlikely(!gw_mac_v6_valid && (get_gw_mac6() == 0))) {
            SASAT_STAT(tx_drop_mac6);
            return;
        }
        copy_mac(eth->h_dest, gw_mac_v6);
    }

    write(if_ingress->sockfd, eth, len);

    SASAT_STAT(tx_packet_v6_eg);
}

/* end */
