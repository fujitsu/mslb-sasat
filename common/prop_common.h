/**
 * file    prop_common.h
 * brief   プロパティ処理(共通処理)
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#include <sys/queue.h>

void anycast_prop_init(void);
const char *anycast_get_properties(const char *key);
long int anycast_get_properties_int(const char *key);
long int anycast_get_properties_hex(const char *key);

#ifndef __PROP_COMMON__
#define __PROP_COMMON__

struct propertie_data {
    SLIST_ENTRY(propertie_data) list;
    unsigned char data[0];
};

// 内部でデータを保持するためのデータ構造
typedef struct {
    const char *key;
    const char *value;
} prop_db_t;


#define PROP_DIRNAME    "/var/opt/sasat/etc"
#define PROP_FILENAME   "sasat.conf"

/*
    動作設定ファイルのkey
*/
#define KEY_IFNAME_INGRESS  "in.ifname"
#define KEY_UD_FILE         "ud_file"
#define KEY_VIP_MODE        "vip_mode"
#define KEY_VIP4            "in.ip4"
#define KEY_VIP6            "in.ip6"

#define IFNAME_INGRESS_DEFAULT "eth0"
#define IFNAME_EGRESS_DEFAULT  "eth1"
#define UD_FILE_NAME "/dev/shm/.sasat"
#endif
