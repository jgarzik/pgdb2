
AM_CPPFLAGS = -I$(top_srcdir)/include

lib_LTLIBRARIES = libpgdb2.la

libpgdb2_la_SOURCES = db.cc dir.cc file.cc get.cc inode.cc

libpgdb2_la_LDFLAGS = \
	-version-info $(LIBPGDB2_CURRENT):$(LIBPGDB2_REVISION):$(LIBPGDB2_AGE) \
	-no-undefined


