/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@novell.com>
 *           Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <ctype.h>

#include "camel-imap4-command.h"
#include "camel-imap4-engine.h"
#include "camel-imap4-stream.h"
#include "camel-imap4-utils.h"

#include "camel-imap4-search.h"


static void camel_imap4_search_class_init (CamelIMAP4SearchClass *klass);
static void camel_imap4_search_init (CamelIMAP4Search *search, CamelIMAP4SearchClass *klass);
static void camel_imap4_search_finalize (CamelObject *object);

static ESExpResult *imap4_body_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);


static CamelFolderSearchClass *parent_class = NULL;


CamelType
camel_imap4_search_get_type (void)
{
	static CamelType type = 0;
	
	if (!type) {
		type = camel_type_register (camel_folder_search_get_type (),
					    "CamelIMAP4Search",
					    sizeof (CamelIMAP4Search),
					    sizeof (CamelIMAP4SearchClass),
					    (CamelObjectClassInitFunc) camel_imap4_search_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_imap4_search_init,
					    (CamelObjectFinalizeFunc) camel_imap4_search_finalize);
	}
	
	return type;
}

static void
camel_imap4_search_class_init (CamelIMAP4SearchClass *klass)
{
	CamelFolderSearchClass *search_class = (CamelFolderSearchClass *) klass;
	
	parent_class = (CamelFolderSearchClass *) camel_type_get_global_classfuncs (CAMEL_FOLDER_SEARCH_TYPE);
	
	search_class->body_contains = imap4_body_contains;
}

static void
camel_imap4_search_init (CamelIMAP4Search *search, CamelIMAP4SearchClass *klass)
{
	search->engine = NULL;
}

static void
camel_imap4_search_finalize (CamelObject *object)
{
	;
}


CamelFolderSearch *
camel_imap4_search_new (CamelIMAP4Engine *engine, const char *cachedir)
{
	CamelIMAP4Search *search;
	
	search = (CamelIMAP4Search *) camel_object_new (camel_imap4_search_get_type ());
	camel_folder_search_construct ((CamelFolderSearch *) search);
	search->engine = engine;
	
	return (CamelFolderSearch *) search;
}


static int
untagged_search (CamelIMAP4Engine *engine, CamelIMAP4Command *ic, guint32 index, camel_imap4_token_t *token, CamelException *ex)
{
	CamelFolderSummary *summary = ((CamelFolder *) engine->folder)->summary;
	GPtrArray *matches = ic->user_data;
	CamelMessageInfo *info;
	char uid[12];
	
	while (1) {
		if (camel_imap4_engine_next_token (engine, token, ex) == -1)
			return -1;
		
		if (token->token == '\n')
			break;
		
		if (token->token != CAMEL_IMAP4_TOKEN_NUMBER || token->v.number == 0)
			goto unexpected;
		
		sprintf (uid, "%u", token->v.number);
		if ((info = camel_folder_summary_uid (summary, uid))) {
			g_ptr_array_add (matches, (char *) camel_message_info_uid (info));
			camel_folder_summary_info_free (summary, info);
		}
	}
	
	return 0;
	
 unexpected:
	
	camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
	
	return -1;
}

