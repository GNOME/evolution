/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* gmime-rfc2047.c: implemention of RFC2047 */

/*
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org> .
 * 
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

/* 
 * Authors:  Robert Brady <rwb197@ecs.soton.ac.uk>
 */

#ifndef GMIME_RFC2047_H
#define GMIME_RFC2047_H 1
#include <glib.h>

gchar *gmime_rfc2047_decode (const gchar *text, const gchar* charset);
gchar *gmime_rfc2047_encode (const gchar *text, const gchar* charset);

/* 
 * pass text and charset, (e.g. "UTF-8", or "ISO-8859-1"), and
 * it will encode or decode text according to RFC2047
 *
 * You will need to link with libunicode for these.
 *
 * TODO : Make it so that if charset==NULL, the charset specified (either
 *   implicitly or explicity) in the locale is used.
 *
 * TODO : Run torture tests and fix the buffer overruns in these functions.
 *
 * The caller will need to free the memory for the string.
 */

#endif /* GMIME_RFC2047_H */
