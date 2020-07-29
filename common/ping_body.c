/**
 * file    ping_body.c
 * brief   ping送信
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

#include "util_inline.h"
#include "anycast.h"
#include "checksum.h"

/* prototype */
void send_ping(struct sockaddr *target, struct ifdata *ifdata);

static void 
ping4(struct sockaddr_in *, struct in_addr *, char *, int *);
static void 
ping6(struct sockaddr_in6 *, struct in6_addr *, char *, int *);
static u_short get_chksum(uchar *dataaddr, uint datalen);
static int init_ping_soc4(int *fd, struct in_addr *src, char *device);
static int init_ping_soc6(int *fd, struct in6_addr *src, char *device);

/*
    ping送信
    @param target target address
    @param src src address
    @param ifdata interface data
*/
void send_ping(struct sockaddr *target, struct ifdata *ifdata)
{
    if (target->sa_family == AF_INET) {
        ping4((struct sockaddr_in *)target,
            &ifdata->sip4, ifdata->ifname, &ifdata->ping4_fd);
    } else {
        ping6((struct sockaddr_in6 *)target, 
            &ifdata->sip6, ifdata->ifname, &ifdata->ping6_fd);
    }
}

/*
    @brief IPv4 icmp echo request
    @param target
    @param src
    @param ifname
*/
static void 
ping4(struct sockaddr_in *target, struct in_addr *src, char *ifname, int *fd)
{
    struct icmp *icmp_p;
    struct sockaddr_in sin;
    uchar send_data[128];  /* 送信データ格納領域 */
    int i, err;
    static unsigned char seq_num = 1;

    if (!*fd) {
        if ((err = init_ping_soc4(fd, src, ifname)) != 0) {
            mlog("ping socket error %x (%s)", err,
                strerror_r(errno, ebuf1, ELOG_DATA_LEN));
            return;
        }
    }

    memset(send_data, 0, sizeof(send_data));

    /* ICMPヘッダ作成 */
    icmp_p = (struct icmp *)send_data;
    icmp_p->icmp_type = ICMP_ECHO;
    icmp_p->icmp_code = 0;
    icmp_p->icmp_id = 0;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = target->sin_addr.s_addr;

    for (i = 0; i < 1; i++, seq_num++) {
        icmp_p->icmp_cksum = 0;   
        icmp_p->icmp_seq = htons(seq_num);
        icmp_p->icmp_cksum = get_chksum(send_data, sizeof(struct icmp));

        if (sendto(*fd, send_data,
                ICMP_MINLEN, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
             mlog("ping send error (%s)",
                strerror_r(errno, ebuf1, ELOG_DATA_LEN));
        }
    }
}

/*
    @brief IPv6 icmp echo request
    @param target
    @param src
    @param ifname
*/
static void 
ping6(struct sockaddr_in6 *target, struct in6_addr *src, char *device, int *fd)
{
    struct icmp6_hdr *icmp6;
    struct sockaddr_in6 sin;
    char send_data[128];  /* 送信データ格納領域 */
    int i, err;
    static unsigned char seq_num = 1;

    if (!*fd) {
        if ((err = init_ping_soc6(fd, src, device)) != 0) {
            mlog("ping socket error %x (%s)", err,
                strerror_r(errno, ebuf1, ELOG_DATA_LEN));
            return;
        }
    }

    memset(send_data, 0, sizeof(send_data));

    /* ICMPヘッダ作成 */
    icmp6 = (struct icmp6_hdr *)send_data;
    icmp6->icmp6_type = ICMP6_ECHO_REQUEST;
    icmp6->icmp6_code = 0;
    icmp6->icmp6_id = 0;

    memset(&sin, 0, sizeof(sin));
    sin.sin6_family = AF_INET6;
    memcpy(&sin.sin6_addr, &target->sin6_addr, sizeof(sin.sin6_addr));

    for (i = 0; i < 1; i++, seq_num++) {

        icmp6->icmp6_seq = htons(seq_num);

        if (sendto(*fd, send_data,
                64, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
            mlog("ping6 send error (%s)",
                strerror_r(errno, ebuf1, ELOG_DATA_LEN));
        }
    }
}

static u_short
get_chksum(uchar *dataaddr, uint datalen)
{
    uint32_t sum;
    sum = soft_checksum(dataaddr, datalen);
    sum = (sum & 0x0000ffff) + (sum >> 16);
    if (sum > 0x0000ffff) {
        sum -= 0x0000ffff;
    }

    return ~(u_short)(sum & 0x0000ffff);
}

/*
    @brief pingソケット作成2
*/
static int
init_ping_soc4(int *fd, struct in_addr *src, char *device)
{
    int soc;
    struct ifreq if_req;
    struct sockaddr_in sa;

    /* ソケットの生成 */
    if ((soc = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        return (NET_REQ_SOCKET_ERR);
    }

    /* 入出力インターフェースを固定する（loは使えなくなる) */
    if (setsockopt(soc, SOL_SOCKET, SO_BINDTODEVICE,
            device, strlen(device)+1) == -1) {
        close(soc);
        return (NET_REQ_SO_BINDTODEVICE_ERR);
    }

    sa.sin_family = AF_INET;
    memcpy(&sa.sin_addr, src, sizeof(sa.sin_addr));
    if (bind(soc, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) < 0) {
        close(soc);
        return (NET_REQ_BIND_ERR);
    }

    /* インターフェースのフラグ取得 */
    strncpy(if_req.ifr_name, device, sizeof(if_req.ifr_name)-1);
    if (ioctl(soc, SIOCGIFFLAGS, &if_req) < 0) {
        close(soc);
        return NET_REQ_GIFFLAGS_ERR;
    }

    /* インターフェースのフラグにORする */
    if_req.ifr_flags |= IFF_UP;
    if (ioctl(soc, SIOCSIFFLAGS, &if_req) < 0) {
        close(soc);
        return NET_REQ_SIFFLAGS_ERR;
    }

    /* non-blockingをセット */
    int val = fcntl(soc, F_GETFL, 0);
    if (fcntl(soc, F_SETFL, val | O_NONBLOCK) < 0) {
        close(soc);
        return NET_REQ_FCNTL_ERR;
    }

    *fd = soc;
    return 0;
}

/*
    @brief ping6ソケット作成
*/
static int
init_ping_soc6(int *fd, struct in6_addr *src, char *device)
{
    int soc;
    struct ifreq if_req;
    struct sockaddr_in6 sa;

    /* ソケットの生成 */
    if ((soc = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) {
        return (NET_REQ_SOCKET_ERR);
    }

    /* 入出力インターフェースを固定する（loは使えなくなる) */
    if (setsockopt(soc, SOL_SOCKET, SO_BINDTODEVICE,
            device, strlen(device)+1) < 0) {
        close(soc);
        return (NET_REQ_SO_BINDTODEVICE_ERR);
    }

/*
    int offset = 2;
    if (setsockopt(soc, IPPROTO_IPV6, IPV6_CHECKSUM,
            &offset, sizeof(offset)) < 0) {
        close(soc);
        return (NET_REQ_SO_BINDTODEVICE_ERR);
    }
*/
    sa.sin6_family = AF_INET6;
    memcpy(&sa.sin6_addr, src, sizeof(sa.sin6_addr));
    if (bind(soc, (struct sockaddr *)&sa, sizeof(struct sockaddr_in6)) < 0) {
        close(soc);
        return (NET_REQ_BIND_ERR);
    }

    /* インターフェースのフラグ取得 */
    strncpy(if_req.ifr_name, device, sizeof(if_req.ifr_name)-1);
    if (ioctl(soc, SIOCGIFFLAGS, &if_req) < 0) {
        close(soc);
        return NET_REQ_GIFFLAGS_ERR;
    }

    /* インターフェースのフラグにORする */
    if_req.ifr_flags |= IFF_UP;
    if (ioctl(soc, SIOCSIFFLAGS, &if_req) < 0) {
        close(soc);
        return NET_REQ_SIFFLAGS_ERR;
    }

    /* non-blockingをセット */
    int val = fcntl(soc, F_GETFL, 0);
    if (fcntl(soc, F_SETFL, val | O_NONBLOCK) < 0) {
        close(soc);
        return NET_REQ_FCNTL_ERR;
    }

    *fd = soc;
    return 0;
}
/* end */

