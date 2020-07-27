/**
 * file     front.c
 * brief    front translator処理 
 * note     COPYRIGHT FUJITSU LIMITED 2010
 *          FCT)Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

/*  include */
#include <unistd.h>
#include <syslog.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include "option.h"

#define VAL_SUBS
#include "log.h"
#include "val.h"
#include "anycast.h"
#include "util_inline.h"
#include "init.h"
#include "policy.h"
#include "front_properties.h"
#include "stat.h"
#include "checksum.h"
#undef  VAL_SUBS

#define DATABUF_SIZE 1520
#define MAX_RECV 128 

/* 
    static variable 
*/
/* interface情報 */
static struct ifdata if_in;
static struct ifdata if_eg;

/* 代表IP */
static struct in_addr   vip4;
static struct in6_addr  vip6;

/* 受信buffer */
static unsigned char databuff[DATABUF_SIZE];

/* VLAN header */
#if 0
struct ethvlan {
    unsigned char   h_dest[ETH_ALEN];   /* destination eth addr */
    unsigned char   h_source[ETH_ALEN]; /* source ether addr    */
    uint32_t        h_tag;
    uint32_t        h_proto;            /* packet type ID field */
} __attribute__((packed));
#endif

/* prototype */
static void proc_recv_data(unsigned char *buf, int len);
static void proc_v4(struct ethhdr *eth, struct ip *ip, int len);
static void proc_v6(struct ethhdr *eth, struct ip6_hdr *ip, int len);
static inline void proc_patrol(struct timeval *);
static void front_cleanup(void *arg);
void *front_ingress1(void *);

int command_proc(void);
void mac_resolve(struct ethhdr *, uint16_t, struct ifdata*, int);
void mac_resolve_uc6(struct ethhdr *, struct ip6_hdr *, struct icmp6_hdr *, struct ifdata*, int);
struct icmp6_hdr *get_icmp6_ns(struct ip6_hdr *, int);
int arp_reply(struct ethhdr *, struct ifdata *);

/*
    @brief front translator (-dオプションでデーモン化)
*/
int main(int argc, char *argv[])
{
    int type, ret;

    if (argc >=2 && strcmp(argv[1], "-d") == 0) {
        /* デーモン */
        daemon(1, 0);
        syslog(LOG_INFO, "Start SASAT front tranlator as a daemon");
    } else {
        syslog(LOG_INFO, "Start SASAT front translator");
    }
    
    /* nice値 */
    nice(-20);

    cpu_mhz = tsc_cycle_init();

    /* ログ初期化 */
    mlog_init();
    evtlog_init();

    memset(&if_in, 0, sizeof(if_in));
    memset(&if_eg, 0, sizeof(if_eg));

    /* 設定ファイル読み出し */
    anycast_prop_init();

    /* サーバ管理テーブル初期化 */
    init_svr_mng_table();
    /* 振り分けテーブル初期化 */
    init_policy_table(0);

    /* 動作モード読み出し */
    type = get_interface_info(&if_in, &if_eg);
    
    nt_info.count = MAX_NET_THREAD;

/*
    if (type == TYPE_TWO_ARM) {
        if_egress = &if_eg;
        if_ingress = &if_in;
        get_policy();
        ret = create_net_thread(front_egress);
        if (!ret) {
            return -1;
        }
        nt_info.th[0].tid = ret;

        ret = create_net_thread(front_ingress);
        if (!ret) {
            return -1;
        }
        nt_info.th[1].tid = ret;
    }
    else
*/
    {
        if_egress = if_ingress = &if_in;
        get_policy();
        ret = create_net_thread(front_ingress1);
        if (!ret) {
            return -1;
        }
        nt_info.th[0].tid = ret;
    }

    /* コマンド処理 */
    if (command_proc() < 0) {
        return -1;
    }

    return 0;
}

/*
    @brief 振り分け処理
    一本腕専用
*/
void * 
front_ingress1 (void *arg)
{
    struct timeval timeout;
    int fd, tno;

    pthread_detach(pthread_self());

    signal_block();

    /* ソケット初期化 */
    if (init_socket_if(if_ingress, &fd) != 0) {
        *(int*)arg = -1;
        return NULL;
    }

    if_ingress->sockfd = fd;
    vip4 = if_ingress->vip4;
    vip6 = if_ingress->vip6;
    
    tno = 0;
    pthread_cleanup_push((void*)front_cleanup, &tno);

    /* 初期化完了 */
    *(int*)arg = 1;

    timeout_set(&timeout, 5);
    /*
        書き換え処理
    */
    for ( ;; ) {
        fd_set fds;
        int ret, len, i;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        /* 待ち受け */ 
        ret = select(fd + 1, &fds, NULL, NULL, &timeout);
        
        if (likely(ret != 0)) {
            for (i = 0; i < MAX_RECV; i++) { 
                if ((len = recv(fd, databuff, DATABUF_SIZE, 0)) > 0) {
                    if (likely(len > sizeof(struct ethhdr))) {
                        proc_recv_data(databuff, len);
                    } else {
                        SASAT_STAT(rx_drop_short);
                    }
                } else {
                    break;
                }
            }
        } else {
            /* timeout */
            SASAT_STAT(select_to);
        }
        proc_patrol(&timeout);
    }

    pthread_cleanup_pop(0);
    return NULL;
}

