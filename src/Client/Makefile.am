# this lists the binaries to produce, the (non-PHONY, binary) targets in
# the previous manual Makefile
include ../../common.mk

lib_LTLIBRARIES = libHuFS.la
SRC_DIR = src
HEADER_DIR = include
AM_CPPFLAGS += -I./include/ -I../Common/include
AM_CPPFLAGS += $(CLIENT_WAIT_FLAG)

vpath %.cpp $(SRC_DIR)
vpath %.h $(HEADER_DIR)

libHuFS_la_SOURCES = CBB_basic.cpp	\
		     CBB_client.cpp 	\
		     CBB_stream.cpp 	\
		     CBB_posix.cpp 	\
		     CBB_stdio.cpp
# headers
libHuFS_la_SOURCES += CBB_basic.h	\
		      CBB_client.h	\
		      CBB_posix.h	\
		      CBB_stdio.h	\
		      CBB_stream.h

libHuFS_la_LDFLAGS = -version-info 1:0:0

bin_PROGRAMS = hufs
hufs_SOURCES = src/CBB_fuse.cpp 
hufs_CPPFLAGS = $(AM_CPPFLAGS) -D_FILE_OFFSET_BITS=64
hufs_LDADD = libHuFS.la ../Common/libCommon.a
hufs_LDFLAGS = $(AM_LDFLAGS) -ldl 
SUBDIRS = ../Common