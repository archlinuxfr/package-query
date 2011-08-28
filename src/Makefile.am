# paths set at make time
conffile  = ${sysconfdir}/pacman.conf
dbpath    = ${localstatedir}/lib/pacman/

AM_CFLAGS = -D_GNU_SOURCE
AM_LDFLAGS = $(LIBCURL) $(LIBINTL)

DEFS = -DLOCALEDIR=\"@localedir@\" \
       -DCONFFILE=\"$(conffile)\" \
       -DROOTDIR=\"$(ROOTDIR)\" \
       -DDBPATH=\"$(dbpath)\" \
       -DAUR_BASE_URL=\"$(AUR_BASE_URL)\" \
	   @DEFS@
if USE_GIT_VERSION
DEFS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif
bin_PROGRAMS = package-query


package_query_SOURCES = aur.h aur.c \
	alpm-query.h alpm-query.c \
	util.h util.c \
	color.h color.c \
	package-query.c 


