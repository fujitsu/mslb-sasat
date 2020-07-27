/**
 * file    arp_proc.c
 * brief   ARPおよびND処理
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT) Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

#include <unistd.h> 
#include <string.h>
#include <linux/types.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/if_ether.h>

#include "val.h"
#include "util_inline.h"
#include "stat.h"
#include "checksum.h"

int arp_reply(struct ethhdr *, struct ifdata*);
static int check_valid_ns(struct ip6_hdr*, struct icmp6_hdr *, struct in6_addr *, int, int);
static void send_icmp6_na(struct ethhdr *, struct ip6_hdr *, struct ifdata*, int);

struct icmp6_hdr *get_icmp6_ns(struct ip6_hdr *, int);

#define NS_BASE_SIZE \
    (sizeof(struct ethhdr)+sizeof(struct ip6_hdr)+sizeof(struct nd_neighbor_solicit))

/*
    @brief マルチキャストフレームでMAC解決処理の場合は応答する
    @param ethhdr
    @param prot プロトコル番号
    @param ifdata 入力インターフェース情報 
    @param len 受信サイズ
*/
void
mac_resolve(struct ethhdr *eth, uint16_t prot, struct ifdata *ifdata, int len)
{
    if (is_broadcast(eth->h_dest)) {
        if ((prot == ETH_P_ARP) && ifdata->v4_enable) {
            /* 有効なARPリクエストであることを確認後応答する */
            arp_reply(eth, ifdata);
            return;
        }
    } else if ((prot == ETH_P_IPV6) && ifdata->v6_enable &&
               (cmp_mac(ifdata->fmmac, eth->h_dest) == 0)) {
        struct ip6_hdr *ip6h = (struct ip6_hdr*)(eth+1);
        struct icmp6_hdr *icmp6h;

        if ((ip6h->ip6_nxt == IPPROTO_ICMPV6)
                && ((icmp6h = get_icmp6_ns(ip6h, len)) != NULL)) {
            len -= NS_BASE_SIZE; 

            if (check_valid_ns(ip6h, icmp6h, &ifdata->vip6, 0, len) != -1) {
                send_icmp6_na(eth, ip6h, ifdata, 1);
                return;
            }
        }
    } else {
        /* nothing to do */
    }

#ifdef FRONT_T
    SASAT_STAT(rx_drop);
#else
    SASAT_STAT(rx_drop_in);
#endif
    evtlog("mcd0", len, prot, (uchar*)eth); 
}

/*
    @brief arpヘッダのテンプレート(受信チェック用)
*/
const struct arphdr arphdr_tmpl = {
    .ar_hrd = bswap2(ARPHRD_ETHER),
    .ar_pro = bswap2(ETH_P_IP),
    .ar_hln = 6,
    .ar_pln = 4,
    .ar_op  = bswap2(ARPOP_REQUEST),
};

/*
    @brief arp応答処理
    @param ethhdr
    @param len length
    @param ifdata 出力先のインターフェース情報
*/
int
arp_reply(struct ethhdr *machdr, struct ifdata *ifdata)
{
    struct ether_arp *arp = (struct ether_arp*)(machdr + 1);
    struct in_addr ip;

    if (memcmp(&arp->ea_hdr, &arphdr_tmpl, sizeof(struct arphdr)) != 0) {
        /* ヘッダ不一致、破棄 */
        return 0;
    }

    memcpy(&ip, arp->arp_tpa, sizeof(ip));

    if (cmp_ipv4(&ip, &ifdata->vip4) != 0) {
        return 0;
    }

    /* macヘッダ */
    copy_mac(machdr->h_dest, arp->arp_sha);
    copy_mac(machdr->h_source, ifdata->mac);

    arp->arp_op = htons(ARPOP_REPLY);

    /* source(ip, mac) をtarget(ip, mac)へ */
    copy_mac(arp->arp_tha, arp->arp_sha);
    memcpy(arp->arp_tpa, arp->arp_spa, sizeof(struct in_addr));

    /* source macに送信インターフェースの情報をセット */
    copy_mac(arp->arp_sha, ifdata->mac);
    /* source ipのtarget ipをセット */
    memcpy(arp->arp_spa, &ip, sizeof(struct in_addr));

    write(ifdata->sockfd, machdr,
        sizeof(struct ethhdr) + sizeof(struct ether_arp));

#ifdef FRONT_T
    SASAT_STAT(tx_arp_reply);
#endif
    return 1;
}

