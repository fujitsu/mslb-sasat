/**
 * file    mac_resolution.c
 * brief   ARPおよびND処理(backend)
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
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

#include "option.h"
#include "val.h"
#include "init.h"

/* common以下の共通処理 */
#include "arp_proc.c"

int resolve_mac(struct sockaddr *, struct ifdata *, uchar *, int);
static void arp_proxy_reply(struct ethhdr *, struct ether_arp *);
static void arp_gw_reply(struct ethhdr *, struct ether_arp *);
static void send_proxy_na(struct ethhdr *, struct ip6_hdr *, 
              struct nd_neighbor_solicit *, int);
static void send_gw_na(struct ethhdr *, struct ip6_hdr *,
              struct nd_neighbor_solicit *, int);

int get_target_mac(struct sockaddr *, char *, struct sockaddr*, uchar*, int);
void send_ping(struct sockaddr *, struct ifdata*);
void arp_egress_reply(struct ethhdr *eth);

/*
    @brief サーバからのMAC解決処理(multicast)
*/
void
mac_resolve_egress(struct ethhdr *eth, 
                  uint16_t prot, 
                  int len)
{
    if (is_broadcast(eth->h_dest)) {
        if ((prot == ETH_P_ARP) && if_egress->v4_enable) {
            arp_egress_reply(eth);
            return;
        }
    } else if ((prot == ETH_P_IPV6) && if_egress->v6_enable &&
               (is_multicast_v6(eth->h_dest))) {
        struct ip6_hdr *ip6h = (struct ip6_hdr*)(eth+1);
        struct icmp6_hdr *icmp6h;

        if ((ip6h->ip6_nxt == IPPROTO_ICMPV6) 
                && ((icmp6h = get_icmp6_ns(ip6h, len)) != NULL)) {
            len -= NS_BASE_SIZE;
            if (vip_mode && (cmp_mac(eth->h_dest, if_egress->fmmac) == 0)) {
                if (check_valid_ns(ip6h, icmp6h, 
                    &if_egress->vip6, 0, len) != -1) {
                    /* 仮想IPに対する応答処理 */
                    send_gw_na(eth, ip6h, 
                        (struct nd_neighbor_solicit*)icmp6h, 1);
                    return;
                }
            } else if ((eth->h_dest[2] == 0xff) &&
                        ((eth->h_dest[3] != if_egress->sip6.s6_addr[13]) ||
                        (eth->h_dest[4] != if_egress->sip6.s6_addr[14]) ||
                        (eth->h_dest[5] != if_egress->sip6.s6_addr[15]))) {
                if (check_valid_ns(ip6h, icmp6h, NULL, 1, len) != -1) {
                    /* 代理応答 */
#ifdef L_MODE
                    if (l_mode == L3_MODE) {
                        send_gw_na(eth, ip6h, 
                            (struct nd_neighbor_solicit*)icmp6h, 1);
                        return;
                    } else {
                        send_proxy_na(eth, ip6h, 
                            (struct nd_neighbor_solicit*)icmp6h, 1);
                        return;
                    }
#else
                    send_proxy_na(eth, ip6h,
                        (struct nd_neighbor_solicit*)icmp6h, 1);
                    return;
#endif
                }
            } else {
                /* nothing to do */
            }
        }
    } else {
        /* nothing to do */
    }
    evtlog("mceg", len, prot, (uchar*)eth);
    SASAT_STAT(rx_drop_eg);
}

/*
    @brief arp応答（サーバ側)
*/
void arp_egress_reply(struct ethhdr *eth)
{
    struct ether_arp *arp = (struct ether_arp*)(eth+1);

#ifdef L_MODE
    if ((cmp_ipv4((struct in_addr*)arp->arp_tpa,
            &if_egress->sip4) != 0) &&
        (cmp_ipv4((struct in_addr*)arp->arp_tpa,
            &svr_info.svr_ip4.sin_addr) != 0)) {
        /* 実IPとサーバIP(GARP)以外を処理する */
        if (l_mode == L3_MODE) {
            arp_gw_reply(eth, arp);
        } else {
            arp_proxy_reply(eth, arp);
        }
    }
#else
    if (vip_mode &&
          cmp_ipv4((struct in_addr*)arp->arp_tpa,
            &if_egress->vip4) == 0) {
        /* gwアドレスを報告 */
        arp_gw_reply(eth, arp);
    } else if ((cmp_ipv4((struct in_addr*)arp->arp_tpa,
                &if_egress->sip4) != 0) &&
               (cmp_ipv4((struct in_addr*)arp->arp_tpa,
                &svr_info.svr_ip4.sin_addr) != 0)) {
        /* ！実IPかつ!サーバIPの場合、代理応答 */
        arp_proxy_reply(eth, arp);
    } else {
        /* nothing to do */
    }
#endif
}

