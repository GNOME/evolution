/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  Camel
 *  Copyright (C) 1999-2004 Jeffrey Stedfast
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
 */


#ifndef __CAMEL_IMAP4_SPECIALS_H__
#define __CAMEL_IMAP4_SPECIALS_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

enum {
	IS_ASPECIAL   = (1 << 0),
	IS_CTRL       = (1 << 1),
	IS_LWSP       = (1 << 2), 
	IS_QSPECIAL   = (1 << 3),
	IS_SPACE      = (1 << 4),
	IS_WILDCARD   = (1 << 5),
};

extern unsigned char camel_imap4_specials[256];

#define is_atom(x) ((camel_imap4_specials[(unsigned char)(x)] & (IS_ASPECIAL|IS_SPACE|IS_CTRL|IS_WILDCARD|IS_QSPECIAL)) == 0)
#define is_ctrl(x) ((camel_imap4_specials[(unsigned char)(x)] & IS_CTRL) != 0)
#define is_lwsp(x) ((camel_imap4_specials[(unsigned char)(x)] & IS_LWSP) != 0)
#define is_type(x, t) ((camel_imap4_specials[(unsigned char)(x)] & (t)) != 0)
#define is_qsafe(x) ((camel_imap4_specials[(unsigned char)(x)] & (IS_QSPECIAL|IS_CTRL)) == 0)
#define is_wild(x)  ((camel_imap4_specials[(unsigned char)(x)] & IS_WILDCARD) != 0)

void camel_imap4_specials_init (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_IMAP4_SPECIALS_H__ */
