/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#include <stdio.h>
#include <string.h>

#include "camel-imap-utils.h"
#include "string-utils.h"

#define d(x) x

char *
imap_next_word (char *buf)
{
	char *word;

	/* skip over current word */
	for (word = buf; *word && *word != ' '; word++);

	/* skip over white space */
	for ( ; *word && *word == ' '; word++);

	return word;
}

gboolean
imap_parse_list_response (char *buf, char *namespace, char **flags, char **sep, char **folder)
{
	char *word, *ep, *f;
	
	*flags = NULL;
	*sep = NULL;
	*folder = NULL;
	
	if (*buf != '*')
		return FALSE;
	
	word = imap_next_word (buf);
	if (g_strncasecmp (word, "LIST", 4) && g_strncasecmp (word, "LSUB", 4))
		return FALSE;
	
	/* get the flags */
	word = imap_next_word (word);
	if (*word != '(')
		return FALSE;
	
	word++;
	for (ep = word; *ep && *ep != ')'; ep++);
	if (*ep != ')')
		return FALSE;
	
	*flags = g_strndup (word, (gint)(ep - word));
	
	/* get the directory separator */
	word = imap_next_word (ep);
	if (*word) {
		for (ep = word; *ep && *ep != ' '; ep++);
		*sep = g_strndup (word, (gint)(ep - word));
		string_unquote (*sep);
	} else {
		return FALSE;
	}
	
	/* get the folder name */
	word = imap_next_word (word);
	*folder = g_strdup (word);
	g_strstrip (*folder);
	
	/* chop out the folder prefix */
	if (*namespace && !strncmp (*folder, namespace, strlen (namespace))) {
		f = *folder + strlen (namespace) + strlen (*sep);
		memmove (*folder, f, strlen (f) + 1);
	}
	
	string_unquote (*folder);  /* unquote the mailbox if it's quoted */
	
	return TRUE;
}
