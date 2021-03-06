AC_INIT(xdpd - GNU/Linux driver, m4_esyscmd_s(cat VERSION), rofl@bisdn.de, xdpd_gnu_linux_fm)

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_GNU_SOURCE

AC_PROG_INSTALL
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_LD
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AUTOCONF_ENV="SHELL=/bin/sh"
LT_INIT
link_all_deplibs=yes
link_all_deplibs_CXX=yes
AC_ENABLE_STATIC
LIBTOOL="$LIBTOOL --preserve-dup-deps"
AC_SUBST(LIBTOOL)dnl

# default flags
CFLAGS="-Werror -Wall $CFLAGS"
CXXFLAGS="-Werror -Wall $CXXFLAGS"
LDFLAGS=" -Wl,-export-dynamic"

AC_DEFINE(__STDC_FORMAT_MACROS)

PKG_CHECK_MODULES([ROFL], [rofl >= 0.4.0], [have_rofl=yes], [have_rofl=no])
if (test "${have_rofl}" = "yes"); then
        CPPFLAGS+="$ROFL_CFLAGS"
        LIBS+="$ROFL_LIBS"
fi

#Debug
m4_include([../../../../config/debug.m4])

# Check for profiling mode
m4_include([../../../../config/profiling.m4])

# pkt classifier 
m4_include([config/pktclassifier.m4])

#Check kernel support
m4_include([config/vlan.m4])


AC_CONFIG_FILES([
	Makefile
	src/Makefile
	src/hal-imp/Makefile
	src/hal-imp/openflow/openflow1x/Makefile
	src/io/Makefile
	src/io/packet_classifiers/Makefile
	src/io/packet_classifiers/c_types_pktclassifier/Makefile
	src/io/ports/Makefile
	src/io/ports/mmap/Makefile
	src/io/ports/mockup/Makefile
	src/io/ports/vlink/Makefile
	src/io/scheduler/Makefile
	src/pipeline-imp/Makefile
	src/processing/Makefile
	src/util/Makefile

	test/Makefile
	test/regression/Makefile
	test/regression/io/Makefile
	test/unit/Makefile
	test/unit/util/Makefile
	test/unit/io/Makefile
	test/unit/io/packet_classifiers/Makefile
	test/unit/io/packet_classifiers/c_pktclassifier/Makefile
	test/unit/io/packet_classifiers/c_pktclassifier/headers/Makefile
])

AC_OUTPUT
