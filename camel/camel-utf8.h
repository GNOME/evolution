/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
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

#ifndef _CAMEL_UTF8_H
#define _CAMEL_UTF8_H

void camel_utf8_putc(unsigned char **ptr, guint32 c);
guint32 camel_utf8_getc(const unsigned char **ptr);
guint32 camel_utf8_getc_limit (const unsigned char **ptr, const unsigned char *end);

/* utility func for utf8 gstrings */
void g_string_append_u(GString *out, guint32 c);

/* convert utf7 to/from utf8, actually this is modified IMAP utf7 */
char *camel_utf7_utf8(const char *ptr);
char *camel_utf8_utf7(const char *ptr);

/* convert ucs2 to/from utf8 */
char *camel_utf8_ucs2(const char *ptr);
char *camel_ucs2_utf8(const char *ptr);

#endif /* ! _CAMEL_UTF8_H */
