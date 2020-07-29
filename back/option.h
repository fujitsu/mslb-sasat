/**
 * file    option.h
 * brief   コンパイルオプション
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         Yagi
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
 
#ifndef __SASAT_OPTION__
#define __SASAT_OPTION__

/* クライアント情報取得 */
#undef NO_CLI_INFO

/* event log取得 */
#undef  NO_EVTLOG

/* イベントログの取得有効/無効 初期値 */
#define EVTLOG_DEFAULT 1

/* サーバからのMAC解決代理応答にL2orL3接続情報を使用する */
#define L_MODE

/* profile */
#define PROFILE_SEC 120

#endif
