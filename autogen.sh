#!/bin/sh -x

libtoolize
aclocal -I m4 --install
autoheader
automake --foreign --add-missing
autoconf
