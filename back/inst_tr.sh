#!/bin/bash
#
# install sasat translator
#

declare -r d_exe="/opt/sasat/sbin"
declare -r d_etc="/var/opt/sasat/etc"
declare -r d_log="/var/opt/sasat/log"
declare -r d_cmn="../common"
declare -r d_cmd="../cmd"

declare -r f_fexe="sasat_f"
declare -r f_bexe="sasat_b"
declare -r f_conf="sasat.conf"
declare -r f_pol="sasat.policy"

# 実行ファイル
    if [ ! -e $d_exe ]; then
        /bin/mkdir -p $d_exe 2>&1
    fi
    if [ -e $f_fexe ]; then
        /bin/cp -p $f_fexe $d_exe 2>&1
    fi
    if [ -e $f_bexe ]; then
        /bin/cp -p $f_bexe $d_exe 2>&1
    fi

    /usr/bin/install -m 755 $d_cmd/sasat $d_exe/sasat
    /usr/bin/install -m 755 $d_cmd/bctl $d_exe/bctl
    /usr/bin/install -m 755 $d_cmd/psasat $d_exe/psasat

# 設定ファイル
    if [ ! -e $d_etc ]; then
        /bin/mkdir -p $d_etc 2>&1
    fi
    if [ ! -e $d_etc/$f_conf ]; then
        /bin/cp -p $d_cmn/$f_conf $d_etc 2>&1
    fi
    if [ ! -e $d_etc/$f_pol ] && [ -e $f_pol ]; then
        /bin/cp -p $d_cmn/$f_pol $d_etc 2>&1
    fi

# logディレクトリ
    if [ ! -e $d_log ]; then
        /bin/mkdir -p $d_log 2>&1
    fi

    exit 0

# end
