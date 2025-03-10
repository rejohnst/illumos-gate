/*
 * Copyright 2013 Garrett D'Amore <garrett@damore.org>
 * Copyright 2010 Nexenta Systems, Inc.  All rights reserved.
 * Copyright (c) 2002-2004 Tim J. Robbins. All rights reserved.
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "lint.h"
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "mblocal.h"
#include "lctype.h"

static size_t	_BIG5_mbrtowc(wchar_t *_RESTRICT_KYWD,
		    const char *_RESTRICT_KYWD,
		    size_t, mbstate_t *RESTRICT_KYWD, boolean_t);
static int	_BIG5_mbsinit(const mbstate_t *);
static size_t	_BIG5_wcrtomb(char *_RESTRICT_KYWD, wchar_t,
		    mbstate_t *_RESTRICT_KYWD);
static size_t	_BIG5_mbsnrtowcs(wchar_t *_RESTRICT_KYWD,
		    const char **_RESTRICT_KYWD, size_t, size_t,
		    mbstate_t *_RESTRICT_KYWD);
static size_t	_BIG5_wcsnrtombs(char *_RESTRICT_KYWD,
		    const wchar_t **_RESTRICT_KYWD, size_t, size_t,
		    mbstate_t *_RESTRICT_KYWD);

typedef struct {
	wchar_t	ch;
} _BIG5State;

void
_BIG5_init(struct lc_ctype *lct)
{
	lct->lc_mbrtowc = _BIG5_mbrtowc;
	lct->lc_wcrtomb = _BIG5_wcrtomb;
	lct->lc_mbsnrtowcs = _BIG5_mbsnrtowcs;
	lct->lc_wcsnrtombs = _BIG5_wcsnrtombs;
	lct->lc_mbsinit = _BIG5_mbsinit;
	lct->lc_max_mblen = 2;
	lct->lc_is_ascii = 0;
}

static int
_BIG5_mbsinit(const mbstate_t *ps)
{

	return (ps == NULL || ((const _BIG5State *)ps)->ch == 0);
}

static int
_big5_check(uint_t c)
{

	c &= 0xff;
	return ((c >= 0xa1 && c <= 0xfe) ? 2 : 1);
}

static size_t
_BIG5_mbrtowc(wchar_t *_RESTRICT_KYWD pwc, const char *_RESTRICT_KYWD s,
    size_t n, mbstate_t *_RESTRICT_KYWD ps, boolean_t zero)
{
	_BIG5State *bs;
	wchar_t wc;
	size_t len;

	bs = (_BIG5State *)ps;

	if ((bs->ch & ~0xFF) != 0) {
		/* Bad conversion state. */
		errno = EINVAL;
		return ((size_t)-1);
	}

	if (s == NULL) {
		s = "";
		n = 1;
		pwc = NULL;
	}

	if (n == 0)
		/* Incomplete multibyte sequence */
		return ((size_t)-2);

	if (bs->ch != 0) {
		if (*s == '\0') {
			errno = EILSEQ;
			return ((size_t)-1);
		}
		wc = (bs->ch << 8) | (*s & 0xFF);
		if (pwc != NULL)
			*pwc = wc;
		bs->ch = 0;
		return (1);
	}

	len = (size_t)_big5_check(*s);
	wc = *s++ & 0xff;
	if (len == 2) {
		if (n < 2) {
			/* Incomplete multibyte sequence */
			bs->ch = wc;
			return ((size_t)-2);
		}
		if (*s == '\0') {
			errno = EILSEQ;
			return ((size_t)-1);
		}
		wc = (wc << 8) | (*s++ & 0xff);
		if (pwc != NULL)
			*pwc = wc;
		return (2);
	} else {
		if (pwc != NULL)
			*pwc = wc;
		if (zero || wc != L'\0') {
			return (1);
		} else {
			return (0);
		}
	}
}

static size_t
_BIG5_wcrtomb(char *_RESTRICT_KYWD s, wchar_t wc, mbstate_t *_RESTRICT_KYWD ps)
{
	_BIG5State *bs;

	bs = (_BIG5State *)ps;

	if (bs->ch != 0) {
		errno = EINVAL;
		return ((size_t)-1);
	}

	if (s == NULL)
		/* Reset to initial shift state (no-op) */
		return (1);
	if (wc & 0x8000) {
		*s++ = (wc >> 8) & 0xff;
		*s = wc & 0xff;
		return (2);
	}
	*s = wc & 0xff;
	return (1);
}

static size_t
_BIG5_mbsnrtowcs(wchar_t *_RESTRICT_KYWD dst, const char **_RESTRICT_KYWD src,
    size_t nms, size_t len, mbstate_t *_RESTRICT_KYWD ps)
{
	return (__mbsnrtowcs_std(dst, src, nms, len, ps, _BIG5_mbrtowc));
}

static size_t
_BIG5_wcsnrtombs(char *_RESTRICT_KYWD dst, const wchar_t **_RESTRICT_KYWD src,
    size_t nwc, size_t len, mbstate_t *_RESTRICT_KYWD ps)
{
	return (__wcsnrtombs_std(dst, src, nwc, len, ps, _BIG5_wcrtomb));
}
