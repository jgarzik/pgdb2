
AM_CPPFLAGS = -I$(top_srcdir)/include

EXTRA_DIST = run-basic.sh run-file.sh

TESTS = run-basic.sh run-file.sh

noinst_PROGRAMS = basic file

basic_SOURCES = basic.cc
basic_LDADD = ../lib/libpgdb2.la

file_SOURCES = file.cc
file_LDADD = ../lib/libpgdb2.la

