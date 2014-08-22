AM_CONDITIONAL([WITH_MGMT_PY], true)
AC_DEFINE(WITH_MGMT_PY)

AC_CONFIG_FILES([
	src/xdpd/management/plugins/py/Makefile
])

