
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001  Ximian Inc.
 * Author: Michael Zucchi <notzed@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _E_ICONV_H_
#define _E_ICONV_H_

#include <iconv.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

const char *e_iconv_charset_name(const char *charset);
iconv_t e_iconv_open(const char *oto, const char *ofrom);
void e_iconv_close(iconv_t ip);
const char *e_iconv_locale_charset(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_E_ICONV_H_ */
