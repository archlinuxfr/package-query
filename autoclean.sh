#!/bin/sh -xu

[ -f Makefile ] && make distclean
rm -rf autom4te.cache
rm -f {,src/,doc/}{Makefile.in,Makefile}
rm -f {config.h.in,config.h}
rm -f config.status
rm -f configure
rm -f stamp*
rm -f aclocal.m4
rm -f compile
rm -f libtool