/*
    @brief unicast(anycast)のNS受信処理(IPv6)
    @param ethhdr
    @param ip6_hdr
    @param icmp6_hdr
    @param ifdata 入力インターフェース情報
    @param len 受信サイズ
*/
void
mac_resolve_uc6(struct ethhdr *eth, struct ip6_hdr *ip6h, 
                struct icmp6_hdr *icmp6h, struct ifdata *ifdata, int len)
{
    /* 宛先チェック */
    if (!ifdata->v6_enable || (cmp_ipv6(&ip6h->ip6_dst, &ifdata->vip6) != 0)) {
#ifdef FRONT_T
        SASAT_STAT(rx_drop);
#else
        SASAT_STAT(rx_drop_in);
#endif
        return;
    }

    /* lengthチェック済み */
    len -= NS_BASE_SIZE;

    if ( check_valid_ns(ip6h, icmp6h, &ifdata->vip6, 0, len) < 0) {
        /* invalid */
#ifdef FRONT_T
        SASAT_STAT(rx_drop);
#else
        SASAT_STAT(rx_drop_in);
#endif
        return;
    }
    send_icmp6_na(eth, ip6h, ifdata, 0);
}


/*
    @brief icmp6 近隣要請メッセージの場合、icmp6ヘッダアドレスを取得
    @param ip6_hdr
    @param len 受信サイズ
*/
struct icmp6_hdr *
get_icmp6_ns(struct ip6_hdr *ip6h, int len)
{
    struct icmp6_hdr *icmp6h;

    if ((ip6h->ip6_plen < sizeof(struct nd_neighbor_solicit)) ||
            (len < NS_BASE_SIZE)) {
        return NULL;
    } 

    icmp6h = (struct icmp6_hdr*)(ip6h+1);
    if (icmp6h->icmp6_type == ND_NEIGHBOR_SOLICIT) {
        return icmp6h;
    }
    return NULL;
}

/*
    @brief 有効な（処理すべき）NSであるかどうかをチェックする
    @param ip6_hdr
    @param icmp6_hdr
    @param in6_addr 
    @param proxy_mode
    @param optlen
*/
static int
check_valid_ns(struct ip6_hdr *ip6h,
               struct icmp6_hdr *icmp6h,
               struct in6_addr *ipaddr,
               int proxy_mode,
               int optlen)
{
    uint64_t sum, base;
    struct nd_neighbor_solicit *ns;
    struct nd_opt_hdr *opt;

    if (cmp_ipv6(&ip6h->ip6_src, (struct in6_addr*)zerodata) == 0) {
        /* 重複アドレスチェックは無視する */
        return -1;
    }

    /* check hop limit and icmp code */
    if ((ip6h->ip6_hlim != 255) || icmp6h->icmp6_code) {
        return -1;
    }
    /* checksum check */
    base = pseudo_sum_v6(ip6h);
    sum = icmpv6_sum((const uchar *)icmp6h, ntohs(ip6h->ip6_plen), base);
    if (sum != 0xffff) {
        return -1;
    }

    /* target address チェック */
    ns = (struct nd_neighbor_solicit*)icmp6h;
#if FRONT_T
    /* マルチキャストまたはターゲットアドレス不一致 */
    if ((ns->nd_ns_target.s6_addr[0] == 0xff) ||
         (cmp_ipv6(&ns->nd_ns_target, ipaddr) != 0)) {
        return -1;
    }
#else
    /* マルチキャストまたはターゲットアドレス不一致 */
    if ((ns->nd_ns_target.s6_addr[0] == 0xff) ||
         (!proxy_mode && (cmp_ipv6(&ns->nd_ns_target, ipaddr) != 0))) {
        return -1;
    } 
#endif

    /* オプションのフォーマットをチェック */
    opt = (struct nd_opt_hdr*)(ns+1);
    while (optlen) {
        int len;
        if ((optlen < 8) || ((len = opt->nd_opt_len) == 0)) {
            return -1;
        }
        len *= 8;
        if (len > optlen) {
            return -1;
        }
        opt = (struct nd_opt_hdr*)((uchar*)opt + len);
        optlen -= len;
    }

    return 0;
}

