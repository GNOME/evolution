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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "camel-imap4-specials.h"

#define CHARS_ATOM_SPECIALS   "(){]"
#define CHARS_LWSP            " \t\r\n"
#define CHARS_QUOTED_SPECIALS "\\\""
#define CHARS_LIST_WILDCARDS  "*%"

unsigned char camel_imap4_specials[256] = {
	  2,  2,  2,  2,  2,  2,  2,  2,  2,  6,  6,  2,  2,  6,  2,  2,
          2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
         20,  0,  8,  0,  0, 32,  0,  0,  1,  1, 32,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  8,  1,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0,  2,
          2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
          2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
          2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
          2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
          2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
          2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
          2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
          2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
};


static void
imap4_init_bits (unsigned short bit, unsigned short bitcopy, int remove, unsigned char *vals)
{
	int i, len = strlen (vals);
	
	if (!remove) {
		for (i = 0; i < len; i++)
			camel_imap4_specials[vals[i]] |= bit;
		if (bitcopy) {
			for (i = 0; i < 256; i++) {
				if (camel_imap4_specials[i] & bitcopy)
					camel_imap4_specials[i] |= bit;
			}
		}
	} else {
		for (i = 0; i < 256; i++)
			camel_imap4_specials[i] |= bit;
		for (i = 0; i < len; i++)
			camel_imap4_specials[vals[i]] &= ~bit;
		if (bitcopy) {
			for (i = 0; i < 256; i++) {
				if (camel_imap4_specials[i] & bitcopy)
					camel_imap4_specials[i] &= ~bit;
			}
		}
	}
}


void
camel_imap4_specials_init (void)
{
	int i;
	
	for (i = 0; i < 256; i++) {
		camel_imap4_specials[i] = 0;
		if (i <= 0x1f || i >= 0x7f)
			camel_imap4_specials[i] |= IS_CTRL;
	}
	
	camel_imap4_specials[' '] |= IS_SPACE;
	
	imap4_init_bits (IS_LWSP, 0, 0, CHARS_LWSP);
	imap4_init_bits (IS_ASPECIAL, 0, 0, CHARS_ATOM_SPECIALS);
	imap4_init_bits (IS_QSPECIAL, 0, 0, CHARS_QUOTED_SPECIALS);
	imap4_init_bits (IS_WILDCARD, 0, 0, CHARS_LIST_WILDCARDS);
}
