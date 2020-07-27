/**
 * file    route.c
 * brief   ルーティングソケットにより、nexthop IP, MACを検索する
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/rtnetlink.h>

#include "option.h"
#include "log.h"
#include "val.h"

/* common以下の共通処理 */
#include "rt_body.c"

/* end */
