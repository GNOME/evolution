/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __CAMEL_ICONV_H__
#define __CAMEL_ICONV_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <sys/types.h>
#include <iconv.h>

const char *camel_iconv_locale_charset (void);
const char *camel_iconv_locale_language (void);

const char *camel_iconv_charset_name (const char *charset);

const char *camel_iconv_charset_language (const char *charset);

iconv_t camel_iconv_open (const char *to, const char *from);
size_t camel_iconv (iconv_t cd, const char **inbuf, size_t *inleft, char **outbuf, size_t *outleft);
void camel_iconv_close (iconv_t cd);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_ICONV_H__ */
