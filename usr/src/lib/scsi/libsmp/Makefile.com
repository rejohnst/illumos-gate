#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
#
# Copyright (c) 2018, Joyent, Inc.

LIBRARY = libsmp.a
VERS = .1

OBJECTS = \
	smp_engine.o \
	smp_errno.o \
	smp_plugin.o \
	smp_subr.o

include ../../../Makefile.lib
include ../../Makefile.defs

SRCS = $(OBJECTS:%.o=../common/%.c)
CSTD = $(CSTD_GNU99)
CPPFLAGS += -I../common -I. -D_REENTRANT
$(NOT_RELEASE_BUILD)CPPFLAGS += -DDEBUG
CFLAGS += $(CCVERBOSE)

CERRWARN += -_gcc=-Wno-type-limits
CERRWARN += $(CNOWARN_UNINIT)

SMOFF += all_func_returns

LDLIBS += \
	-lumem \
	-lc
LIBS =		$(DYNLIB) $(LINTLIB)
ROOTLIBDIR =	$(ROOTSCSILIBDIR)
ROOTLIBDIR64 =	$(ROOTSCSILIBDIR)/$(MACH64)

CLEANFILES += \
	../common/smp_errno.c

$(LINTLIB) :=	SRCS = $(SRCDIR)/$(LINTSRC)

.KEEP_STATE:

all : $(LIBS)

lint : lintcheck

../common/smp_errno.c: ../common/mkerrno.sh ../common/libsmp.h
	sh ../common/mkerrno.sh < ../common/libsmp.h > $@

pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

include ../../../Makefile.targ
include ../../Makefile.rootdirs
