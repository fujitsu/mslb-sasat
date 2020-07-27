/**
 * file    ping.c
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
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

/* common以下の共通処理 */
#include "ping_body.c"

/* end */