/*
    @brief  ipv6ユニキャストNS応答処理(サーバ側)
*/
void
mac_resolve_uc6_egress(struct ethhdr *eth, struct ip6_hdr *ip6h,
                    struct icmp6_hdr *icmp6h, int len)
{
    if (!if_egress->v6_enable) {
        SASAT_STAT(rx_drop_eg);
        return;
    }

    len -= NS_BASE_SIZE;

    if (cmp_mac(eth->h_dest, if_egress->mac) == 0) {
        if (vip_mode && 
            (check_valid_ns(ip6h, icmp6h, &if_egress->vip6, 0, len) != -1)) {
            send_gw_na(eth, ip6h, (struct nd_neighbor_solicit*)icmp6h, 0);
        }
    } else {
        if (check_valid_ns(ip6h, icmp6h, NULL, 1, len) != -1) {
            /* 代理応答 */
#ifdef L_MODE
            if (l_mode == L3_MODE) {
                 send_gw_na(eth, ip6h, (struct nd_neighbor_solicit*)icmp6h, 0);
            } else {
                send_proxy_na(eth, ip6h,
                    (struct nd_neighbor_solicit*)icmp6h, 0);
            }
#else
            send_proxy_na(eth, ip6h, (struct nd_neighbor_solicit*)icmp6h, 0);
#endif
        }
    }
}

/*
    @brief MAC代理解決処理
*/
int
resolve_mac(struct sockaddr *sa, struct ifdata *ifdata,
    uchar *mac, int default_gw)
{
    struct sockaddr_storage gw;
    int i, ret;

    gw.ss_family = sa->sa_family;

    for (i = 0; i < 3; i++)  {
        ret = get_target_mac((struct sockaddr*)sa, ifdata->ifname,
                             (struct sockaddr*)&gw, mac, default_gw);
        if (ret < 0) {
            /*　エラー */
            continue;
        }
        /* 正常終了 */
        if (cmp_mac(zerodata, mac) != 0) {
            return SVR_OK;
        }
        /* MACが不明の場合、pingを打つ */
        send_ping((struct sockaddr*)&gw, ifdata);
        anycast_sleep(10);
    }

    return SVR_INIT;
}

/*
    @brief arp代理応答処理(本体)
*/
static int 
arp_proxy_body(struct ethhdr *eth, struct ether_arp *arp, uchar *mac)
{
    struct in_addr ip;

    if (memcmp(&arp->ea_hdr, &arphdr_tmpl, sizeof(arphdr_tmpl)) != 0) {
        return -1;
    }

    memcpy(&ip, arp->arp_tpa, sizeof(ip));

    /* macヘッダ */
    copy_mac(eth->h_dest, arp->arp_sha);
    copy_mac(eth->h_source, mac);

    arp->arp_op = htons(ARPOP_REPLY);

    /* source(ip, mac) をtarget(ip, mac)へ */
    copy_mac(arp->arp_tha, arp->arp_sha);
    memcpy(arp->arp_tpa, arp->arp_spa, sizeof(struct in_addr));

    /* source macにクライアントのMACをセット */
    copy_mac(arp->arp_sha, mac);
    /* source ipのtarget ipをセット */
    memcpy(arp->arp_spa, &ip, sizeof(struct in_addr));

    /* サーバへ応答 */
    write(if_egress->sockfd, eth,
        sizeof(struct ethhdr) + sizeof(struct ether_arp));

    return 0;
}

/*
    @brief  サーバ側のMACを要求する場合、デフォルトGWのMACで応答する
*/
static void
arp_gw_reply(struct ethhdr *eth, struct ether_arp *arp)
{
    int i = 0;

    do {
        if (gw_mac_v4_valid) {
            if (arp_proxy_body(eth, arp, gw_mac_v4) != -1) {
                SASAT_STAT(tx_arp_reply_eg);
            }
            break;
        }
        /* v4 default gw */
        get_default_gw(if_ingress, 1, 0);
        i++;
    } while (i < 3);
}

/*
    @brief ARP代理応答
*/
static void
arp_proxy_reply(struct ethhdr *eth, struct ether_arp *arp)
{
    struct sockaddr_in sa;
    uchar mac[ETH_ALEN];

    sa.sin_family = AF_INET;
    sa.sin_addr = *(struct in_addr*)arp->arp_tpa;
    if (resolve_mac((struct sockaddr*)&sa, if_ingress, mac, 0) == SVR_INIT) {
        /* mac解決できなかった */
        return;
    }
    if (arp_proxy_body(eth, arp, mac) != -1) {
        SASAT_STAT(tx_proxy_arp);
    }
}

