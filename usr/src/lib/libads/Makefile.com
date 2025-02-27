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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2014 Nexenta Systems, Inc.  All rights reserved.
#

LIBRARY =	libads.a
VERS =		.1
LINT_OBJECTS =	dsgetdc.o poke.o
OBJECTS =	$(LINT_OBJECTS) adspriv_xdr.o

include ../../Makefile.lib

CSTD=	$(CSTD_GNU99)
C99LMODE=	-Xc99=%all

LIBS =		$(DYNLIB) $(LINTLIB)
LDLIBS +=	-lnsl -lc
SRCDIR =	../common
$(LINTLIB):=	SRCS = $(SRCDIR)/$(LINTSRC)

CFLAGS +=	$(CCVERBOSE)
CPPFLAGS +=	-D_REENTRANT -I$(SRCDIR) -I..
# CPPFLAGS +=	-I$(SRC)/lib/libldap5/include/ldap

CERRWARN +=	-_gcc=-Wno-type-limits
CERRWARN +=	$(CNOWARN_UNINIT)
CERRWARN +=	-_gcc=-Wno-unused-variable

.KEEP_STATE:

all: $(LIBS)

lint := OBJECTS = $(LINT_OBJECTS)

lint: lintcheck

#LINTFLAGS += -erroff=E_CONSTANT_CONDITION
#LINTFLAGS64 += -erroff=E_CONSTANT_CONDITION

include ../../Makefile.targ
