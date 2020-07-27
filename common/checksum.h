/**
 * file    checksum.h
 * brief   checksum関連処理 
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */

#ifndef __ANC_CHECKSUM__
#define __ANC_CHECKSUM__

#include <stdint.h>
#include <netinet/ip6.h>
#include "util_inline.h"

/*
    @brief 16bit補数和計算
*/
static inline uint16_t add16forCheckSum(uint16_t olddata, uint16_t newdata)
{
	uint32_t sum;
	sum	= olddata + newdata ;
	sum = (sum >> 16) + (sum & 0xffff);
	return ((uint16_t)sum);
}

/*
    @brief ワード境界での2バイト取得
*/
static inline unsigned short get_w(void *wp)
{
    unsigned char *p = (unsigned char *)wp;
    unsigned short v;

    v = (p[1] << 8) | (p[0]);

    return v;
}

/*
    @brief 奇数アドレスから始まるデータのチェックサム
*/
static inline u_int32_t
soft_checksum(const unsigned char *buf, u_int len)
{
    unsigned short *w = (unsigned short *)buf;
    u_int32_t  sum = 0;
    int maxcnt = len/sizeof(short) ;
    int    cnt ;

    for (cnt=0; cnt < maxcnt; cnt++, w++) {
        sum += get_w(w);
    }

    /* データサイズが16ビット単位で割り切れない場合は最後の1バイトはパディング */
    if ((len % sizeof(short)) != 0) {
        sum += (*(unsigned char *)w);
    }

    return sum;
}

/*
    @brief IPv6の擬似ヘッダのチェックサム計算
*/
static inline uint64_t
pseudo_sum_v6(struct ip6_hdr *ip6h)
{
    uint32_t *saddr, *daddr;

    saddr = (uint32_t*)&ip6h->ip6_src.s6_addr;
    daddr = (uint32_t*)&ip6h->ip6_dst.s6_addr;

    return ((uint64_t)saddr[0] + (uint64_t)saddr[1] + (uint64_t)saddr[2] + (uint64_t)saddr[3] +
            (uint64_t)daddr[0] + (uint64_t)daddr[1] + (uint64_t)daddr[2] + (uint64_t)daddr[3] +
            ((uint64_t)ip6h->ip6_plen) +
            ((uint64_t)IPPROTO_ICMPV6 << 8));
}

union l_util {
    uint16_t s[2];
    uint32_t l;
};
union q_util {
    uint16_t s[4];
    uint32_t l[2];
    uint64_t q;
};

#define ADDCARRY(x)  (x > 65535 ? x -= 65535 : x)

/*
    @brief icmpv6のチェックサムを計算する
*/
static inline uint16_t 
icmpv6_sum(const unsigned char *buf, uint len, uint64_t base)
{
    uint32_t sum;
    union q_util q_util;
    union l_util l_util;

    q_util.q = soft_checksum(buf, len);
    q_util.q += base;
    
    l_util.l = q_util.s[0] + q_util.s[1] + q_util.s[2] + q_util.s[3];
    sum = l_util.s[0] + l_util.s[1];                                  
    ADDCARRY(sum);                                                    

    return sum;
}

/*
    @brief ipアドレス変更によるipチェックサム値差分計算
*/
static inline uint16_t 
calc_chksum_delta(struct in_addr *old, struct in_addr *new)
{
    uint32_t sum;
    union l_util lu1, lu2, lu3;

    lu1.l = ~ntohl(old->s_addr);
    lu2.l = ntohl(new->s_addr);
    lu3.l = lu1.s[0] + lu1.s[1] + lu2.s[0] + lu2.s[1];
    sum = lu3.s[0] + lu3.s[1];
    ADDCARRY(sum);

    return ~sum;
}

#endif
