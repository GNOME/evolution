/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-search.c: IMAP folder search */

/*
 *  Authors:
 *    Dan Winship <danw@helixcode.com>
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


#include <config.h>

#include <string.h>

#include "camel-imap-command.h"
#include "camel-imap-folder.h"
#include "camel-imap-search.h"

static ESExpResult *
imap_body_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv,
		    CamelFolderSearch *s);

static void
camel_imap_search_class_init (CamelImapSearchClass *camel_imap_search_class)
{
	/* virtual method overload */
	CamelFolderSearchClass *camel_folder_search_class =
		CAMEL_FOLDER_SEARCH_CLASS (camel_imap_search_class);
	
	/* virtual method overload */
	camel_folder_search_class->body_contains = imap_body_contains;
}

CamelType
camel_imap_search_get_type (void)
{
	static CamelType camel_imap_search_type = CAMEL_INVALID_TYPE;
	
	if (camel_imap_search_type == CAMEL_INVALID_TYPE) {
		camel_imap_search_type = camel_type_register (
			CAMEL_FOLDER_SEARCH_TYPE, "CamelImapSearch",
			sizeof (CamelImapSearch),
			sizeof (CamelImapSearchClass),
			(CamelObjectClassInitFunc) camel_imap_search_class_init,
			NULL, NULL, NULL);
	}

	return camel_imap_search_type;
}

static ESExpResult *
imap_body_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv,
		    CamelFolderSearch *s)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (s->folder->parent_store);
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (s->folder);
	char *value = argv[0]->value.string;
	CamelImapResponse *response;
	char *result, *p, *lasts = NULL;
	const char *uid;
	ESExpResult *r;
	CamelMessageInfo *info;

	if (s->current) {
		uid = camel_message_info_uid (s->current);
		r = e_sexp_result_new (ESEXP_RES_BOOL);
		r->value.bool = FALSE;
		response = camel_imap_command (store, s->folder, NULL,
					       "UID SEARCH UID %s BODY \"%s\"",
					       uid, value);
	} else {
		r = e_sexp_result_new(ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new ();
		response = camel_imap_command (store, s->folder, NULL,
					       "UID SEARCH BODY \"%s\"",
					       value);
	}
	if (!response)
		return r;
	result = camel_imap_response_extract (response, "SEARCH", NULL);
	if (!result)
		return r;

	p = result + sizeof ("* SEARCH");
	for (p = strtok_r (p, " ", &lasts); p; p = strtok_r (NULL, " ", &lasts)) {
		if (s->current) {
			if (!strcmp (uid, p)) {
				r->value.bool = TRUE;
				break;
			}
		} else {
			/* FIXME: The strings added to the array must be
			 * static...
			 */
			info = camel_folder_summary_uid (imap_folder->summary, p);
			g_ptr_array_add (r->value.ptrarray, (char *)camel_message_info_uid (info));
		}
	}

	return r;
}

/**
 * camel_imap_search_new:
 *
 * Return value: A new CamelImapSearch widget.
 **/
CamelFolderSearch *
camel_imap_search_new (void)
{
	CamelFolderSearch *new = CAMEL_FOLDER_SEARCH (camel_object_new (camel_imap_search_get_type ()));

	camel_folder_search_construct (new);
	return new;
}
