/**
 * file    rt_body.c 
 * brief   ルーティングソケットにより、nexthop IP, MACを検索する処理
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 *
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

#include "log.h"
#include "val.h"
#include "util_inline.h"

/* struct */
typedef struct
{
    __u8 family;
    __u8 bytelen;
    __s16 bitlen;
    __u32 flags;
    __u32 data[4];
} inet_prefix;

/* MAC取得用フィルタ */
struct filter
{
    int family;
    int state;
    int index;
    inet_prefix pfx;
};

struct rtsock_handle
{
    int fd;
    struct sockaddr_nl  local;
    struct sockaddr_nl  peer;
    __u32   seq;
    __u32   dump;
};

/* prototype */
#define NLMSG_TAIL(nmsg) \
    ((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

typedef int (*rtsock_filter_t)(struct nlmsghdr *n, void*, void*);
static int getattr(struct nlmsghdr *n, int maxlen, int type, const void *data,int alen);
static int wdump_request(struct rtsock_handle *rth, int family, int type);
static int dump_filter(struct rtsock_handle *rth,
             rtsock_filter_t filter, void *, void *);
static int get_neigh(struct nlmsghdr *n, void *, void*);
#ifdef L_MODE
static int get_masklen(struct nlmsghdr *n, void *, void*);
#endif
static int rtsock_open(struct rtsock_handle *rth);
static void rtsock_close(struct rtsock_handle *rth);
static int rtsock_talk(struct rtsock_handle *rtsock, struct nlmsghdr *n); 
static int get_route(struct nlmsghdr *n, inet_prefix *dst, inet_prefix *via);

/**
 * @brief 宛先アドレスからnext hopIPとそのMACアドレスを求める処理
 *
 * @param   taget_addr 宛先MAC
 * @param   ifname インターフェース（NULLのとき指定なし）
 * @return  nexthop nexthop ip（ルータまたは宛先IP)
 * @return  dstmac nexthopのMAC  ALL0のとき、MAC解決が必要
 */
int get_target_mac(struct sockaddr *target_addr,
                   char *ifname,
                   struct sockaddr *nexthop,
                   unsigned char *dstmac,
                   int gw)
{
    struct {
        struct nlmsghdr n;
        struct rtmsg    r;
        char            buf[1024];
    } req;
    inet_prefix addr;
    inet_prefix dst;
    inet_prefix via;
    sa_family_t family;
    int bytelen, bitlen;
    unsigned char *taddr, *naddr;
    struct rtsock_handle rth;
    struct filter filter;

    copy_mac(dstmac, zerodata);

    if (rtsock_open(&rth) < 0) {
        return -1;
    }

    family = target_addr->sa_family;
    if (family == AF_INET) {
        bytelen = 4;
        bitlen  = 32;
        taddr = (unsigned char*)&((struct sockaddr_in *)target_addr)->sin_addr;
        naddr = (unsigned char*)&((struct sockaddr_in *)nexthop)->sin_addr;
    } else if (family == AF_INET6) {
        bytelen = 16;
        bitlen  = 128;
        taddr = (unsigned char*)&((struct sockaddr_in6 *)target_addr)->sin6_addr;
        naddr = (unsigned char*)&((struct sockaddr_in6 *)nexthop)->sin6_addr;
    } else {
        rtsock_close(&rth);
        return -1;
    }

    memset(&req, 0, sizeof(req));
    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    req.n.nlmsg_flags = NLM_F_REQUEST;
    req.n.nlmsg_type = RTM_GETROUTE;
    req.r.rtm_family = family;

    memcpy(addr.data, taddr, bytelen);
    addr.family = family;
    if (gw == 1) {
        /* default gw検索 */
        addr.bytelen = 0;
    } else {
        addr.bytelen = bytelen;
    }
    addr.bitlen = -1;
    addr.flags = 0;

    getattr(&req.n, sizeof(req), RTA_DST, &addr.data, addr.bytelen);
    req.r.rtm_dst_len = addr.bitlen;

    if (rtsock_talk(&rth, &req.n) < 0) {
        rtsock_close(&rth);
        return -1;
    }

    /* next hopの取得 */
    if (get_route(&req.n, &dst, &via) < 0) {
        rtsock_close(&rth);
        return -1;
    }

    memset(&filter, 0, sizeof(filter));
    filter.state = 0xFF & ~NUD_NOARP;

    if (memcmp(via.data, zerodata, sizeof(via.data)) == 0) {
        if (gw) {
            rtsock_close(&rth);
            return -1;
        }
        memcpy(via.data, dst.data, sizeof(dst.data));
    }

    /* next hopを設定 */
    memcpy(naddr, via.data, bytelen);

    if (ifname) {
        if ( (filter.index = if_nametoindex(ifname) ) == 0) {
            mlog( "if_nametoindex error(%s)", strerror_r(errno, ebuf2, sizeof(ELOG_DATA_LEN)));
            rtsock_close(&rth);
            return -1;
        }
    }

    memcpy(&filter.pfx, &via, sizeof(via));
    filter.pfx.family = filter.family = family;
    filter.pfx.bitlen = bitlen;
    filter.pfx.bytelen = bytelen;

    if (wdump_request(&rth, family, RTM_GETNEIGH) < 0) {
        mlog("Cannot send dump request");
        rtsock_close(&rth);
        return -1;
    }

    /* MAC取得 */
    if (dump_filter(&rth, get_neigh, dstmac, &filter) < 0) {
        rtsock_close(&rth);
        return -1;
    }

    rtsock_close(&rth);
    return 0;
}

#ifdef L_MODE 
/*
    @brief mask長を取得する
*/
int get_mask(struct sockaddr *target_addr)
{
    struct rtsock_handle rth;
    struct filter filter;
    int mask = 0;

    if (rtsock_open(&rth) < 0) {
        return -1;
    }

    if (target_addr->sa_family == AF_INET) {
        memcpy(filter.pfx.data,
            (unsigned char*)&((struct sockaddr_in *)target_addr)->sin_addr, 4);
        filter.pfx.family = filter.family = AF_INET;
        filter.pfx.bitlen = 32;
        filter.pfx.bytelen = 4;
    } else if (target_addr->sa_family == AF_INET6) {
        memcpy(filter.pfx.data,
            (unsigned char*)&((struct sockaddr_in6 *)target_addr)->sin6_addr, 16);
        filter.pfx.family = filter.family = AF_INET6;
        filter.pfx.bitlen = 128;
        filter.pfx.bytelen = 16;
    } else {
        rtsock_close(&rth);
        return -1;
    }

    if (wdump_request(&rth, filter.family, RTM_GETADDR) < 0) {
        mlog("Cannot send dump request");
        rtsock_close(&rth);
        return -1;
    }

    /* mask長取得 */
    if (dump_filter(&rth, get_masklen, &mask, &filter) < 0) {
        rtsock_close(&rth);
        return -1;
    }
    return mask;
}
#endif

/**
 * @brief ルーティングソケットオープン
 */
static int rtsock_open(struct rtsock_handle *rth)
{
    socklen_t addr_len;
    int sndbuf = 32768;
    int rcvbuf = 32768;

    memset(rth, 0, sizeof(rth));

    rth->fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (rth->fd < 0) {
        mlog("Cannot open netlink socket(%s)", strerror_r(errno, ebuf2, ELOG_DATA_LEN));
        return -1;
    }

    if (setsockopt(rth->fd,SOL_SOCKET,SO_SNDBUF,&sndbuf,sizeof(sndbuf)) < 0) {
        mlog("SO_SNDBUF (%s)", strerror_r(errno, ebuf2, ELOG_DATA_LEN));
        return -1;
    }

    if (setsockopt(rth->fd,SOL_SOCKET,SO_RCVBUF,&rcvbuf,sizeof(rcvbuf)) < 0) {
        mlog("SO_RCVBUF (%s)", strerror_r(errno, ebuf2, ELOG_DATA_LEN));
        return -1;
    }

    memset(&rth->local, 0, sizeof(rth->local));
    rth->local.nl_family = AF_NETLINK;
    rth->local.nl_groups = 0;

    if (bind(rth->fd, (struct sockaddr*)&rth->local, sizeof(rth->local)) < 0) {
        mlog("Cannot bind netlink socket (%s)", strerror_r(errno, ebuf2, ELOG_DATA_LEN));
        return -1;
    }
    addr_len = sizeof(rth->local);
    if (getsockname(rth->fd, (struct sockaddr*)&rth->local, &addr_len) < 0) {
        mlog("Cannot getsockname (%s)", strerror_r(errno, ebuf2, ELOG_DATA_LEN));
        return -1;
    }
    if (addr_len != sizeof(rth->local)) {
        mlog("Wrong address length %d", addr_len);
        return -1;
    }
    if (rth->local.nl_family != AF_NETLINK) {
        mlog("Wrong address family %d", rth->local.nl_family);
        return -1;
    }
    rth->seq = time(NULL);
    return 0;
}

/**
 * @brief ルーティングソケットクローズ
 */
static void rtsock_close(struct rtsock_handle *rth)
{
    close(rth->fd);
}

static int getattr(struct nlmsghdr *n, int maxlen, int type, const void *data,
          int alen)
{
    int len = RTA_LENGTH(alen);
    struct rtattr *rta;

    if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > maxlen) {
        return -1;
    }
    rta = NLMSG_TAIL(n);
    rta->rta_type = type;
    rta->rta_len = len;
    memcpy(RTA_DATA(rta), data, alen);
    n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);
    return 0;
}

