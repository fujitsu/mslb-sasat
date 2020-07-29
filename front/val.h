/**
 * file    val.h
 * brief   大域変数
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef __ANC_VAL_H__
#define __ANC_VAL_H__

#include "server.h"
#include "log.h"
#include "anycast.h"

#ifndef VAL_SUBS
#define SLOCAL  extern
#else
#define SLOCAL
#endif

/* global valiables */
SLOCAL struct ifdata *if_ingress;
SLOCAL struct ifdata *if_egress;
SLOCAL int vip_mode;
SLOCAL const uint8_t zerodata[16];  /* all 0 ip */
SLOCAL volatile int patrol_flag;

SLOCAL struct lb_pol_info lb_policy_info;
SLOCAL struct net_thread_info nt_info;
SLOCAL server_manage_t svr_mng_tbl;

SLOCAL struct log_ctl mlog_ctl;
SLOCAL struct mlogdata mlog_data[MAX_MLOG];

SLOCAL struct log_ctl evtlog_ctl;
SLOCAL struct evtlog_data evtlog_data[MAX_ELOG];

SLOCAL char ebuf1[ELOG_DATA_LEN];   /* メインスレッド */
SLOCAL char ebuf2[ELOG_DATA_LEN];   /* 振り分けスレッド */

#undef SLOCAL

#endif
