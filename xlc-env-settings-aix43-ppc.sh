#!/bin/sh

. /usr/lpp/som/bin/somenv.sh

export CONFIG_SHELL=/opt/freeware/bin/bash
export CONFIG_ENV_ARGS=/opt/freeware/bin/bash

export CC=cc
export CFLAGS="-qmaxmem=32768 -qcpluscmt -qarch=com -DSYSV -D_AIX -D_AIX32 -D_AIX41 -D_AIX43 \
-D_ALL_SOURCE -DXTHREADS -D_REENTRANT -D_THREAD_SAFE -DFUNCPROTO=15 -O3 \
-I/opt/freeware/include -I/usr/lpp/UMS/include -I/usr/lpp/som/include"

export CXX=xlC
export CXXFLAGS=$CFLAGS

export LD=ld
export LDFLAGS="-L/opt/freeware/lib -L/usr/lib/threads -Wl,-blibpath:/opt/freeware/lib:/usr/lib:/lib -Wl,-bmaxdata:0x80000000 -lpthreads -lc"
