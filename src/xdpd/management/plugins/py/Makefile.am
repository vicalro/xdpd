MAINTAINERCLEANFILES = Makefile.in

#Library MUST BE named libxdpd_mgmt_PLUGIN_NAME.la
noinst_LTLIBRARIES = libxdpd_mgmt_py.la
lib_LTLIBRARIES = libxdpd_mgmt_py_native.la

libxdpd_mgmt_py_native_la_SOURCES = \
	exported.cc\
	native_wrap.cxx
	
libxdpd_mgmt_py_la_SOURCES = \
	py.cc

libxdpd_mgmt_py_la_LIBADD = -lpython2.7  
