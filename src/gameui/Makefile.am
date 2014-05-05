include $(top_srcdir)/Makefile.common

AM_CFLAGS += $(WARN_CFLAGS)
AM_CPPFLAGS += $(WARN_CPPFLAGS)
AM_CXXFLAGS += $(STD_CXXFLAGS) $(WARN_CXXFLAGS)

AM_CPPFLAGS += -isystem $(top_srcdir)/contrib -I$(srcdir)/..

noinst_LIBRARIES = libgameui.a
noinst_HEADERS = \
	GameUI.h \
	Face.h \
	ModelSpinner.h \
	BindingCapture.h

libgameui_a_SOURCES = \
	Face.cpp \
	ModelSpinner.cpp \
	BindingCapture.cpp
