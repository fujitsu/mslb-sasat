/**
 * file    util_inline.h
 * brief   各種処理
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#include <stdint.h>

#ifndef VAL_SUBS
uint64_t cpu_mhz;
#else
extern uint64_t cpu_mhz;
#endif

#ifndef __UTIL_INLINE_H__
#define __UTIL_INLINE_H__

#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <byteswap.h>

#include "val.h"

#define get_clock() (cpu_mhz)

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define byteswap2(value)  (bswap_16(value)) 
#define byteswap4(value)  (bswap_32(value))
#define bswap2(value)     (__bswap_constant_16(value))

#ifndef LOCK_PREFIX
#define LOCK_PREFIX "lock ; "
#endif

#define mfence()  __asm__ __volatile__ ("lock; addl $0,0(%%esp)": : :"memory")

/*
   tscレジスタ読み込み（ia32)
*/
#define rdtsc()\
        ({uint64_t ret;\
          __asm__ volatile ("rdtsc" : "=A" (ret));\
          ret;})

/*
    @brief sleep (usec order)
*/
static inline void anycast_usleep(int usec)
{
   struct timespec req;

    req.tv_sec = 0;
    req.tv_nsec = usec * 1000;
    nanosleep(&req, NULL);
}

/*
    @brief sleep (ms order)
    あふれ注意(チェックしない）
*/
static inline void anycast_sleep(int msec)
{
    anycast_usleep(msec * 1000);
}

/*
    @brief ipv6比較
    @return 0 一致、 0以外 不一致
*/
static inline int 
cmp_ipv6(const struct in6_addr *ip1, const struct in6_addr *ip2)
{
    return memcmp(ip1, ip2, sizeof(struct in6_addr));
}

/*
    @brief ipv4比較
*/
static inline int
cmp_ipv4(const struct in_addr *ip1, const struct in_addr *ip2)
{
    return (ip1->s_addr != ip2->s_addr);
}

/*
    @brief mac比較
*/
static inline int
cmp_mac(const unsigned char *mac1, const unsigned char *mac2)
{
    return memcmp(mac1, mac2, ETH_ALEN);
}

static inline void
copy_mac(unsigned char *mac1, const unsigned char *mac2)
{
#if 0 
    ((uint16_t *)mac1)[0] = ((uint16_t *)mac2)[0];
    ((uint16_t *)mac1)[1] = ((uint16_t *)mac2)[1];
    ((uint16_t *)mac1)[2] = ((uint16_t *)mac2)[2];
#else
    memcpy(mac1, mac2, ETH_ALEN);
#endif
}

/*
    @brief IPが0かどうか
    @param struct sockaddr*
    @return 0 -- 0, 0以外 not 0
*/
static inline int
is_zero_ip(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        struct in_addr *ip = &((struct sockaddr_in*)sa)->sin_addr;
        return cmp_ipv4(ip, (struct in_addr*)zerodata);
    } else /* sa->sa_family == AF_INET6 */ {
        struct in6_addr *ip = &((struct sockaddr_in6*)sa)->sin6_addr;
        return cmp_ipv6(ip, (struct in6_addr*)zerodata);
    }
}

/*
    @brief ipv6アドレスのマスク
*/
static inline void mask_ipv6(struct in6_addr *ip, struct in6_addr *mask)
{
    ip->s6_addr32[0] &= mask->s6_addr32[0];
    ip->s6_addr32[1] &= mask->s6_addr32[1];
    ip->s6_addr32[2] &= mask->s6_addr32[2];
    ip->s6_addr32[3] &= mask->s6_addr32[3];
}

/*
    @brief tsc値をusに変換して返す
*/
static inline uint64_t
get_us(uint64_t tsc)
{
    return (tsc/get_clock());
}

/*
    @brief itc値をms単位で返す
*/
static inline uint64_t 
get_ms(uint64_t tsc)
{
    return (get_us(tsc)/1000);
}

/*
    @brief tsc値を秒単位で返す
*/
static inline uint 
get_sec(uint64_t tsc)
{
    return (uint)(get_ms(tsc)/1000);
}

/*
    @brief timevalに秒を設定
*/
static inline void
timeout_set(struct timeval *to, int sec)
{
    to->tv_sec = sec;
    to->tv_usec = 0;
}

/*
    @brief 文字列のスペース、タブをスキップ
*/
static inline char *
skip_space(char *p)
{
    while ((*p == ' ') || (*p == '\t')) {
        p++;
    }
    return p;
}

/*
    @brief マルチキャストMACかどうか判定する
*/
static inline int
is_multicast(unsigned char *mac) 
{
    return (mac[0] & 0x01);
}

/*
    @brief ブロードキャストMACかどうか判定する
*/
static inline int
is_broadcast(unsigned char *mac)
{
    static const unsigned char bmac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    return (cmp_mac(mac, bmac) == 0);
}

/*
    @brief IPv6 マルチキャストMACかどうか判定する
*/
static inline int
is_multicast_v6(unsigned char *mac)
{
    return (*(unsigned short*)mac == 0x3333);
}

/*
    @brief ハッシュ(v4)
*/
static inline uint ip_hash_code4(uint32_t addr, uint32_t mask)
{
    unsigned char *p = (unsigned char *)&addr;
    uint tmp1 = p[0] + p[3];
    uint tmp2 = p[1] + p[2];
    return ((tmp2<<8) + tmp1) & mask;
}

/*
    @brief ハッシュ(v6)
*/
static inline uint ip_hash_code6(uint32_t *addr, uint32_t mask)
{
    uint32_t hash = (addr[0] + addr[1] + addr[2] + addr[3]);
    return (hash + (hash>>16) + (hash>>24)) & mask;
}

#endif /* */