static int wdump_request(struct rtsock_handle *rth, int family, int type)
{
    struct {
        struct nlmsghdr nlh;
        struct rtgenmsg g;
    } req;
    struct sockaddr_nl nladdr;

    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;

    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = sizeof(req);
    req.nlh.nlmsg_type = type;
    req.nlh.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
    req.nlh.nlmsg_pid = 0;
    req.nlh.nlmsg_seq = rth->dump = ++rth->seq;
    req.g.rtgen_family = family;

    return sendto(rth->fd, (void*)&req, sizeof(req), 0,
              (struct sockaddr*)&nladdr, sizeof(nladdr));
}

static int dump_filter(struct rtsock_handle *rth,
             rtsock_filter_t filter, void *arg1, void *arg2)
{
    struct sockaddr_nl nladdr;
    struct iovec iov;
    struct msghdr msg = {
        .msg_name = &nladdr,
        .msg_namelen = sizeof(nladdr),
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };
    char buf[16384];

    iov.iov_base = buf;
    while (1) {
        int status;
        struct nlmsghdr *h;

        iov.iov_len = sizeof(buf);
        status = recvmsg(rth->fd, &msg, 0);

        if (status < 0) {
            if (errno == EINTR)
                continue;
            evtlog("dfl0", errno, 0, NULL);
            continue;
        }

        if (!status) {
            return -1;
        }

        h = (struct nlmsghdr*)buf;
        while (NLMSG_OK(h, status)) {
            int err;

            if (nladdr.nl_pid != 0 ||
                h->nlmsg_pid != rth->local.nl_pid ||
                h->nlmsg_seq != rth->dump) {
                goto skip;
            }

            if (h->nlmsg_type == NLMSG_DONE)
                return 0;
            if (h->nlmsg_type == NLMSG_ERROR) {
                struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(h);
                if (h->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
                    evtlog("dfl1", 0,0, (uchar*)"ERROR truncated");
                } else {
                    errno = -err->error;
                    evtlog("dfl2", errno, 0, (uchar*)"RTNETLINK answers");
                }
                return -1;
            }
            err = filter(h, arg1, arg2);
            if (err < 0)
                return err;

skip:
            h = NLMSG_NEXT(h, status);
        }
        if (msg.msg_flags & MSG_TRUNC) {
            continue;
        }
        if (status) {
            return -1;
        }
    }
}

