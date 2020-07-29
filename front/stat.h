/**
 * file    stat.h
 * brief   統計
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef __ANC_STAT__
#define __ANC_STAT__

#include "../common/stat_common.h"
/*
    @brief 統計種別 
*/
enum {
    rx_packet_v6 = 0,
    tx_packet_v6,
    rx_packet_v4,
    tx_packet_v4,
    
    rx_packet_mc, 
    tx_arp_reply,
    tx_na,
    rx_drop_addr_v6,

    rx_drop_addr_v4,
    rx_drop_noip,
    rx_drop_vlan,
    rx_drop_short,

    rx_drop_policy,
    rx_drop,

    select_to,
    clr_policy_cache,
    cmd_upd_policy,

    cmd_dump_req,
    cmd_trace,
    cmd_illegal,

    STAT_MAX
};

#ifdef VAL_SUBS
struct stat_member sstat[STAT_MAX] = {
    {0, ":rx packets v6\n"},
    {0, ":tx packets v6\n"},
    {0, ":rx packets v4\n"},
    {0, ":tx packets v4\n"},

    {0, ":rx packets multicast\n"},

    {0, ":tx arp reply \n"},
    {0, ":tx neighbor adv\n"},
    {0, ":rx drop (v6 address)\n"},
    {0, ":rx drop (v4 address)\n"},

    {0, ":rx drop (not ip)\n"},
    {0, ":rx drop (vlan)\n"},
    {0, ":rx drop (short)\n"},
    {0, ":rx drop (policy)\n"},

    {0, ":rx drop (other)\n"},
    {0, ":select\n"},
    {0, ":clear policy cache\n"},
    {0, ":command update policy\n"},

    {0, ":command dump req\n"},
    {0, ":command event trace ctrl\n"},
    {0, ":command illegal req\n"}
};

#else
extern struct stat_member sstat[STAT_MAX];
#endif

#define SASAT_STAT(member)  (sstat[member].stat++)

#endif
