/**
 * file    stat_common.h
 * brief   統計処理(共通処理)
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         Yagi
 *
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef __STAT_COMMON_H__
#define __STAT_COMMON_H__

#include <stdio.h> 
#include <stdint.h>
#include <string.h>

/* 統計情報のビット長 */
#define STATLEN_32

#ifdef STATLEN_64   /* 64bit */
typedef long long unsigned int ssz_t;
#define STAT_DATALEN 20
static const char conv_string[] = "%20llu";

#else
typedef long unsigned int ssz_t;
#define STAT_DATALEN 10
static const char conv_string[] = "%10lu";
#endif

/* 統計内部保持形式 */
struct stat_member {
    ssz_t stat;
    const char *name;
};

/* 読める形式 */
struct stat_print {
    char stat_num[STAT_DATALEN];
    char name[0];
};

static inline void
convert_stat(struct stat_print *s1, struct stat_member *s2)
{
    sprintf(s1->stat_num, conv_string, s2->stat);
    strcpy(s1->name, s2->name);
}

#endif
