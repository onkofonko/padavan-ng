#  test_cuesheet - Simple tester for cuesheet routines in grabbag
#  Copyright (C) 2002-2009  Josh Coalson
#  Copyright (C) 2011-2025  Xiph.Org Foundation
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License along
#  with this program; if not, write to the Free Software Foundation, Inc.,
#  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

EXTRA_DIST = \
	CMakeLists.txt

AM_CPPFLAGS = -I$(top_builddir) -I$(srcdir)/include -I$(top_srcdir)/include
check_PROGRAMS = test_cuesheet
test_cuesheet_SOURCES = \
	main.c

if OS_IS_WINDOWS
win_utf8_lib = $(top_builddir)/src/share/win_utf8_io/libwin_utf8_io.la
endif

test_cuesheet_LDADD = \
	$(top_builddir)/src/share/grabbag/libgrabbag.la \
	$(top_builddir)/src/share/replaygain_analysis/libreplaygain_analysis.la \
	$(top_builddir)/src/libFLAC/libFLAC.la \
	$(win_utf8_lib)

CLEANFILES = test_cuesheet.exe
