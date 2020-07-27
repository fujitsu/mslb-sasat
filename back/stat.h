/**
 * file    stat.h
 * brief   統計
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef __ANC_STAT__
#define __ANC_STAT__

#include "stat_common.h"

/*
    @brief 統計種別 
*/
enum {
    rx_packet_v6_in = 0,
    tx_packet_v6_in,
    rx_packet_v4_in,
    tx_packet_v4_in,

    rx_packet_mc_in,

    tx_arp_reply_in,
    tx_na_in,
    rx_drop_addr_v6_in,
    rx_drop_addr_v4_in,

    rx_drop_noip_in,
    rx_drop_short_in,
    rx_drop_in,

    rx_packet_v6_eg,
    tx_packet_v6_eg,
    rx_packet_v4_eg,
    tx_packet_v4_eg,

    rx_packet_mc_eg,

    tx_arp_reply_eg,
    tx_na_eg,
    tx_proxy_arp,
    tx_proxy_na,

    rx_drop_addr_v6_eg,
    rx_drop_addr_v4_eg,
    rx_drop_noip_eg,
    rx_drop_short_eg,

    rx_drop_eg,

    tx_drop_mac6,
    tx_drop_mac4,

    cmd_dump_req,
    cmd_trace,
    cmd_illegal,

    STAT_MAX
};

#ifdef VAL_SUBS
struct stat_member sstat[STAT_MAX] = {
/* ingress */
    {0, ":rx packets v6(in)\n"},
    {0, ":tx packets v6(in)\n"},
    {0, ":rx packets v4(in)\n"},
    {0, ":tx packets v4(in)\n"},

    {0, ":rx packets multicast(in)\n"},
    {0, ":tx arp reply(in)\n"},
    {0, ":tx neighbor adv(in)\n"},
    {0, ":rx drop(v6 address/in)\n"},

    {0, ":rx drop(v4 address/in)\n"},
    {0, ":rx drop(not ip/in)\n"},
    {0, ":rx drop(short/in)\n"},
    {0, ":rx drop(other/in)\n"},

/* egress */
    {0, ":rx packets v6(out)\n"},
    {0, ":tx packets v6(out)\n"},
    {0, ":rx packets v4(out)\n"},
    {0, ":tx packets v4(out)\n"},

    {0, ":rx packets multicast(out)\n"},
    {0, ":tx arp reply(out)\n"},
    {0, ":tx neighbor adv(out)\n"},
    {0, ":tx proxy arp(out)\n"},

    {0, ":tx proxy neighbor adv(out)\n"},
    {0, ":rx drop(v6 address/out)\n"},
    {0, ":rx drop(v4 address/out)\n"},
    {0, ":rx drop(not ip/out)\n"},

    {0, ":rx drop(short/out)\n"},
    {0, ":rx drop(other/out)\n"},

    {0, ":tx drop(mac error v6/out)\n"},
    {0, ":tx drop(mac error v4/out)\n"},

/* command */
    {0, ":command dump req\n"},
    {0, ":command event trace ctrl\n"},
    {0, ":command illegal request\n"}
};

#else
extern struct stat_member sstat[STAT_MAX];
#endif

#define SASAT_STAT(member)  (sstat[member].stat++)

#endif