#define PID 0
#define GROUP 0
static int rtsock_talk(struct rtsock_handle *rtsock, struct nlmsghdr *n)
{
    int status;
    unsigned seq;
    struct nlmsghdr *h;
    struct sockaddr_nl nladdr;
    struct iovec iov = {
        .iov_base = (void*) n,
        .iov_len = n->nlmsg_len
    };
    struct msghdr msg = {
        .msg_name = &nladdr,
        .msg_namelen = sizeof(nladdr),
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };
    char   buf[16384];

    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;
    nladdr.nl_pid = PID;
    nladdr.nl_groups = GROUP;

    n->nlmsg_seq = seq = ++rtsock->seq;

    status = sendmsg(rtsock->fd, &msg, 0);

    if (status < 0) {
        evtlog("rts0", status, 0, (uchar*)"Cannot talk to rtnetlink");
        return -1;
    }

    memset(buf,0,sizeof(buf));

    iov.iov_base = buf;

    while (1) {
        iov.iov_len = sizeof(buf);
        status = recvmsg(rtsock->fd, &msg, 0);

        if (status < 0) {
            if (errno == EINTR)
                continue;
            evtlog("rts2", errno, status, (uchar*)"OVERRUN");
            continue;
        }
        if (!status ) {
            evtlog("rts1", 0, 0, (uchar*)"EOF on netlink");
            return -1;
        }
        if (msg.msg_namelen != sizeof(nladdr)) {
            return -1;
        }
        for (h = (struct nlmsghdr*)buf; status >= sizeof(*h); ) {
            int len = h->nlmsg_len;
            int l = len - sizeof(*h);

            if (l<0 || len>status) {
                if (msg.msg_flags & MSG_TRUNC) {
                    evtlog("rts3", len, status, (uchar*)"Truncated message");
                    return -1;
                }
                return -1;
            }

            if (nladdr.nl_pid != PID ||
                h->nlmsg_pid != rtsock->local.nl_pid ||
                h->nlmsg_seq != seq) {
                continue;
            }

            if (h->nlmsg_type == NLMSG_ERROR) {
                struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(h);
                if (l < sizeof(struct nlmsgerr)) {
                    evtlog("rts4", l, 0, (uchar*)"ERROR truncated");
                } else {
                    errno = -err->error;
                    if (!errno) {
                        memcpy(n, h, h->nlmsg_len);
                        return 0;
                    }
                    evtlog("rts5", errno, 0, (uchar*)"RTNETLINK answers");
                }
                return -1;
            }
            memcpy(n, h, h->nlmsg_len);
            return 0;
        }
        if (msg.msg_flags & MSG_TRUNC) {
            continue;
        }
        if (status) {
            return -1;
        }
    }
}

int parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
    memset(tb, 0, sizeof(struct rtattr *) * (max + 1));
    while (RTA_OK(rta, len)) {
        if (rta->rta_type <= max)
            tb[rta->rta_type] = rta;
        rta = RTA_NEXT(rta,len);
    }
    if (len) {
        mlog("!!!Deficit %d, rta_len=%d\n", len, rta->rta_len);
    }
    return 0;
}

/*
    routing table
*/
static int get_route(struct nlmsghdr *n, inet_prefix *dst, inet_prefix *via)
{
    struct rtmsg *r = NLMSG_DATA(n);
    int len = n->nlmsg_len;
    struct rtattr * tb[RTA_MAX+1];
    int host_len = -1;

    len -= NLMSG_LENGTH(sizeof(*r));
    if (len < 0) {
        mlog("get route bug: wrong nlmsg len %d\n", len);
        return -1;
    }

    if (r->rtm_family == AF_INET6)
        host_len = 128;
    else if (r->rtm_family == AF_INET)
        host_len = 32;

    parse_rtattr(tb, RTA_MAX, RTM_RTA(r), len);

    memset(dst, 0, sizeof(*dst));
    dst->family = r->rtm_family;
    if (tb[RTA_DST]) {
        memcpy(&dst->data, RTA_DATA(tb[RTA_DST]), (r->rtm_dst_len+7)/8);
    }
    memset(via, 0, sizeof(*via));
    via->family = r->rtm_family;
    if (tb[RTA_GATEWAY]) {
        memcpy(&via->data, RTA_DATA(tb[RTA_GATEWAY]), host_len/8);
    }

    return 0;
}

/*
    neigh
*/
static int get_neigh(struct nlmsghdr *n, void *arg1, void *arg2)
{
    struct ndmsg *r = NLMSG_DATA(n);
    int len = n->nlmsg_len;
    struct rtattr * tb[NDA_MAX+1];
    struct filter *filter = (struct filter *)arg2;

    len -= NLMSG_LENGTH(sizeof(*r));
    if (len < 0) {
        mlog("get neigh bug: wrong nlmsg len %d\n", len);
        return -1;
    }

    /* familyは必ずチェック */
    if (filter->family != r->ndm_family)
        return 0;

    if (filter->index && (filter->index != r->ndm_ifindex))
        return 0;

    if (!(filter->state&r->ndm_state) &&
        (r->ndm_state || !(filter->state&0x100))) 
        return 0;

    parse_rtattr(tb, NDA_MAX, NDA_RTA(r), n->nlmsg_len - NLMSG_LENGTH(sizeof(*r)));

    if (tb[NDA_DST]) {
        if (memcmp(filter->pfx.data, RTA_DATA(tb[NDA_DST]), 
                RTA_PAYLOAD(tb[NDA_DST])) != 0) {
            return 0;
        }
        if (tb[NDA_LLADDR]) {
            memcpy(arg1, RTA_DATA(tb[NDA_LLADDR]), RTA_PAYLOAD(tb[NDA_LLADDR]));
        }
    }
    return 0;
}

#ifdef L_MODE 
static int
get_masklen(struct nlmsghdr *n, void *arg1, void *arg2)
{
    struct ifaddrmsg *ifa = NLMSG_DATA(n);
    int len = n->nlmsg_len;
    struct rtattr * rta_tb[IFA_MAX+1];

    struct filter *filter = (struct filter *)arg2;

    len -= NLMSG_LENGTH(sizeof(*ifa));
    if (len < 0) {
        mlog("get mask bug: wrong nlmsg len %d\n", len);
        return -1;
    }
    if (filter->family && (filter->family != ifa->ifa_family))
        return 0;

    parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(ifa), 
        n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa)));

    if (!rta_tb[IFA_LOCAL])
        rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];

    if (rta_tb[IFA_LOCAL]) {
        if (memcmp(filter->pfx.data, RTA_DATA(rta_tb[IFA_LOCAL]),
                RTA_PAYLOAD(rta_tb[IFA_LOCAL])) != 0) {
            return 0;
        }
        *(unsigned int*)arg1 = ifa->ifa_prefixlen;
    }
    return 0;
}
#endif

/* end */