static ESExpResult *
imap4_body_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	CamelIMAP4Search *imap4_search = (CamelIMAP4Search *) search;
	CamelIMAP4Engine *engine = imap4_search->engine;
	GPtrArray *strings, *matches, *infos;
	register const unsigned char *inptr;
	gboolean utf8_search = FALSE;
	GPtrArray *summary_set;
	CamelMessageInfo *info;
	CamelIMAP4Command *ic;
	const char *expr;
	ESExpResult *r;
	int id, i, n;
	size_t used;
	char *set;
	
	summary_set = search->summary_set ? search->summary_set : search->summary;
	
	/* check the simple cases */
	if (argc == 0 || summary_set->len == 0) {
		/* match nothing */
		if (search->current) {
			r = e_sexp_result_new (f, ESEXP_RES_BOOL);
			r->value.bool = FALSE;
		} else {
			r = e_sexp_result_new (f, ESEXP_RES_ARRAY_PTR);
			r->value.ptrarray = g_ptr_array_new ();
		}
		
		return r;
	} else if (argc == 1 && argv[0]->type == ESEXP_RES_STRING && argv[0]->value.string[0] == '\0') {
		/* match everything */
		if (search->current) {
			r = e_sexp_result_new (f, ESEXP_RES_BOOL);
			r->value.bool = TRUE;
		} else {
			r = e_sexp_result_new (f, ESEXP_RES_ARRAY_PTR);
			r->value.ptrarray = g_ptr_array_new ();
			g_ptr_array_set_size (r->value.ptrarray, summary_set->len);
			r->value.ptrarray->len = summary_set->len;
			for (i = 0; i < summary_set->len; i++) {
				info = g_ptr_array_index (summary_set, i);
				r->value.ptrarray->pdata[i] = (char *) camel_message_info_uid (info);
			}
		}
		
		return r;
	}
	
	strings = g_ptr_array_new ();
	for (i = 0; i < argc; i++) {
		if (argv[i]->type == ESEXP_RES_STRING && argv[i]->value.string[0] != '\0') {
			g_ptr_array_add (strings, argv[i]->value.string);
			if (!utf8_search) {
				inptr = (unsigned char *) argv[i]->value.string;
				while (*inptr != '\0') {
					if (!isascii ((int) *inptr)) {
						utf8_search = TRUE;
						break;
					}
					
					inptr++;
				}
			}
		}
	}
	
	if (strings->len == 0) {
		/* match everything */
		g_ptr_array_free (strings, TRUE);
		
		if (search->current) {
			r = e_sexp_result_new (f, ESEXP_RES_BOOL);
			r->value.bool = TRUE;
		} else {
			r = e_sexp_result_new (f, ESEXP_RES_ARRAY_PTR);
			r->value.ptrarray = g_ptr_array_new ();
			g_ptr_array_set_size (r->value.ptrarray, summary_set->len);
			r->value.ptrarray->len = summary_set->len;
			for (i = 0; i < summary_set->len; i++) {
				info = g_ptr_array_index (summary_set, i);
				r->value.ptrarray->pdata[i] = (char *) camel_message_info_uid (info);
			}
		}
		
		return r;
	}
	
	g_ptr_array_add (strings, NULL);
	matches = g_ptr_array_new ();
	infos = g_ptr_array_new ();
	
	if (search->current) {
		g_ptr_array_add (infos, search->current);
	} else {
		g_ptr_array_set_size (infos, summary_set->len);
		infos->len = summary_set->len;
		for (i = 0; i < summary_set->len; i++)
			infos->pdata[i] = summary_set->pdata[i];
	}
	
 retry:
	if (utf8_search && (engine->capa & CAMEL_IMAP4_CAPABILITY_utf8_search))
		expr = "UID SEARCH CHARSET UTF-8 UID %s BODY %V\r\n";
	else
		expr = "UID SEARCH UID %s BODY %V\r\n";
	
	used = strlen (expr) + (5 * (strings->len - 2));
	
	for (i = 0; i < infos->len; i += n) {
		n = camel_imap4_get_uid_set (engine, search->folder->summary, infos, i, used, &set);
		
		ic = camel_imap4_engine_queue (engine, search->folder, expr, set, strings->pdata);
		camel_imap4_command_register_untagged (ic, "SEARCH", untagged_search);
		ic->user_data = matches;
		g_free (set);
		
		while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
			;
		
		if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
			camel_imap4_command_unref (ic);
			goto done;
		}
		
		
		if (ic->result == CAMEL_IMAP4_RESULT_NO && utf8_search && (engine->capa & CAMEL_IMAP4_CAPABILITY_utf8_search)) {
			int j;
			
			/* might be because the server is lame and doesn't support UTF-8 */
			for (j = 0; j < ic->resp_codes->len; j++) {
				CamelIMAP4RespCode *resp = ic->resp_codes->pdata[j];
				
				if (resp->code == CAMEL_IMAP4_RESP_CODE_BADCHARSET) {
					engine->capa &= ~CAMEL_IMAP4_CAPABILITY_utf8_search;
					camel_imap4_command_unref (ic);
					goto retry;
				}
			}
		}
		
		if (ic->result != CAMEL_IMAP4_RESULT_OK) {
			camel_imap4_command_unref (ic);
			break;
		}
		
		camel_imap4_command_unref (ic);
	}
	
 done:
	
	g_ptr_array_free (strings, TRUE);
	g_ptr_array_free (infos, TRUE);
	
	if (search->current) {
		const char *uid;
		
		uid = camel_message_info_uid (search->current);
		r = e_sexp_result_new (f, ESEXP_RES_BOOL);
		r->value.bool = FALSE;
		for (i = 0; i < matches->len; i++) {
			if (!strcmp (matches->pdata[i], uid)) {
				r->value.bool = TRUE;
				break;
			}
		}
		
		g_ptr_array_free (matches, TRUE);
	} else {
		r = e_sexp_result_new (f, ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = matches;
	}
	
	return r;
}
