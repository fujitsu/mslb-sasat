/**
 * file    common_init.c
 * brief   共通初期化処理
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

#include <stdlib.h> 
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/ioctl.h> 
#include <sys/un.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <linux/sysctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util_inline.h"

static struct ifaddrs *ifap0 = NULL;

static void set_max_buffer(void);
static int cre_ud_socket(const char *file_name);
static int get_ifaddr_info(struct ifdata *ifdata);
static void free_ifaddr_info(void);

/*
    @brief インターフェース情報をプロパティファイルから取得
*/
int
get_interface_info(struct ifdata *if_in, struct ifdata *if_eg)
{
    const char *str;

    str = anycast_get_properties(KEY_IFNAME_INGRESS);
    if (str == NULL) {
        evtlog("gft0", 0, 0, NULL);
        str = IFNAME_INGRESS_DEFAULT;
    }

    strncpy(if_in->ifname, str, IFNAMSIZ);

    get_ifaddr_info(if_in);

#ifdef BACKEND_T
    str = anycast_get_properties(KEY_IFNAME_EGRESS);
    if (str == NULL) {
        evtlog("gft1", 0, 0, NULL);
        str = IFNAME_EGRESS_DEFAULT;
    }

    strncpy(if_eg->ifname, str, IFNAMSIZ);
    get_ifaddr_info(if_eg);
#endif

    free_ifaddr_info();

    vip_mode = anycast_get_properties_int(KEY_VIP_MODE);

    if (vip_mode == 1) {
        /* 仮想IPモード */
        if (if_in->v4_enable) {
            if (((str = anycast_get_properties(KEY_VIP4)) == NULL) ||
                    (inet_pton(AF_INET, str, &if_in->vip4) != 1)) {
                mlog("VIP(v4 ingress) not set or invalid");
                if_in->v4_enable = 0;
            }
        }
#ifdef BACKEND_T
        if (if_eg->v4_enable) {
            if (((str = anycast_get_properties(KEY_EGRESS_IP4)) == NULL) ||
                    (inet_pton(AF_INET, str, &if_eg->vip4) != 1)) {
                mlog("VIP(v4 egress) not set or invalid");
                if_eg->v4_enable = 0;
            }
        }
#endif
        if (if_in->v6_enable) {
            if (((str = anycast_get_properties(KEY_VIP6)) == NULL) ||
                    (inet_pton(AF_INET6, str, &if_in->vip6) != 1)) {
                mlog("VIP(v6 ingress) not set or invalid");
                if_in->v6_enable = 0;
            }
        }
#ifdef BACKEND_T
        if (if_eg->v6_enable) {
            if (((str = anycast_get_properties(KEY_EGRESS_IP6)) == NULL) ||
                    (inet_pton(AF_INET6, str, &if_eg->vip6) != 1)) {
                mlog("VIP(v6 egress) not set or invalid");
                if_eg->v6_enable = 0;
            }
        }
#endif
    } else {
        vip_mode = 0;
        /* 実IPモード */
        if_in->vip4 = if_in->sip4;
        if_in->vip6 = if_in->sip6;
#ifdef BACKEND_T
        if_eg->vip4 = if_eg->sip4;
        if_eg->vip6 = if_eg->sip6;
#endif
    }
    mlog("vip mode = %d", vip_mode);

    memset(if_in->fmmac, 0, ETH_ALEN);
    if (if_in->v6_enable) {
        if_in->fmmac[0] = 0x33;
        if_in->fmmac[1] = 0x33;
        if_in->fmmac[2] = 0xff;
        if_in->fmmac[3] = if_in->vip6.s6_addr[13];
        if_in->fmmac[4] = if_in->vip6.s6_addr[14];
        if_in->fmmac[5] = if_in->vip6.s6_addr[15];
    }
#ifdef BACKEND_T
    memset(if_eg->fmmac, 0, ETH_ALEN);
    if (if_eg->v6_enable) {
        if_eg->fmmac[0] = 0x33;
        if_eg->fmmac[1] = 0x33;
        if_eg->fmmac[2] = 0xff;
        if_eg->fmmac[3] = if_eg->vip6.s6_addr[13];
        if_eg->fmmac[4] = if_eg->vip6.s6_addr[14];
        if_eg->fmmac[5] = if_eg->vip6.s6_addr[15];
    }
#endif

#ifdef FRONT_T
    return TYPE_ONE_ARM;
#else
    return TYPE_TWO_ARM;
#endif
}
/*
    @brief 指定インターフェースのIPアドレス情報を取得
*/
static int
get_ifaddr_info(struct ifdata *ifdata)
{
    struct ifaddrs *ifap;
    char *ifname = ifdata->ifname;
    int fd, set4, set6, llv;
    struct in6_addr ll;
    static const char *enbl[] =  {"not available", "available"};

    if (ifap0 == NULL) {
        if (getifaddrs(&ifap0)) {
            char buff[32];
            ifap0 = NULL;
            mlog("getifaddrs(%s)", 0, 0,
                strerror_r(errno, buff, sizeof(buff)));
            goto mymac;
        }
    }

    set4 = set6 = llv = 0;
    for (ifap = ifap0; ifap; ifap=ifap->ifa_next) {
        struct sockaddr *sa;

        if ((set4 == 1) && (set6 == 1)) {
            break;
        }

        if ((sa = ifap->ifa_addr) == NULL) {
            continue;
        }

        if (strcmp(ifap->ifa_name, ifname) == 0) {
            switch (sa->sa_family) {
            case AF_INET:
                if (!set4) {
                    ifdata->sip4 = ((struct sockaddr_in *)sa)->sin_addr;
                    ifdata->v4_enable = 1;
                    set4 = 1;
                }
                break;
            case AF_INET6:
                if (!set6) {
                    struct in6_addr tmp;
                    tmp = ((struct sockaddr_in6 *)sa)->sin6_addr;
                    if (tmp.s6_addr[0] == 0xfe) {
                        if (llv == 0) {
                            /* link localの優先度は低い */
                            llv = 1;
                            ll = tmp;
                        }
                    } else {
                        ifdata->sip6 = tmp;
                        ifdata->v6_enable = 1;
                        set6 = 1;
                    }
                }
                break;
            default:
                break;
            }
        }
    }
    if ((set6 == 0) && (llv == 1)) {
        /* globalアドレスが無い場合、link localを使用 */
        ifdata->sip6 = ll;
        ifdata->v6_enable = 1;
    }

mymac:
    memset(ifdata->mac, 0, ETH_ALEN);
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd != -1) {
        struct ifreq ifr;

        ifr.ifr_addr.sa_family = AF_INET;
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);

        ioctl(fd, SIOCGIFHWADDR, &ifr);
        memcpy(ifdata->mac, &ifr.ifr_hwaddr.sa_data[0], ETH_ALEN);
        close(fd);
    }

    mlog("interface(%s) v4 %s, v6 %s", ifdata->ifname,
            enbl[ifdata->v4_enable], enbl[ifdata->v6_enable]);

    return 0;
}