/*
    @brief neigbor advertisement送信
    @param ethhdr
    @param ip6_hdr
    @param ifdata
    @param mcast  マルチキャスト受信かどうか

*/
static void
send_icmp6_na(struct ethhdr *s_eth, struct ip6_hdr *s_ip6h, struct ifdata *oif, int mcast)
{
    uchar buff[NS_BASE_SIZE + 8]; 
    struct ethhdr *eth = (struct ethhdr*)buff;
    struct ip6_hdr *ip6h = (struct ip6_hdr*)(eth+1);
    struct nd_neighbor_advert *na = (struct nd_neighbor_advert *)(ip6h+1);
    uint64_t base, sum;
    int plen;

    memset(buff, 0, sizeof(buff));

    if (mcast) {
        plen =  sizeof(struct nd_neighbor_advert) + 8;
    } else {
        plen =  sizeof(struct nd_neighbor_advert); 
    }

    /* ether header */
    copy_mac(eth->h_dest, s_eth->h_source);
    copy_mac(eth->h_source, oif->mac);
    eth->h_proto = htons(ETH_P_IPV6);

    /* ip header */
    ip6h->ip6_vfc = 0x60;
    ip6h->ip6_plen = htons(plen);
    ip6h->ip6_nxt = IPPROTO_ICMPV6;
    ip6h->ip6_hlim = 255;
    ip6h->ip6_src = oif->vip6;
    ip6h->ip6_dst = s_ip6h->ip6_src;
    base = pseudo_sum_v6(ip6h);

    /* icmp header */
    na->nd_na_type = ND_NEIGHBOR_ADVERT;
    na->nd_na_flags_reserved = ND_NA_FLAG_SOLICITED;
    na->nd_na_target = oif->vip6;

    if (mcast) {
        na->nd_na_flags_reserved |= ND_NA_FLAG_OVERRIDE;
        struct nd_opt_hdr *opt = (struct nd_opt_hdr *)(na+1);
        opt->nd_opt_type = ND_OPT_TARGET_LINKADDR;
        opt->nd_opt_len = 1;
        memcpy((uchar*)opt + 2, &oif->mac, ETH_ALEN);
    }

    /* checksum 計算 */
    sum = icmpv6_sum((const uchar *)&na->nd_na_hdr, plen, base);
    na->nd_na_hdr.icmp6_cksum = (uint16_t)~sum;

    write(oif->sockfd, buff,
        plen + sizeof(struct ethhdr) + sizeof(struct ip6_hdr));

#ifdef FRONT_T
    SASAT_STAT(tx_na);
#else
    SASAT_STAT(tx_na_in);
#endif
}

/*
    @brief garp送信
    @param ifdata
    @param start_time 前回送信時間
    @param interval 送信間隔
*/
void
send_garp(struct ifdata *ifdata, uint64_t *start_time, uint interval)
{
    uint64_t ctime, stime;
    uint diff_sec;

    ctime = rdtsc();
    stime = *start_time;

    if (ctime <= stime) {
        *start_time = ctime;
        return;
    }

    /* 前回送信からの秒数を計算 */
    diff_sec = get_sec(ctime - stime);

    if (diff_sec >= interval) {
        static const unsigned char bmac[] = {0xff,0xff,0xff,0xff,0xff,0xff};
        uchar buff[sizeof(struct ethhdr) + sizeof(struct ether_arp)];
        struct ethhdr *eth = (struct ethhdr*)buff;
        struct ether_arp *arp = (struct ether_arp*)(eth+1);

        copy_mac(eth->h_dest, bmac);
        copy_mac(eth->h_source, ifdata->mac);
        eth->h_proto = htons(ETH_P_ARP);

        memcpy(&arp->ea_hdr, &arphdr_tmpl, sizeof(struct arphdr));
        copy_mac(arp->arp_sha, ifdata->mac);
        memcpy(&arp->arp_spa, &ifdata->sip4, 4);
        copy_mac(arp->arp_tha, bmac);
        memcpy(&arp->arp_tpa, &ifdata->sip4, 4);

        write(ifdata->sockfd, buff, sizeof(buff)); 

#if 0
        /* 2つめ */
        anycast_sleep(0);
        write(ifdata->sockfd, buff, sizeof(buff));
#endif

        *start_time = ctime;
    }
}
/* end */
