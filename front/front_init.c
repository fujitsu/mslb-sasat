/**
 * file    front_init.c
 * brief   初期化処理（front) 
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <linux/sysctl.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include "option.h"
#include "anycast.h"
#include "init.h"
#include "val.h"
#include "util_inline.h"
#include "front_properties.h"

/*　共通ソース */
#include "common_init.c"

/*
    @brief 振り分けスレッド起動
*/
pthread_t
create_net_thread(void*(*func)(void*))
{
    pthread_t tid;
    volatile int wait = 0;

    /* 一本腕専用 */
    /* 通過型の場合は通過型(ルータ/ブリッジ）の処理を起動する */
    if (pthread_create(&tid, NULL, func, (void*)&wait)) {
        strerror_r(errno, ebuf1, ELOG_DATA_LEN);
        syslog(LOG_ERR,
            "thread(ing) create error %s",ebuf1);
        return 0;
    }

    for ( ;; ) {
        if (wait) {
            break;
        }
        anycast_sleep(10);
    }

    if (wait == -1) {
        return 0;
    }

    return tid;
}

/* end */
