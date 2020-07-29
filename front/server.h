/**
 * file    server.h
 * brief   backendトランスレータ管理テーブル
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef __FRONT_SRV_H__
#define __FRONT_SRV_H__

#include <sys/queue.h>
#include <netinet/if_ether.h>

/* サーバテーブルの状態 */
enum {
    SVR_INIT = 0,   /* MACが不明    */
    SVR_OK,         /* MAC解決済み  */
    SVR_DROP,       /* 破棄         */
};

/*
    統計
*/
struct server_stat {
    uint32_t hit;
    uint32_t resolve;   /* MAC解決リトライ */
};

/*
    サーバ（backend)情報テーブル
    各Backend translatorの情報を保持する

*/
typedef struct server_tbl_s
{
    SLIST_ENTRY(server_tbl_s) list;    	/* */
    ushort family;
    uint8_t status;
    uint8_t _rsv1;

    struct sockaddr_storage  svr_ip;    /* server(backend) IP */
    struct sockaddr_storage  gw_ip;     /* gateway IP */

    uint8_t dst_mac[ETH_ALEN];          /* gateway mac */          
#if 0
    uint8_t bytelen;
    uint8_t bitlen;
#else
    uint8_t _rsv2[2];
#endif

    // uint64_t time;
    
    struct server_stat srv_stat;

} server_tbl_t;

/*
    管理テーブル
*/
typedef struct server_manage_s
{
    int server_num;

    /* Backend translator管理テーブルのリスト */
    SLIST_HEAD(, server_tbl_s) head4;
    SLIST_HEAD(, server_tbl_s) head6;

} server_manage_t;


inline void
resolve_target_mac(server_tbl_t *svr);
server_tbl_t *get_svr_table(char *, sa_family_t);
void destroy_svr_tbl(void);
void init_svr_mng_table(void);


#endif /*__FRONT_SRV_H__*/
