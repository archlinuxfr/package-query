#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.69)
AC_INIT([package-query], [1.7], [https://github.com/archlinuxfr/package-query])
AM_INIT_AUTOMAKE([-Wall])
AC_CONFIG_MACRO_DIR([m4])
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

LIBCURL_CHECK_CONFIG([yes], [7.19.4])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.13.1)


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

AC_ARG_WITH(aur-url,
	AS_HELP_STRING([--with-aur-url=url], [default to https://aur.archlinux.org]),
	[AUR_BASE_URL=$withval], [AUR_BASE_URL=https://aur.archlinux.org])

AC_SUBST(AUR_BASE_URL)
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

  Variable information:
    root working directory : ${ROOTDIR}
    aur base url           : ${AUR_BASE_URL}
"



