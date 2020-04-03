#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright 2020 Joyent, Inc.
#

.KEEP_STATE:
.SUFFIXES:

SRCS += sastopo.c
OBJS = $(SRCS:%.c=%.o)

PROG = sastopo
ROOTLIBFM = $(ROOT)/usr/lib/fm
ROOTLIBFMD = $(ROOT)/usr/lib/fm/fmd
ROOTPROG = $(ROOTLIBFMD)/$(PROG)

$(NOT_RELEASE_BUILD)CPPFLAGS += -DDEBUG
CPPFLAGS += -I. -I../common -I$(SRC)/lib/fm/topo/libtopo/common
CFLAGS += $(CTF_FLAGS) $(CCVERBOSE) $(XSTRCONST) $(CSTD_GNU99)
LDLIBS += -L$(ROOT)/usr/lib/fm -ltopo -lnvpair
LDFLAGS += -R/usr/lib/fm

.NO_PARALLEL:
.PARALLEL: $(OBJS)

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(CTFMERGE) -L VERSION -o $@ $(OBJS)
	$(POST_PROCESS)

%.o: ../common/%.c
	$(COMPILE.c) $<
	$(CTFCONVERT_O)

%.o: %.c
	$(COMPILE.c) $<
	$(CTFCONVERT_O)

clean:
	$(RM) $(OBJS)

clobber: clean
	$(RM) $(PROG)

$(ROOTLIBFMD)/%: %
	$(INS.file)

install_h:

install: all $(ROOTPROG)
