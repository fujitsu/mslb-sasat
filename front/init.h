/**
 * file    init.h
 * brief   初期化
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 *
 * 版数   日付    変更者    リリースノート               
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef __INIT_H__
#define __INIT_H__

#include <stdint.h>
#include <netinet/in.h>
#include "anycast.h"

void *front_ingress1 (void *arg);

int get_interface_info(struct ifdata *if_in, struct ifdata *if_eg);
pthread_t create_net_thread(void *(*func)(void *));
int init_ud_socket(void);
uint64_t tsc_cycle_init(void);
void signal_block(void);
int init_socket_if(struct ifdata *, int * fd);
int init_pid(void);

#endif
