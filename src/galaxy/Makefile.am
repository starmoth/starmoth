include $(top_srcdir)/Makefile.common

AM_CFLAGS += $(WARN_CFLAGS)
AM_CPPFLAGS += $(WARN_CPPFLAGS)
AM_CXXFLAGS += $(STD_CXXFLAGS) $(WARN_CXXFLAGS)

AM_CPPFLAGS += -isystem $(top_srcdir)/contrib -I$(srcdir)/..

noinst_LIBRARIES = libgalaxy.a
noinst_HEADERS = \
	Galaxy.h \
	GalaxyCache.h \
	Sector.h \
	StarSystem.h \
	SystemPath.h

libgalaxy_a_SOURCES = \
	Galaxy.cpp \
	GalaxyCache.cpp \
	Sector.cpp \
	StarSystem.cpp \
	SystemPath.cpp