static void free_ifaddr_info(void)
{
    if (ifap0) {
        freeifaddrs(ifap0);
        ifap0 = NULL;
    }
}

/*
    @brief 受信ソケット初期化
*/
#define MAX_BUFF_SZ 512
#ifdef FRONT_T
int
init_socket_if(struct ifdata *ifdata, int * fd)
{
#else
int
init_socket_if(struct ifdata *ifdata, int egress, int *fd)
{
#endif
    struct ifreq if_req;
    struct sockaddr_ll sa;
    char *device = ifdata->ifname;
    int soc, err = 0;
    int opt;
    char buf[32];

    /* ソケットの生成 */
    if ((soc = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
        syslog(LOG_ERR, "socket create error(%s)",
            strerror_r(errno, buf, sizeof(buf)));
        return (NET_REQ_SOCKET_ERR);
    }

    set_max_buffer();

    /* 受信バッファサイズの設定 */
    opt = MAX_BUFF_SZ * 1024;
    setsockopt(soc, SOL_SOCKET, SO_RCVBUF,
            (char *) &opt, sizeof(opt));

    /* 受信バッファサイズの設定 */
    setsockopt(soc, SOL_SOCKET, SO_SNDBUF,
            (char *) &opt, sizeof(opt));

    /* インターフェース情報の取得 */
    strncpy(if_req.ifr_name, device, sizeof(if_req.ifr_name)-1);
    if (ioctl(soc, SIOCGIFINDEX, &if_req) < 0) {
        err = NET_REQ_GIFINDEX_ERR;
        syslog(LOG_ERR, "socket error SIOCGIFINDEX(%s)",
            strerror_r(errno, buf, sizeof(buf)));
        goto init_sock_end;
    }

    /* インターフェースをbind */
    sa.sll_family = PF_PACKET;
    sa.sll_protocol = htons(ETH_P_ALL);
    sa.sll_ifindex = if_req.ifr_ifindex;
    if (bind(soc, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
        err = NET_REQ_BIND_ERR;
        syslog(LOG_ERR, "socket bind error(%s)",
            strerror_r(errno, buf, sizeof(buf)));
        goto init_sock_end;
    }

    /* インターフェースのフラグ取得 */
    strncpy(if_req.ifr_name, device, sizeof(if_req.ifr_name)-1);
    if (ioctl(soc, SIOCGIFFLAGS, &if_req) < 0) {
        err = NET_REQ_GIFFLAGS_ERR;
        syslog(LOG_ERR, "socket error SIOCGIFFLAGS(%s)",
            strerror_r(errno, buf, sizeof(buf)) );
        goto init_sock_end;
    }
    /* インターフェースのフラグにORする */
#ifdef FRONT_T
    if (ifdata->v6_enable) {
        if_req.ifr_flags |= (IFF_UP | IFF_ALLMULTI );
    } else {
        if_req.ifr_flags |= (IFF_UP);
    }
#else
    if (egress) {
         if_req.ifr_flags |= (IFF_UP | IFF_PROMISC );
    } else {
        if (ifdata->v6_enable) {
             if_req.ifr_flags |= (IFF_UP | IFF_ALLMULTI );
        } else {
            if_req.ifr_flags |= (IFF_UP);
        }
    }
#endif
    if (ioctl(soc, SIOCSIFFLAGS, &if_req) < 0) {
        err = NET_REQ_SIFFLAGS_ERR;
        syslog(LOG_ERR, "socket error SIOCGIFFLAGS(%s)", strerror_r(errno, buf, sizeof(buf)));
        goto init_sock_end;
    }
/*
    if (ifdata->v6_enable) {
        // multicast address(Neighbor Solicite)設定
        struct packet_mreq mreq;
        mreq.mr_ifindex = index;
        mreq.mr_type = PACKET_MR_MULTICAST;
        mreq.mr_alen = ETH_ALEN;
        memcpy(mreq.mr_address, ifdata->fmmac, ETH_ALEN);
        if (setsockopt(soc, SOL_SOCKET,  PACKET_ADD_MEMBERSHIP,
            &mreq, sizeof(mreq)) < 0) {
            err = NET_REQ_ADD_MEMBERSHIP_ERR;
            syslog(LOG_ERR, "PACKET_ADD_MEMBERSHIP (%s)",
                strerror_r(errno, buf, sizeof(buf)));
            goto init_sock_end;
        }
    }
*/

    /* non-blockingをセット */
    int val = fcntl(soc, F_GETFL, 0);
    if (fcntl(soc, F_SETFL, val | O_NONBLOCK) < 0) {
        err = NET_REQ_FCNTL_ERR;
        syslog(LOG_ERR, "socket error F_SETFL(%s)",
            strerror_r(errno, buf, sizeof(buf))); 
    }

init_sock_end:

    if (err) {
        close(soc);
        return err;
    }

    *fd = soc;
    return (0);
}

/*
    送受信ﾊﾞｯﾌｧのシステム設定
*/
static void set_max_buffer(void)
{
    struct __sysctl_args args;
    int val;
    int name1[] = {CTL_NET, NET_CORE, NET_CORE_RMEM_MAX};
    int name2[] = {CTL_NET, NET_CORE, NET_CORE_WMEM_MAX};
    char buf[32];

    /* 受信バッファサイズ最大値の変更 */
    val = MAX_BUFF_SZ * 1024;
    if (val) {
        memset(&args, 0, sizeof(struct __sysctl_args));
        args.name = name1;
        args.nlen = sizeof(name1)/sizeof(name1[0]);
        args.newval = &val;
        args.newlen = sizeof(val);
        if(syscall(SYS__sysctl, &args) == -1) {
            mlog("SYSCall (%s)", strerror_r(errno, buf, sizeof(buf)));
        }
    }

    /* 送信バッファサイズ最大値の変更 */
    val = MAX_BUFF_SZ * 1024;
    if (val) {
        memset(&args, 0, sizeof(struct __sysctl_args));
        args.name = name2;
        args.nlen = sizeof(name2)/sizeof(name2[0]);
        args.newval = &val;
        args.newlen = sizeof(val);
        if(syscall(SYS__sysctl, &args) == -1) {
            mlog("SYSCall (%s)", strerror_r(errno, buf, sizeof(buf)));
        }
    }
}

/*
    @brief unix domain file
*/
int init_ud_socket(void)
{
    const char *file_name = anycast_get_properties(KEY_UD_FILE);

    if (file_name == NULL) {
        mlog("unix domain file name(%s)", UD_FILE_NAME);
        file_name = UD_FILE_NAME;
    }
    return cre_ud_socket(file_name);
}

/*
    @unix domain socketを作成
*/
static int
cre_ud_socket(const char *file_name)
{
    struct sockaddr_un servaddr;
    int soc, val;
    mode_t omask;
    char buf[32];

    if ((soc = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        syslog(LOG_ERR, "ud socket error(%s)",
            strerror_r(errno, buf, sizeof(buf)));
        return (-1);
    }

    unlink(file_name);
    /* 外部からのアクセスを受ける */
    omask = umask(0000);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strncpy(servaddr.sun_path, file_name, sizeof(servaddr.sun_path)-1);
    if (bind(soc, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        syslog(LOG_ERR, "ud bind error(%s:%s)", file_name,
            strerror_r(errno, buf, sizeof(buf)));
        close(soc);
        umask(omask);
        return (-1);
    }

    umask(omask);

    /* non-blockingをセット */
    val = fcntl(soc, F_GETFL, 0);
    if (fcntl(soc, F_SETFL, val | O_NONBLOCK) < 0) {
        syslog(LOG_ERR, "ud fcnt error(%s)",
            strerror_r(errno, buf, sizeof(buf)));
        close(soc);
        return (-1);
    }
    return soc;
}


/*
    @brief tscサイクルの取得 ia32専用

*/
uint64_t tsc_cycle_init(void)
{
    double clk = 1662.5f;
    char buf[512];
    const char key[] = "cpu MHz";
    FILE *fp;
    char *p;

    fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        for (; fgets(buf, 512, fp); ) {
            if (strncmp(key, buf, strlen(key)) == 0) {
                p = strstr(buf, ":");
                p += 2;
                clk = strtod(p, NULL);
                break;
            }
        }
        fclose(fp);
    } else {
        char buf[32];
        mlog("cpuinfo open error %s", strerror_r(errno, buf, sizeof(buf)));
    }
    return (uint64_t)clk;
}

/*
    @brief 全シグナルをブロック
*/
void signal_block(void)
{
    sigset_t sigmask;

    sigfillset(&sigmask);
    pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
}

/*
    @brief pidファイル処理 2重起動防止も行う
*/
int init_pid(void)
{
    FILE *pid_file;

    pid_file = fopen(PID_PATH, "r+");
    if ((pid_file == NULL) && (errno == ENOENT)) {
        /* ファイルが存在しない */
        pid_file = fopen (PID_PATH, "w");
    } else {
        /* ファイルが存在する */
        char buff[32];
        char fname[64];
        DIR *dir;
        char *end;

        fgets(buff, 32, pid_file);
        end = strchr(buff, '\n');
        if (end) {
            *end = '\0';
        }
        sprintf(fname, "/proc/%s", buff);
        dir = opendir(fname);
        if (dir != NULL) {
            /* 2重起動 */
            fprintf(stderr, "SASAT translator already running\n");
            fclose(pid_file);
            closedir(dir);
            return -1;
        }
        truncate(PID_PATH, 0);
        rewind(pid_file);
    }
    fprintf (pid_file, "%d\n", (int) getpid());
    fclose (pid_file);
    return 0;
}

/* end */
