#!/bin/sh -x

aclocal -I m4 --install
autoheader
automake --foreign
autoconf
