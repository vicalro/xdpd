MAINTAINERCLEANFILES = Makefile.in

SUBDIRS = extensions plugins snapshots 

noinst_LTLIBRARIES = libxdpd_mgmt.la

libxdpd_mgmt_la_SOURCES = \
	monitoring_manager.cc \
	plugin_manager.cc \
	switch_manager.cc \
	port_manager.cc\
	system_manager.cc

libxdpd_mgmt_la_LIBADD = extensions/libxdpd_mgmt_extensions.la \
			plugins/libxdpd_mgmt_plugins.la \
			snapshots/libxdpd_mgmt_snapshots.la
