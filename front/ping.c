/**
 * file    ping.c
 * brief   ping処理
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

/* include */
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <sys/ioctl.h>

#include "option.h"
#include "util_inline.h"
#include "anycast.h"
#include "checksum.h"

#include "ping_body.c"

/* end */

