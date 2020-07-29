/**
 * file    val.h
 * brief   大域変数
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         Yagi
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef __ANC_VAL_H__
#define __ANC_VAL_H__

#include <time.h>

#include "log.h"
#include "anycast.h"
#include "server.h"
#include "client_tbl.h"

#ifndef VAL_SUBS
#define SLOCAL  extern
#else
#define SLOCAL
#endif

/* global valiables */
SLOCAL backend_svr_t svr_info;

SLOCAL struct ifdata *if_ingress;
SLOCAL struct ifdata *if_egress;
SLOCAL int vip_mode;
#ifdef L_MODE
SLOCAL int l_mode;
#endif
SLOCAL const uint8_t zerodata[16];  /* all 0 ip */

SLOCAL client_info_t client_info;

SLOCAL struct log_ctl mlog_ctl;
SLOCAL struct mlogdata mlog_data[MAX_MLOG];

SLOCAL struct log_ctl evtlog_ctl;
SLOCAL struct evtlog_data evtlog_data[MAX_ELOG];

SLOCAL char ebuf1[ELOG_DATA_LEN];   /* メインスレッド */
SLOCAL char ebuf2[ELOG_DATA_LEN];   /* 振り分けスレッド */

SLOCAL uchar gw_mac_v4_valid;
SLOCAL uchar gw_mac_v6_valid;
SLOCAL uchar gw_mac_v4[ETH_ALEN] __attribute__((aligned(4)));
SLOCAL uchar gw_mac_v6[ETH_ALEN] __attribute__((aligned(4)));

#undef EXTERM

#endif
