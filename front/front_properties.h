/**
 * file    front_properties.h
 * brief   �v���p�e�B�t�@�C���ǂݏo��
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 *
 * �Ő�   ���t    �ύX��    �����[�X�m�[�g               
 * ---- -------- --------- --------------------------------------------------
 */
#ifndef __ANYC_UTIL_PROPERTIES_H__
#define __ANYC_UTIL_PROPERTIES_H__

#include "prop_common.h"

#ifdef VAL_SUBS
prop_db_t prop_db_front[] = { 
    {"in.ifname", "eth0"},
    {"ud_file",   "/dev/shm/.sasat"}, 
    {"vip_mode",  "1"},

    /*==============================================================*
     *    table end.
     *==============================================================*/
    {NULL, NULL}                // table end
};
#else
extern prop_db_t prop_db_front[];
#endif

#endif /*__ANYC_UTIL_PROPERTIES_H__*/

