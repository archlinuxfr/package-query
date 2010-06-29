#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT([package-query], [0.3], [tuxce.net@gmail.com])
AM_INIT_AUTOMAKE([-Wall])
AC_CONFIG_SRCDIR([src/package-query.c])
AC_CONFIG_HEADER([config.h])

 

AC_PROG_CC
AC_PROG_LIBTOOL


# Checks for header files.
AC_CHECK_HEADERS([ctype.h getopt.h libintl.h limits.h locale.h sys/ioctl.h sys/utsname.h])

AC_CHECK_LIB([alpm], [alpm_version], ,
	AC_MSG_ERROR([pacman is needed to compile package-query]))
AC_CHECK_LIB([yajl], [yajl_free], ,
	AC_MSG_ERROR([yajl is needed to compile package-query]))

PKG_CHECK_MODULES(LIBCURL, libcurl)


# Enable or disable use of git version in pacman version string
usegitver=no
gitver=""
AC_CHECK_PROGS([GIT], [git])
if test "$GIT"; then
	AC_CHECK_FILE([.git/], hasgitdir=yes)
	if test "x$hasgitdir" = "xyes"; then
		gitver=$(git describe --abbrev=4)
		usegitver=yes
		AC_DEFINE([USE_GIT_VERSION], , [Use GIT version in version string])
		AC_SUBST([GIT_VERSION], [$gitver])
	fi
fi
AM_CONDITIONAL(USE_GIT_VERSION, test "x$usegitver" = "xyes")

# Help line for root directory
AC_ARG_WITH(root-dir,
	AS_HELP_STRING([--with-root-dir=path], [see pacman configuration]),
	[ROOTDIR=$withval], [ROOTDIR=/])

AC_SUBST(ROOTDIR)
AC_CONFIG_FILES([src/Makefile
doc/Makefile
Makefile
])
AC_OUTPUT

echo "
${PACKAGE_NAME}:

  Build information:
    source code location   : ${srcdir}
    prefix                 : ${prefix}
    sysconfdir             : $(eval echo ${sysconfdir})
       conf file           : $(eval echo ${sysconfdir})/pacman.conf
    localstatedir          : $(eval echo ${localstatedir})
       database dir        : $(eval echo ${localstatedir})/lib/pacman/
    compiler               : ${CC}
    compiler flags         : ${CFLAGS}

    package-query version  : ${PACKAGE_VERSION}
    using git version      : ${usegitver}
       git ver             : ${gitver}

  Directory and file information:
    root working directory : ${ROOTDIR}

"