/*
    @brief  NA代理応答処理
            入力インターフェース、出力インターフェースは固定

*/
static void
send_na_body(struct ethhdr *s_eth, struct ip6_hdr *s_ip6h,
              struct nd_neighbor_solicit *s_ns, int mcast, uchar *mac)
{
    uchar buff[NS_BASE_SIZE + 8];
    struct ethhdr *eth = (struct ethhdr*)buff;
    struct ip6_hdr *ip6h = (struct ip6_hdr*)(eth+1);
    struct nd_neighbor_advert *na = (struct nd_neighbor_advert *)(ip6h+1);
    uint64_t base, sum;
    int plen;

    if (mcast) {
        /* multicast addressをチェック */
        if ((s_eth->h_dest[3] != s_ns->nd_ns_target.s6_addr[13]) ||
              (s_eth->h_dest[4] != s_ns->nd_ns_target.s6_addr[14]) ||
              (s_eth->h_dest[5] != s_ns->nd_ns_target.s6_addr[15])) {
            return;
        }
        plen = sizeof(struct nd_neighbor_advert) + 8;
    } else {
        plen = sizeof(struct nd_neighbor_advert);
    }

    memset(buff, 0, sizeof(buff));

    /* ether header */
    copy_mac(eth->h_dest, s_eth->h_source);
    copy_mac(eth->h_source, mac);
    eth->h_proto = htons(ETH_P_IPV6);

    /* ip header */
    ip6h->ip6_vfc = 0x60;
    ip6h->ip6_plen = htons(plen);
    ip6h->ip6_nxt = IPPROTO_ICMPV6;
    ip6h->ip6_hlim = 255;
    memcpy(&ip6h->ip6_src, &s_ns->nd_ns_target, sizeof(ip6h->ip6_src));
    ip6h->ip6_dst = s_ip6h->ip6_src;
    base = pseudo_sum_v6(ip6h);

    /* icmp header */
    na->nd_na_type = ND_NEIGHBOR_ADVERT;
    na->nd_na_flags_reserved = ND_NA_FLAG_SOLICITED;
    memcpy(&na->nd_na_target, &s_ns->nd_ns_target, sizeof(na->nd_na_target));

    if (mcast) {
        na->nd_na_flags_reserved |= ND_NA_FLAG_OVERRIDE;
        struct nd_opt_hdr *opt = (struct nd_opt_hdr *)(na+1);
        opt->nd_opt_type = ND_OPT_TARGET_LINKADDR;
        opt->nd_opt_len = 1;
        memcpy((uchar*)opt + 2, mac, ETH_ALEN);
    }

    /* checksum 計算 */
    sum = icmpv6_sum((const uchar *)&na->nd_na_hdr, plen, base);
    na->nd_na_hdr.icmp6_cksum = (uint16_t)~sum;

    write(if_egress->sockfd, buff,
        plen + sizeof(struct ethhdr) + sizeof(struct ip6_hdr));
}


/*
    @brief 代理応答
*/
static void
send_proxy_na(struct ethhdr *s_eth, struct ip6_hdr *s_ip6h,
              struct nd_neighbor_solicit *s_ns, int mcast)
{
    struct sockaddr_in6 sa;
    uchar mac[ETH_ALEN];

    /* クライアント側インターフェースのデータを検索 */
    sa.sin6_family = AF_INET6;
    memcpy(&sa.sin6_addr, &s_ns->nd_ns_target, sizeof(sa.sin6_addr));

    if (resolve_mac((struct sockaddr*)&sa, if_ingress, mac, 0) == SVR_INIT) {
        /* mac解決できなかった */
        return;
    }

    send_na_body(s_eth, s_ip6h, s_ns, mcast, mac);
    SASAT_STAT(tx_proxy_na);
}

/*
    @brief デフォルトGWのMACを応答
*/
static void
send_gw_na(struct ethhdr *s_eth, struct ip6_hdr *s_ip6h,
              struct nd_neighbor_solicit *s_ns, int mcast)
{
    int i = 0;

    do {
        if (gw_mac_v6_valid) {
            send_na_body(s_eth, s_ip6h, s_ns, mcast, gw_mac_v6);
            SASAT_STAT(tx_na_eg);
            break;
        }
        /* v6 default gw */
        get_default_gw(if_ingress, 0, 1);
        i++;
    } while (i < 3);
}
/* end */
