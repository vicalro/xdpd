MAINTAINERCLEANFILES = Makefile.in

SUBDIRS = \
	mmap \
	mockup\
	pf_ring\
	vlink	

noinst_LTLIBRARIES = libxdpd_driver_gnu_linux_io_ports.la

libxdpd_driver_gnu_linux_io_ports_la_SOURCES = \
	ioport.cc \
	ioport.h 

libxdpd_driver_gnu_linux_io_ports_la_LIBADD = \
	pf_ring/libxdpd_driver_gnu_linux_io_ports_pf_ring.la \
	mmap/libxdpd_driver_gnu_linux_io_ports_mmap.la \
	mockup/libxdpd_driver_gnu_linux_io_ports_mockup.la\
	vlink/libxdpd_driver_gnu_linux_io_ports_vlink.la
