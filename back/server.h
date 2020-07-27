/**
 * file    server.h 
 * brief   サーバ管理テーブル
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef __BACK_SRV_H__
#define __BACK_SRV_H__

/* サーバ情報テーブルの状態 */
enum {
    SVR_INIT = 0,   /* MACが不明    */
    SVR_OK,         /* MAC解決済み  */
};

/*
    サーバ情報テーブル
    実サーバの情報

*/
typedef struct backend_svr_s
{
    uint8_t stat;
    uint8_t v4_enable;
    uint8_t v6_enable;
    uint8_t _rsv1;

    struct sockaddr_in svr_ip4;
    struct sockaddr_in6 svr_ip6;

    uint8_t svr_mac[ETH_ALEN];
    uint16_t checksum_delta;

    uint32_t    hit4;
    uint32_t    hit6;
} backend_svr_t;

#endif /*__BACK_SRV_H__*/