/*
    IPパケットのみ振り分け
    その他は破棄
*/
static void
proc_recv_data(unsigned char *buf, int len)
{
    struct ethhdr *eth = (struct ethhdr *)buf;
    uint16_t prot = ntohs(eth->h_proto);

    if (unlikely(cmp_mac(eth->h_source, if_ingress->mac) == 0)) {
        return;
    }
    if (is_multicast(eth->h_dest)) {
        SASAT_STAT(rx_packet_mc);
        if (vip_mode) {
            mac_resolve(eth, prot, if_ingress, len);
        }
    } else if (prot == ETH_P_IP) {
        /* IPv4 */
        SASAT_STAT(rx_packet_v4);
        if (likely(len > (sizeof(struct ethhdr) + sizeof(struct ip)))) {
            proc_v4(eth, (struct ip*)(eth+1), len);        
        }else {
            SASAT_STAT(rx_drop_short);
        }
    } else if (prot == ETH_P_IPV6) {
        /* IPv6 */
        SASAT_STAT(rx_packet_v6);
        struct ip6_hdr *ip6h = (struct ip6_hdr*)(eth+1);
        if (likely(len > (sizeof(struct ethhdr)+sizeof(struct ip6_hdr)))) {
            struct icmp6_hdr *icmp6h;
            if ((ip6h->ip6_nxt == IPPROTO_ICMPV6) && 
                    ((icmp6h = get_icmp6_ns(ip6h, len)) != NULL) ) {
                if (vip_mode) {
                    mac_resolve_uc6(eth, ip6h, icmp6h, if_ingress, len);     
                }
            } else {
               proc_v6(eth, ip6h, len);
            }
        } else {
            SASAT_STAT(rx_drop_short);
        }
#if 0   /* VLAN */
    } else if (prot == ETH_P_8021Q) {
        struct ethvlan *vlanhdr = (struct ethvlan*)buf;
        prot = ntohs(vlanhdr->h_proto);
        
        if (prot == ETH_P_IP) {
            if (likely(len >= (sizeof(struct ethhdr)+4+sizeof(struct ip)))) {
                proc_v4(eth,(struct ip*)(buf+sizeof(struct ethhdr)+4), len);
            } else {
                SASAT_STAT(rx_drop_short);
            }
        } else if (prot == ETH_P_IPV6)  {
            if (likely(len >= 
                    (sizeof(struct ethhdr)+4+sizeof(struct ip6_hdr)))) {
                proc_v6(eth,(struct ip6_hdr*)(buf+sizeof(struct ethhdr)+4),len);
            } else {
                SASAT_STAT(rx_drop_short);
            }
        } else {
            /* 破棄 */
            SASAT_STAT(rx_drop_vlan);
        }
#endif
    } else if (prot == ETH_P_ARP) {
        /* unicast arp request応答 */
        if (if_ingress->v4_enable) {
            arp_reply(eth, if_ingress);
        }
    } else  {
        /* 破棄 */        
        SASAT_STAT(rx_drop_noip);
    }
}

/*
    @brief ipv4処理
*/
static void
proc_v4(struct ethhdr *eth, struct ip *ip, int len)
{
    lb_pol_cache_v4_t *lb;

    evtlog("prc4", len, 0, (uchar*)eth);

    if (unlikely(cmp_ipv4(&ip->ip_dst, &vip4) != 0)) {
        SASAT_STAT(rx_drop_addr_v4);
        return;
    }  
    if ((lb = get_pol_v4(ip->ip_src)) == NULL) {
        SASAT_STAT(rx_drop_policy);
        return;
    }
    copy_mac(eth->h_dest, lb->lb_dst_mac); 
    copy_mac(eth->h_source, if_egress->mac);

    ip->ip_dst = lb->lb_dst_ip;
    ip->ip_sum = htons(add16forCheckSum(ntohs(ip->ip_sum), lb->chksum_delta));

    SASAT_STAT(tx_packet_v4);
    write(if_egress->sockfd , eth, len);
}

/*
    @brief ipv6処理
*/
static void
proc_v6(struct ethhdr *eth, struct ip6_hdr *ip, int len)
{
    lb_pol_cache_v6_t *lb;

    evtlog("prc6", len, 0, (uchar*)eth);

    if (unlikely(cmp_ipv6(&ip->ip6_dst, &vip6) != 0)) {
        SASAT_STAT(rx_drop_addr_v6);
        return;
    }  
    if ((lb = get_pol_v6(&ip->ip6_src)) == NULL) {
        SASAT_STAT(rx_drop_policy);
        return;
    }
    
    copy_mac(eth->h_dest, lb->lb_dst_mac); 
    copy_mac(eth->h_source, if_egress->mac);

    ip->ip6_dst = lb->lb_dst_ip;

    SASAT_STAT(tx_packet_v6);
    write(if_egress->sockfd, eth, len);
}

/*
    @brief 振り分けテーブル関連の処理　排他の関係で振り分けスレッドで動作   
*/
static inline void
proc_patrol(struct timeval *to)
{
    int tv = 5;

    if (unlikely(patrol_flag != 0)) {
    
        SASAT_STAT(clr_policy_cache);

        if (patrol_flag & DEL_CACHE4) {
            static int ptr4;

            ptr4 = clear_v4_cache(ptr4);

            if (ptr4 == LB_POL_DIVISOR) {
                ptr4 = 0;
                patrol_flag &= ~DEL_CACHE4;
            }
        }
        if (patrol_flag & DEL_CACHE6) {
            static int ptr6;

            ptr6 = clear_v6_cache(ptr6);
            
            if (ptr6 == LB_POL_DIVISOR) {
                ptr6 = 0;
                patrol_flag &= ~DEL_CACHE6;
            }
        }    

        if (patrol_flag) {
            tv = 1;
        }
    }

    timeout_set(to, tv);
}

/*
    スレッドcleanup ハンドラ
*/
static void
front_cleanup(void *arg)
{
    int tno = *(int*)arg;

    if (likely(if_ingress->sockfd != 0)) {
        close(if_ingress->sockfd);
        if_ingress->sockfd = 0;
    }

    nt_info.th[tno].cancel_end = 1;
}

/* end */
