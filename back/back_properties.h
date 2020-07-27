/**
 * file  back_properties.h
 * brief プロパティファイル読み出し
 * note  COPYRIGHT FUJITSU LIMITED 2010
 *       FCT) Yagi
 *
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef __ANYC_UTIL_PROPERTIES_H__
#define __ANYC_UTIL_PROPERTIES_H__

#include "prop_common.h"

#define KEY_IFNAME_EGRESS   "eg.ifname"
#define KEY_EGRESS_IP4      "eg.ip4"
#define KEY_EGRESS_IP6      "eg.ip6"
#define KEY_SVR_IP4         "svr.ip4"
#define KEY_SVR_IP6         "svr.ip6"

#ifdef VAL_SUBS
prop_db_t prop_db_backend[] = { 
    {"in.ifname", "eth0"},
    {"eg.ifname", "eth1"},
    {"ud_file",   "/dev/shm/.sasat"},
    {"vip_mode",  "1"},
    {"in.ip4",    "0.0.0.0"},
    {"in.ip6",    "::"},
    {"eg.ip4",    "0.0.0.0"},
    {"eg.ip6",    "::"},
    {"svr.ip4",   "0.0.0,0"},
    {"svr.ip6",   "::"},

    /*==============================================================*
     *    table end.
     *==============================================================*/
    {NULL, NULL}                // table end
};
#else
extern prop_db_t prop_db_backend[];
#endif

#endif /*__ANYC_UTIL_PROPERTIES_H__*/

