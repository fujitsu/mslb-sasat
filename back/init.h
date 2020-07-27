/**
 * file    init.h
 * brief   
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT) Yagi
 * 改版履歴(出荷後記入)
 * 版数   日付    変更者    リリースノート
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef __INIT_H__
#define __INIT_H__

#include <stdint.h>
#include <netinet/in.h>
#include "anycast.h"

int get_interface_info(struct ifdata *if_in, struct ifdata *if_eg);
int init_ud_socket(void);
uint64_t tsc_cycle_init(void);
void signal_block(void);
int init_socket_if(struct ifdata *, int, int *);
void get_svr_info(void);
void marge_info(void);
int create_back_thread(void*(*func)(void*), volatile int *wait);
void wait_thread(volatile int *flag, int cnt);
void sync_thread(volatile int *flag);
void sync_init(volatile int *flag, int cnt);
void get_default_gw(struct ifdata *, int, int);
int get_connect_mode(void);
int init_pid(void);

#endif
