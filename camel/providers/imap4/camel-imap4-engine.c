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
#include <ctype.h>
#include <errno.h>

#include <camel/camel-sasl.h>
#include <camel/camel-stream-buffer.h>
#include <camel/camel-i18n.h>

#include "camel-imap4-summary.h"
#include "camel-imap4-command.h"
#include "camel-imap4-stream.h"
#include "camel-imap4-folder.h"
#include "camel-imap4-utils.h"

#include "camel-imap4-engine.h"

#define d(x) x


static void camel_imap4_engine_class_init (CamelIMAP4EngineClass *klass);
static void camel_imap4_engine_init (CamelIMAP4Engine *engine, CamelIMAP4EngineClass *klass);
static void camel_imap4_engine_finalize (CamelObject *object);


static CamelObjectClass *parent_class = NULL;


CamelType
camel_imap4_engine_get_type (void)
{
	static CamelType type = 0;
	
	if (!type) {
		type = camel_type_register (camel_object_get_type (),
					    "CamelIMAP4Engine",
					    sizeof (CamelIMAP4Engine),
					    sizeof (CamelIMAP4EngineClass),
					    (CamelObjectClassInitFunc) camel_imap4_engine_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_imap4_engine_init,
					    (CamelObjectFinalizeFunc) camel_imap4_engine_finalize);
	}
	
	return type;
}

static void
camel_imap4_engine_class_init (CamelIMAP4EngineClass *klass)
{
	parent_class = camel_type_get_global_classfuncs (CAMEL_OBJECT_TYPE);
	
	klass->tagprefix = 'A';
}

static void
camel_imap4_engine_init (CamelIMAP4Engine *engine, CamelIMAP4EngineClass *klass)
{
	engine->state = CAMEL_IMAP4_ENGINE_DISCONNECTED;
	engine->level = CAMEL_IMAP4_LEVEL_UNKNOWN;
	
	engine->session = NULL;
	engine->service = NULL;
	engine->url = NULL;
	
	engine->istream = NULL;
	engine->ostream = NULL;
	
	engine->authtypes = g_hash_table_new (g_str_hash, g_str_equal);
	
	engine->capa = 0;
	
	/* this is the suggested default, impacts the max command line length we'll send */
	engine->maxlentype = CAMEL_IMAP4_ENGINE_MAXLEN_LINE;
	engine->maxlen = 1000;
	
	engine->namespaces.personal = NULL;
	engine->namespaces.other = NULL;
	engine->namespaces.shared = NULL;
	
	if (klass->tagprefix > 'Z')
		klass->tagprefix = 'A';
	
	engine->tagprefix = klass->tagprefix++;
	engine->tag = 0;
	
	engine->nextid = 1;
	
	engine->folder = NULL;
	
	e_dlist_init (&engine->queue);
}

static void
imap4_namespace_clear (CamelIMAP4Namespace **namespace)
{
	CamelIMAP4Namespace *node, *next;
	
	node = *namespace;
	while (node != NULL) {
		next = node->next;
		g_free (node->path);
		g_free (node);
		node = next;
	}
	
	*namespace = NULL;
}

static void
camel_imap4_engine_finalize (CamelObject *object)
{
	CamelIMAP4Engine *engine = (CamelIMAP4Engine *) object;
	EDListNode *node;
	
	if (engine->istream)
		camel_object_unref (engine->istream);
	
	if (engine->ostream)
		camel_object_unref (engine->ostream);
	
	g_hash_table_foreach (engine->authtypes, (GHFunc) g_free, NULL);
	g_hash_table_destroy (engine->authtypes);
	
	imap4_namespace_clear (&engine->namespaces.personal);
	imap4_namespace_clear (&engine->namespaces.other);
	imap4_namespace_clear (&engine->namespaces.shared);
	
	if (engine->folder)
		camel_object_unref (engine->folder);
	
	while ((node = e_dlist_remhead (&engine->queue))) {
		node->next = NULL;
		node->prev = NULL;
		
		camel_imap4_command_unref ((CamelIMAP4Command *) node);
	}
}


/**
 * camel_imap4_engine_new:
 * @service: service
 *
 * Returns a new imap4 engine
 **/
CamelIMAP4Engine *
camel_imap4_engine_new (CamelService *service, CamelIMAP4ReconnectFunc reconnect)
{
	CamelIMAP4Engine *engine;
	
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), NULL);
	
	engine = (CamelIMAP4Engine *) camel_object_new (CAMEL_TYPE_IMAP4_ENGINE);
	engine->session = service->session;
	engine->url = service->url;
	engine->service = service;
	engine->reconnect = reconnect;
	
	return engine;
}


/**
 * camel_imap4_engine_take_stream:
 * @engine: imap4 engine
 * @stream: tcp stream
 * @ex: exception
 *
 * Gives ownership of @stream to @engine and reads the greeting from
 * the stream.
 *
 * Returns 0 on success or -1 on fail.
 *
 * Note: on error, @stream will be unref'd.
 **/
int
camel_imap4_engine_take_stream (CamelIMAP4Engine *engine, CamelStream *stream, CamelException *ex)
{
	camel_imap4_token_t token;
	int code;
	
	g_return_val_if_fail (CAMEL_IS_IMAP4_ENGINE (engine), -1);
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);
	
	if (engine->istream)
		camel_object_unref (engine->istream);
	
	if (engine->ostream)
		camel_object_unref (engine->ostream);
	
	engine->istream = (CamelIMAP4Stream *) camel_imap4_stream_new (stream);
	engine->ostream = camel_stream_buffer_new (stream, CAMEL_STREAM_BUFFER_WRITE);
	engine->state = CAMEL_IMAP4_ENGINE_CONNECTED;
	camel_object_unref (stream);
	
	if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
		goto exception;
	
	if (token.token != '*') {
		camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
		goto exception;
	}
	
	if ((code = camel_imap4_engine_handle_untagged_1 (engine, &token, ex)) == -1) {
		goto exception;
	} else if (code != CAMEL_IMAP4_UNTAGGED_OK && code != CAMEL_IMAP4_UNTAGGED_PREAUTH) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, _("Unexpected greeting from IMAP server %s."),
				      engine->url->host);
		goto exception;
	}
	
	return 0;
	
 exception:
	
	engine->state = CAMEL_IMAP4_ENGINE_DISCONNECTED;
	
	camel_object_unref (engine->istream);
	engine->istream = NULL;
	camel_object_unref (engine->ostream);
	engine->ostream = NULL;
	
	return -1;
}


/**
 * camel_imap4_engine_capability:
 * @engine: IMAP4 engine
 * @ex: exception
 *
 * Forces the IMAP4 engine to query the IMAP4 server for a list of capabilities.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
camel_imap4_engine_capability (CamelIMAP4Engine *engine, CamelException *ex)
{
	CamelIMAP4Command *ic;
	int id, retval = 0;
	
	ic = camel_imap4_engine_prequeue (engine, NULL, "CAPABILITY\r\n");
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		retval = -1;
	}
	
	camel_imap4_command_unref (ic);
	
	return retval;
}


/**
 * camel_imap4_engine_namespace:
 * @engine: IMAP4 engine
 * @ex: exception
 *
 * Forces the IMAP4 engine to query the IMAP4 server for a list of namespaces.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
camel_imap4_engine_namespace (CamelIMAP4Engine *engine, CamelException *ex)
{
	camel_imap4_list_t *list;
	GPtrArray *array = NULL;
	CamelIMAP4Command *ic;
	int id, i;
	
	if (engine->capa & CAMEL_IMAP4_CAPABILITY_NAMESPACE) {
		ic = camel_imap4_engine_prequeue (engine, NULL, "NAMESPACE\r\n");
	} else {
		ic = camel_imap4_engine_prequeue (engine, NULL, "LIST \"\" \"\"\r\n");
		camel_imap4_command_register_untagged (ic, "LIST", camel_imap4_untagged_list);
		ic->user_data = array = g_ptr_array_new ();
	}
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		
		if (array != NULL)
			g_ptr_array_free (array, TRUE);
		
		return -1;
	}
	
	if (array != NULL) {
		if (ic->result == CAMEL_IMAP4_RESULT_OK) {
			CamelIMAP4Namespace *namespace;
			
			g_assert (array->len == 1);
			list = array->pdata[0];
			
			namespace = g_new (CamelIMAP4Namespace, 1);
			namespace->next = NULL;
			namespace->path = g_strdup ("");
			namespace->sep = list->delim;
			
			engine->namespaces.personal = namespace;
		} else {
			/* should never *ever* happen */
		}
		
		for (i = 0; i < array->len; i++) {
			list = array->pdata[i];
			g_free (list->name);
			g_free (list);
		}
		
		g_ptr_array_free (array, TRUE);
	}
	
	camel_imap4_command_unref (ic);
	
	return 0;
}


int
camel_imap4_engine_select_folder (CamelIMAP4Engine *engine, CamelFolder *folder, CamelException *ex)
{
	CamelIMAP4RespCode *resp;
	CamelIMAP4Command *ic;
	int id, retval = 0;
	int i;
	
	g_return_val_if_fail (CAMEL_IS_IMAP4_ENGINE (engine), -1);
	g_return_val_if_fail (CAMEL_IS_IMAP4_FOLDER (folder), -1);
	
	/* POSSIBLE FIXME: if the folder to be selected will already
	 * be selected by the time the queue is emptied, simply
	 * no-op? */
	
	ic = camel_imap4_engine_queue (engine, folder, "SELECT %F\r\n", folder);
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		return -1;
	}
	
	switch (ic->result) {
	case CAMEL_IMAP4_RESULT_OK:
		/*folder->mode = 0;*/
		for (i = 0; i < ic->resp_codes->len; i++) {
			resp = ic->resp_codes->pdata[i];
			switch (resp->code) {
			case CAMEL_IMAP4_RESP_CODE_PERM_FLAGS:
				folder->permanent_flags = resp->v.flags;
				break;
			case CAMEL_IMAP4_RESP_CODE_READONLY:
				/*folder->mode = CAMEL_FOLDER_MODE_READ_ONLY;*/
				break;
			case CAMEL_IMAP4_RESP_CODE_READWRITE:
				/*folder->mode = CAMEL_FOLDER_MODE_READ_WRITE;*/
				break;
			case CAMEL_IMAP4_RESP_CODE_UIDNEXT:
				camel_imap4_summary_set_uidnext (folder->summary, resp->v.uidnext);
				break;
			case CAMEL_IMAP4_RESP_CODE_UIDVALIDITY:
				camel_imap4_summary_set_uidvalidity (folder->summary, resp->v.uidvalidity);
				break;
			case CAMEL_IMAP4_RESP_CODE_UNSEEN:
				camel_imap4_summary_set_unseen (folder->summary, resp->v.unseen);
				break;
			default:
				break;
			}
		}
		
		/*if (folder->mode == 0) {
		  folder->mode = CAMEL_FOLDER_MODE_READ_ONLY;
		  g_warning ("Expected to find [READ-ONLY] or [READ-WRITE] in SELECT response");
		  }*/
		
		break;
	case CAMEL_IMAP4_RESULT_NO:
		/* FIXME: would be good to save the NO reason into the err message */
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot select folder `%s': Invalid mailbox name"),
				      folder->full_name);
		retval = -1;
		break;
	case CAMEL_IMAP4_RESULT_BAD:
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot select folder `%s': Bad command"),
				      folder->full_name);
		retval = -1;
		break;
	default:
		g_assert_not_reached ();
		retval = -1;
	}
	
	camel_imap4_command_unref (ic);
	
	return retval;
}


static struct {
	const char *name;
	guint32 flag;
} imap4_capabilities[] = {
	{ "IMAP4",         CAMEL_IMAP4_CAPABILITY_IMAP4         },
	{ "IMAP4REV1",     CAMEL_IMAP4_CAPABILITY_IMAP4REV1     },
	{ "STATUS",        CAMEL_IMAP4_CAPABILITY_STATUS        },
	{ "NAMESPACE",     CAMEL_IMAP4_CAPABILITY_NAMESPACE     },
	{ "UIDPLUS",       CAMEL_IMAP4_CAPABILITY_UIDPLUS       },
	{ "LITERAL+",      CAMEL_IMAP4_CAPABILITY_LITERALPLUS   },
	{ "LOGINDISABLED", CAMEL_IMAP4_CAPABILITY_LOGINDISABLED },
	{ "STARTTLS",      CAMEL_IMAP4_CAPABILITY_STARTTLS      },
	{ NULL,            0                                    }
};

static gboolean
auth_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	return TRUE;
}

static int
engine_parse_capability (CamelIMAP4Engine *engine, int sentinel, CamelException *ex)
{
	camel_imap4_token_t token;
	int i;
	
	engine->capa = CAMEL_IMAP4_CAPABILITY_utf8_search;
	engine->level = 0;
	
	g_hash_table_foreach_remove (engine->authtypes, (GHRFunc) auth_free, NULL);
	
	if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	while (token.token == CAMEL_IMAP4_TOKEN_ATOM) {
		if (!strncasecmp ("AUTH=", token.v.atom, 5)) {
			CamelServiceAuthType *auth;
			
			if ((auth = camel_sasl_authtype (token.v.atom + 5)) != NULL)
				g_hash_table_insert (engine->authtypes, g_strdup (token.v.atom + 5), auth);
		} else {
			for (i = 0; imap4_capabilities[i].name; i++) {
				if (!strcasecmp (imap4_capabilities[i].name, token.v.atom))
					engine->capa |= imap4_capabilities[i].flag;
			}
		}
		
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			return -1;
	}
	
	if (token.token != sentinel) {
		camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	/* unget our sentinel token */
	camel_imap4_stream_unget_token (engine->istream, &token);
	
	/* figure out which version of IMAP4 we are dealing with */
	if (engine->capa & CAMEL_IMAP4_CAPABILITY_IMAP4REV1) {
		engine->level = CAMEL_IMAP4_LEVEL_IMAP4REV1;
		engine->capa |= CAMEL_IMAP4_CAPABILITY_STATUS;
	} else if (engine->capa & CAMEL_IMAP4_CAPABILITY_IMAP4) {
		engine->level = CAMEL_IMAP4_LEVEL_IMAP4;
	} else {
		engine->level = CAMEL_IMAP4_LEVEL_UNKNOWN;
	}
	
	return 0;
}

static int
engine_parse_flags_list (CamelIMAP4Engine *engine, CamelIMAP4RespCode *resp, int perm, CamelException *ex)
{
	guint32 flags = 0;
	
	if (camel_imap4_parse_flags_list (engine, &flags, ex) == -1)
		return-1;
	
	if (resp != NULL)
		resp->v.flags = flags;
	
	if (engine->current && engine->current->folder) {
		if (perm)
			((CamelFolder *) engine->current->folder)->permanent_flags = flags;
		/*else
		  ((CamelFolder *) engine->current->folder)->folder_flags = flags;*/
	} else if (engine->folder) {
		if (perm)
			((CamelFolder *) engine->folder)->permanent_flags = flags;
		/*else
		  ((CamelFolder *) engine->folder)->folder_flags = flags;*/
	} else {
		fprintf (stderr, "We seem to be in a bit of a pickle. we've just parsed an untagged %s\n"
			 "response for a folder, yet we do not currently have a folder selected?\n",
			 perm ? "PERMANENTFLAGS" : "FLAGS");
	}
	
	return 0;
}

static int
engine_parse_flags (CamelIMAP4Engine *engine, CamelException *ex)
{
	camel_imap4_token_t token;
	
	if (engine_parse_flags_list (engine, NULL, FALSE, ex) == -1)
		return -1;
	
	if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	if (token.token != '\n') {
		d(fprintf (stderr, "Expected to find a '\\n' token after the FLAGS response\n"));
		camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	return 0;
}

static int
engine_parse_namespace (CamelIMAP4Engine *engine, CamelException *ex)
{
	CamelIMAP4Namespace *namespaces[3], *node, *tail;
	camel_imap4_token_t token;
	int i, n = 0;
	
	imap4_namespace_clear (&engine->namespaces.personal);
	imap4_namespace_clear (&engine->namespaces.other);
	imap4_namespace_clear (&engine->namespaces.shared);
	
	if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	do {
		namespaces[n] = NULL;
		tail = (CamelIMAP4Namespace *) &namespaces[n];
		
		if (token.token == '(') {
			/* decode the list of namespace pairs */
			if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
				goto exception;
			
			while (token.token == '(') {
				/* decode a namespace pair */
				
				/* get the path name token */
				if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
					goto exception;
				
				if (token.token != CAMEL_IMAP4_TOKEN_QSTRING) {
					d(fprintf (stderr, "Expected to find a qstring token as first element in NAMESPACE pair\n"));
					camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
					goto exception;
				}
				
				node = g_new (CamelIMAP4Namespace, 1);
				node->next = NULL;
				node->path = g_strdup (token.v.qstring);
				
				/* get the path delimiter token */
				if (camel_imap4_engine_next_token (engine, &token, ex) == -1) {
					g_free (node->path);
					g_free (node);
					
					goto exception;
				}
				
				if (token.token != CAMEL_IMAP4_TOKEN_QSTRING || strlen (token.v.qstring) > 1) {
					d(fprintf (stderr, "Expected to find a qstring token as second element in NAMESPACE pair\n"));
					camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
					g_free (node->path);
					g_free (node);
					
					goto exception;
				}
				
				node->sep = *token.v.qstring;
				tail->next = node;
				tail = node;
				
				/* canonicalise the namespace path */
				if (node->path[strlen (node->path) - 1] == node->sep)
					node->path[strlen (node->path) - 1] = '\0';
				
				/* canonicalise if this is an INBOX namespace */
				if (!g_ascii_strncasecmp (node->path, "INBOX", 5) &&
				    (node->path[6] == '\0' || node->path[6] == node->sep))
					memcpy (node->path, "INBOX", 5);
				
				/* get the closing ')' for this namespace pair */
				if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
					goto exception;
				
				if (token.token != ')') {
					d(fprintf (stderr, "Expected to find a ')' token to close the current namespace pair\n"));
					camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
					
					goto exception;
				}
				
				/* get the next token (should be either '(' or ')') */
				if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
					goto exception;
			}
			
			if (token.token != ')') {
				d(fprintf (stderr, "Expected to find a ')' to close the current namespace list\n"));
				camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
				goto exception;
			}
		} else if (token.token == CAMEL_IMAP4_TOKEN_NIL) {
			/* namespace list is NIL */
			namespaces[n] = NULL;
		} else {
			d(fprintf (stderr, "Expected to find either NIL or '(' token in untagged NAMESPACE response\n"));
			camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
			goto exception;
		}
		
		/* get the next token (should be either '(', NIL, or '\n') */
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			goto exception;
		
		n++;
	} while (n < 3);
	
	engine->namespaces.personal = namespaces[0];
	engine->namespaces.other = namespaces[1];
	engine->namespaces.shared = namespaces[2];
	
	return 0;
	
 exception:
	
	for (i = 0; i <= n; i++)
		imap4_namespace_clear (&namespaces[i]);
	
	return -1;
}


/**
 * 
 * resp-text-code  = "ALERT" /
 *                   "BADCHARSET" [SP "(" astring *(SP astring) ")" ] /
 *                   capability-data / "PARSE" /
 *                   "PERMANENTFLAGS" SP "(" [flag-perm *(SP flag-perm)] ")" /
 *                   "READ-ONLY" / "READ-WRITE" / "TRYCREATE" /
 *                   "UIDNEXT" SP nz-number / "UIDVALIDITY" SP nz-number /
 *                   "UNSEEN" SP nz-number /
 *                   atom [SP 1*<any TEXT-CHAR except "]">]
 **/

static struct {
	const char *name;
	camel_imap4_resp_code_t code;
	int save;
} imap4_resp_codes[] = {
	{ "ALERT",          CAMEL_IMAP4_RESP_CODE_ALERT,       0 },
	{ "BADCHARSET",     CAMEL_IMAP4_RESP_CODE_BADCHARSET,  0 },
	{ "CAPABILITY",     CAMEL_IMAP4_RESP_CODE_CAPABILITY,  0 },
	{ "PARSE",          CAMEL_IMAP4_RESP_CODE_PARSE,       1 },
	{ "PERMANENTFLAGS", CAMEL_IMAP4_RESP_CODE_PERM_FLAGS,  1 },
	{ "READ-ONLY",      CAMEL_IMAP4_RESP_CODE_READONLY,    1 },
	{ "READ-WRITE",     CAMEL_IMAP4_RESP_CODE_READWRITE,   1 },
	{ "TRYCREATE",      CAMEL_IMAP4_RESP_CODE_TRYCREATE,   1 },
	{ "UIDNEXT",        CAMEL_IMAP4_RESP_CODE_UIDNEXT,     1 },
	{ "UIDVALIDITY",    CAMEL_IMAP4_RESP_CODE_UIDVALIDITY, 1 },
	{ "UNSEEN",         CAMEL_IMAP4_RESP_CODE_UNSEEN,      1 },
	{ "NEWNAME",        CAMEL_IMAP4_RESP_CODE_NEWNAME,     1 },
	{ "APPENDUID",      CAMEL_IMAP4_RESP_CODE_APPENDUID,   1 },
	{ "COPYUID",        CAMEL_IMAP4_RESP_CODE_COPYUID,     1 },
	{ NULL,             CAMEL_IMAP4_RESP_CODE_UNKNOWN,     0 }
};


int
camel_imap4_engine_parse_resp_code (CamelIMAP4Engine *engine, CamelException *ex)
{
	CamelIMAP4RespCode *resp = NULL;
	camel_imap4_resp_code_t code;
	camel_imap4_token_t token;
	unsigned char *linebuf;
	size_t len;
	
	if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	if (token.token != '[') {
		d(fprintf (stderr, "Expected a '[' token (followed by a RESP-CODE)\n"));
		camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	if (token.token != CAMEL_IMAP4_TOKEN_ATOM) {
		d(fprintf (stderr, "Expected an atom token containing a RESP-CODE\n"));
		camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	for (code = 0; imap4_resp_codes[code].name; code++) {
		if (!strcmp (imap4_resp_codes[code].name, token.v.atom)) {
			if (engine->current && imap4_resp_codes[code].save) {
				resp = g_new0 (CamelIMAP4RespCode, 1);
				resp->code = code;
			}
			break;
		}
	}
	
	switch (code) {
	case CAMEL_IMAP4_RESP_CODE_BADCHARSET:
		/* apparently we don't support UTF-8 afterall */
		engine->capa &= ~CAMEL_IMAP4_CAPABILITY_utf8_search;
		break;
	case CAMEL_IMAP4_RESP_CODE_CAPABILITY:
		/* capability list follows */
		if (engine_parse_capability (engine, ']', ex) == -1)
			goto exception;
		break;
	case CAMEL_IMAP4_RESP_CODE_PERM_FLAGS:
		/* flag list follows */
		if (engine_parse_flags_list (engine, resp, TRUE, ex) == -1)
			goto exception;
		break;
	case CAMEL_IMAP4_RESP_CODE_READONLY:
		break;
	case CAMEL_IMAP4_RESP_CODE_READWRITE:
		break;
	case CAMEL_IMAP4_RESP_CODE_TRYCREATE:
		break;
	case CAMEL_IMAP4_RESP_CODE_UIDNEXT:
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			goto exception;
		
		if (token.token != CAMEL_IMAP4_TOKEN_NUMBER) {
			d(fprintf (stderr, "Expected an nz_number token as an argument to the UIDNEXT RESP-CODE\n"));
			camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
			goto exception;
		}
		
		if (resp != NULL)
			resp->v.uidnext = token.v.number;
		
		break;
	case CAMEL_IMAP4_RESP_CODE_UIDVALIDITY:
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			goto exception;
		
		if (token.token != CAMEL_IMAP4_TOKEN_NUMBER) {
			d(fprintf (stderr, "Expected an nz_number token as an argument to the UIDVALIDITY RESP-CODE\n"));
			camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
			goto exception;
		}
		
		if (resp != NULL)
			resp->v.uidvalidity = token.v.number;
		
		break;
	case CAMEL_IMAP4_RESP_CODE_UNSEEN:
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			return -1;
		
		if (token.token != CAMEL_IMAP4_TOKEN_NUMBER) {
			d(fprintf (stderr, "Expected an nz_number token as an argument to the UNSEEN RESP-CODE\n"));
			camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
			goto exception;
		}
		
		if (resp != NULL)
			resp->v.unseen = token.v.number;
		
		break;
	case CAMEL_IMAP4_RESP_CODE_NEWNAME:
		/* this RESP-CODE may actually be removed - see here:
		 * http://www.washington.edu/imap4/listarch/2001/msg00058.html */
		
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			return -1;
		
		if (token.token != CAMEL_IMAP4_TOKEN_ATOM && token.token != CAMEL_IMAP4_TOKEN_QSTRING) {
			d(fprintf (stderr, "Expected an atom or qstring token as the first argument to the NEWNAME RESP-CODE\n"));
			camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
			goto exception;
		}
		
		if (resp != NULL)
			resp->v.newname[0] = g_strdup (token.v.atom);
		
		if (token.token != CAMEL_IMAP4_TOKEN_ATOM && token.token != CAMEL_IMAP4_TOKEN_QSTRING) {
			d(fprintf (stderr, "Expected an atom or qstring token as the second argument to the NEWNAME RESP-CODE\n"));
			camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
			goto exception;
		}
		
		if (resp != NULL)
			resp->v.newname[1] = g_strdup (token.v.atom);
		
		break;
	case CAMEL_IMAP4_RESP_CODE_APPENDUID:
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			return -1;
		
		if (token.token != CAMEL_IMAP4_TOKEN_NUMBER) {
			d(fprintf (stderr, "Expected an nz_number token as the first argument to the APPENDUID RESP-CODE\n"));
			camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
			goto exception;
		}
		
		if (resp != NULL)
			resp->v.appenduid.uidvalidity = token.v.number;
		
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			return -1;
		
		if (token.token != CAMEL_IMAP4_TOKEN_NUMBER) {
			d(fprintf (stderr, "Expected an nz_number token as the second argument to the APPENDUID RESP-CODE\n"));
			camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
			goto exception;
		}
		
		if (resp != NULL)
			resp->v.appenduid.uid = token.v.number;
		
		break;
	case CAMEL_IMAP4_RESP_CODE_COPYUID:
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			return -1;
		
		if (token.token != CAMEL_IMAP4_TOKEN_NUMBER) {
			d(fprintf (stderr, "Expected an nz_number token as the first argument to the COPYUID RESP-CODE\n"));
			camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
			goto exception;
		}
		
		if (resp != NULL)
			resp->v.copyuid.uidvalidity = token.v.number;
		
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			return -1;
		
		if (token.token != CAMEL_IMAP4_TOKEN_ATOM) {
			d(fprintf (stderr, "Expected an atom token as the second argument to the COPYUID RESP-CODE\n"));
			camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
			goto exception;
		}
		
		if (resp != NULL)
			resp->v.copyuid.srcset = g_strdup (token.v.atom);
		
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			return -1;
		
		if (token.token != CAMEL_IMAP4_TOKEN_ATOM) {
			d(fprintf (stderr, "Expected an atom token as the third argument to the APPENDUID RESP-CODE\n"));
			camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
			goto exception;
		}
		
		if (resp != NULL)
			resp->v.copyuid.destset = g_strdup (token.v.atom);
		
		break;
	default:
		d(fprintf (stderr, "Unknown RESP-CODE encountered: %s\n", token.v.atom));
		
		/* extensions are of the form: "[" atom [SPACE 1*<any TEXT_CHAR except "]">] "]" */
		
		/* eat up the TEXT_CHARs */
		while (token.token != ']' && token.token != '\n') {
			if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
				goto exception;
		}
		
		break;
	}
	
	while (token.token != ']' && token.token != '\n') {
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			goto exception;
	}
	
	if (token.token != ']') {
		camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
		d(fprintf (stderr, "Expected to find a ']' token after the RESP-CODE\n"));
		return -1;
	}
	
	if (code == CAMEL_IMAP4_RESP_CODE_ALERT) {
		if (camel_imap4_engine_line (engine, &linebuf, &len, ex) == -1)
			goto exception;
		
		camel_session_alert_user (engine->session, CAMEL_SESSION_ALERT_INFO, linebuf, FALSE);
		g_free (linebuf);
	} else if (resp != NULL && code == CAMEL_IMAP4_RESP_CODE_PARSE) {
		if (camel_imap4_engine_line (engine, &linebuf, &len, ex) == -1)
			goto exception;
		
		resp->v.parse = linebuf;
	} else {
		/* eat up the rest of the response */
		if (camel_imap4_engine_line (engine, NULL, NULL, ex) == -1)
			goto exception;
	}
	
	if (resp != NULL)
		g_ptr_array_add (engine->current->resp_codes, resp);
	
	return 0;
	
 exception:
	
	if (resp != NULL)
		camel_imap4_resp_code_free (resp);
	
	return -1;
}



/* returns -1 on error, or one of CAMEL_IMAP4_UNTAGGED_[OK,NO,BAD,PREAUTH,HANDLED] on success */
int
camel_imap4_engine_handle_untagged_1 (CamelIMAP4Engine *engine, camel_imap4_token_t *token, CamelException *ex)
{
	int code = CAMEL_IMAP4_UNTAGGED_HANDLED;
	CamelIMAP4Command *ic = engine->current;
	CamelIMAP4UntaggedCallback untagged;
	CamelFolder *folder;
	unsigned int v;
	
	if (camel_imap4_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	if (token->token == CAMEL_IMAP4_TOKEN_ATOM) {
		if (!strcmp ("BYE", token->v.atom)) {
			/* we don't care if we fail here, either way we've been disconnected */
			if (camel_imap4_engine_next_token (engine, token, NULL) == 0) {
				if (token->token == '[') {
					camel_imap4_stream_unget_token (engine->istream, token);
					camel_imap4_engine_parse_resp_code (engine, NULL);
				} else {
					camel_imap4_engine_line (engine, NULL, NULL, NULL);
				}
			}
			
			engine->state = CAMEL_IMAP4_ENGINE_DISCONNECTED;
			
			/* we don't return -1 here because there may be more untagged responses after the BYE */
		} else if (!strcmp ("CAPABILITY", token->v.atom)) {
			/* capability tokens follow */
			if (engine_parse_capability (engine, '\n', ex) == -1)
				return -1;
			
			/* find the eoln token */
			if (camel_imap4_engine_next_token (engine, token, ex) == -1)
				return -1;
			
			if (token->token != '\n') {
				camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
				return -1;
			}
		} else if (!strcmp ("FLAGS", token->v.atom)) {
			/* flags list follows */
			if (engine_parse_flags (engine, ex) == -1)
				return -1;
		} else if (!strcmp ("NAMESPACE", token->v.atom)) {
			if (engine_parse_namespace (engine, ex) == -1)
				return -1;
		} else if (!strcmp ("NO", token->v.atom) || !strcmp ("BAD", token->v.atom)) {
			code = !strcmp ("NO", token->v.atom) ? CAMEL_IMAP4_UNTAGGED_NO : CAMEL_IMAP4_UNTAGGED_BAD;
			
			/* our command has been rejected */
			if (camel_imap4_engine_next_token (engine, token, ex) == -1)
				return -1;
			
			if (token->token == '[') {
				/* we have a resp code */
				camel_imap4_stream_unget_token (engine->istream, token);
				if (camel_imap4_engine_parse_resp_code (engine, ex) == -1)
					return -1;
			} else if (token->token != '\n') {
				/* we just have resp text */
				if (camel_imap4_engine_line (engine, NULL, NULL, ex) == -1)
					return -1;
			}
		} else if (!strcmp ("OK", token->v.atom)) {
			code = CAMEL_IMAP4_UNTAGGED_OK;
			
			if (engine->state == CAMEL_IMAP4_ENGINE_CONNECTED) {
				/* initial server greeting */
				engine->state = CAMEL_IMAP4_ENGINE_PREAUTH;
			}
			
			if (camel_imap4_engine_next_token (engine, token, ex) == -1)
				return -1;
			
			if (token->token == '[') {
				/* we have a resp code */
				camel_imap4_stream_unget_token (engine->istream, token);
				if (camel_imap4_engine_parse_resp_code (engine, ex) == -1)
					return -1;
			} else {
				/* we just have resp text */
				if (camel_imap4_engine_line (engine, NULL, NULL, ex) == -1)
					return -1;
			}
		} else if (!strcmp ("PREAUTH", token->v.atom)) {
			code = CAMEL_IMAP4_UNTAGGED_PREAUTH;
			
			if (engine->state == CAMEL_IMAP4_ENGINE_CONNECTED)
				engine->state = CAMEL_IMAP4_ENGINE_AUTHENTICATED;
			
			if (camel_imap4_engine_parse_resp_code (engine, ex) == -1)
				return -1;
		} else if (ic && (untagged = g_hash_table_lookup (ic->untagged, token->v.atom))) {
			/* registered untagged handler for imap4 command */
			if (untagged (engine, ic, 0, token, ex) == -1)
				return -1;
		} else {
			d(fprintf (stderr, "Unhandled atom token in untagged response: %s", token->v.atom));
			
			if (camel_imap4_engine_eat_line (engine, ex) == -1)
				return -1;
		}
	} else if (token->token == CAMEL_IMAP4_TOKEN_NUMBER) {
		/* we probably have something like "* 1 EXISTS" */
		v = token->v.number;
		
		if (camel_imap4_engine_next_token (engine, token, ex) == -1)
			return -1;
		
		if (token->token != CAMEL_IMAP4_TOKEN_ATOM) {
			camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
			
			return -1;
		}
		
		/* which folder is this EXISTS/EXPUNGE/RECENT acting on? */
		if (engine->current && engine->current->folder)
			folder = (CamelFolder *) engine->current->folder;
		else if (engine->folder)
			folder = (CamelFolder *) engine->folder;
		else
			folder = NULL;
		
		/* NOTE: these can be over-ridden by a registered untagged response handler */
		if (!strcmp ("EXISTS", token->v.atom)) {
			camel_imap4_summary_set_exists (folder->summary, v);
		} else if (!strcmp ("EXPUNGE", token->v.atom)) {
			camel_imap4_summary_expunge (folder->summary, (int) v);
		} else if (!strcmp ("RECENT", token->v.atom)) {
			camel_imap4_summary_set_recent (folder->summary, v);
		} else if (ic && (untagged = g_hash_table_lookup (ic->untagged, token->v.atom))) {
			/* registered untagged handler for imap4 command */
			if (untagged (engine, ic, v, token, ex) == -1)
				return -1;
		} else {
			d(fprintf (stderr, "Unrecognized untagged response: * %u %s\n", v, token->v.atom));
		}
		
		/* find the eoln token */
		if (camel_imap4_engine_eat_line (engine, ex) == -1)
			return -1;
	} else {
		camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
		
		return -1;
	}
	
	return code;
}


void
camel_imap4_engine_handle_untagged (CamelIMAP4Engine *engine, CamelException *ex)
{
	camel_imap4_token_t token;
	
	g_return_if_fail (CAMEL_IS_IMAP4_ENGINE (engine));
	
	do {
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			goto exception;
		
		if (token.token != '*')
			break;
		
		if (camel_imap4_engine_handle_untagged_1 (engine, &token, ex) == -1)
			goto exception;
	} while (1);
	
	camel_imap4_stream_unget_token (engine->istream, &token);
	
	return;
	
 exception:
	
	engine->state = CAMEL_IMAP4_ENGINE_DISCONNECTED;
}


static int
imap4_process_command (CamelIMAP4Engine *engine, CamelIMAP4Command *ic)
{
	int retval;
	
	while ((retval = camel_imap4_command_step (ic)) == 0)
		;
	
	if (retval == -1) {
		engine->state = CAMEL_IMAP4_ENGINE_DISCONNECTED;
		return -1;
	}
	
	return 0;
}


static void
engine_prequeue_folder_select (CamelIMAP4Engine *engine)
{
	CamelIMAP4Command *ic;
	const char *cmd;
	
	ic = (CamelIMAP4Command *) engine->queue.head;
	cmd = (const char *) ic->parts->buffer;
	
	if (!ic->folder || ic->folder == engine->folder ||
	    !strncmp (cmd, "SELECT ", 7) || !strncmp (cmd, "EXAMINE ", 8)) {
		/* no need to pre-queue a SELECT */
		return;
	}
	
	/* we need to pre-queue a SELECT */
	ic = camel_imap4_engine_prequeue (engine, (CamelFolder *) ic->folder, "SELECT %F\r\n", ic->folder);
	ic->user_data = engine;
	
	camel_imap4_command_unref (ic);
}


static int
engine_state_change (CamelIMAP4Engine *engine, CamelIMAP4Command *ic)
{
	const char *cmd;
	int retval = 0;
	
	cmd = ic->parts->buffer;
	if (!strncmp (cmd, "SELECT ", 7) || !strncmp (cmd, "EXAMINE ", 8)) {
		if (ic->result == CAMEL_IMAP4_RESULT_OK) {
			/* Update the selected folder */
			camel_object_ref (ic->folder);
			if (engine->folder)
				camel_object_unref (engine->folder);
			engine->folder = ic->folder;
			
			engine->state = CAMEL_IMAP4_ENGINE_SELECTED;
		} else if (ic->user_data == engine) {
			/* the engine pre-queued this SELECT command */
			retval = -1;
		}
	} else if (!strncmp (cmd, "CLOSE", 5)) {
		if (ic->result == CAMEL_IMAP4_RESULT_OK)
			engine->state = CAMEL_IMAP4_ENGINE_AUTHENTICATED;
	} else if (!strncmp (cmd, "LOGOUT", 6)) {
		engine->state = CAMEL_IMAP4_ENGINE_DISCONNECTED;
	}
	
	return retval;
}

/**
 * camel_imap4_engine_iterate:
 * @engine: IMAP4 engine
 *
 * Processes the first command in the queue.
 *
 * Returns the id of the processed command, 0 if there were no
 * commands to process, or -1 on error.
 *
 * Note: more details on the error will be held on the
 * CamelIMAP4Command that failed.
 **/
int
camel_imap4_engine_iterate (CamelIMAP4Engine *engine)
{
	CamelIMAP4Command *ic, *nic;
	GPtrArray *resp_codes;
	int retval = -1;
	
	if (e_dlist_empty (&engine->queue))
		return 0;
	
	/* This sucks... it would be nicer if we didn't have to check the stream's disconnected status */
	if ((engine->state == CAMEL_IMAP4_ENGINE_DISCONNECTED || engine->istream->disconnected) && !engine->reconnecting) {
		CamelException rex;
		gboolean connected;
		
		camel_exception_init (&rex);
		engine->reconnecting = TRUE;
		connected = engine->reconnect (engine, &rex);
		engine->reconnecting = FALSE;
		
		if (!connected) {
			/* pop the first command and act as tho it failed (which, technically, it did...) */
			ic = (CamelIMAP4Command *) e_dlist_remhead (&engine->queue);
			ic->status = CAMEL_IMAP4_COMMAND_ERROR;
			camel_exception_xfer (&ic->ex, &rex);
			return -1;
		}
	}
	
	/* check to see if we need to pre-queue a SELECT, if so do it */
	engine_prequeue_folder_select (engine);
	
	engine->current = ic = (CamelIMAP4Command *) e_dlist_remhead (&engine->queue);
	ic->status = CAMEL_IMAP4_COMMAND_ACTIVE;
	
	if (imap4_process_command (engine, ic) != -1) {
		if (engine_state_change (engine, ic) == -1) {
			/* This can ONLY happen if @ic was the pre-queued SELECT command
			 * and it got a NO or BAD response.
			 *
			 * We have to pop the next imap4 command or we'll get into an
			 * infinite loop. In order to provide @nic's owner with as much
			 * information as possible, we move all @ic status information
			 * over to @nic and pretend we just processed @nic.
			 **/
			
			nic = (CamelIMAP4Command *) e_dlist_remhead (&engine->queue);
			
			nic->status = ic->status;
			nic->result = ic->result;
			resp_codes = nic->resp_codes;
			nic->resp_codes = ic->resp_codes;
			ic->resp_codes = resp_codes;
			
			camel_exception_xfer (&nic->ex, &ic->ex);
			
			camel_imap4_command_unref (ic);
			ic = nic;
		}
		
		retval = ic->id;
	}
	
	camel_imap4_command_unref (ic);
	
	return retval;
}


/**
 * camel_imap4_engine_queue:
 * @engine: IMAP4 engine
 * @folder: IMAP4 folder that the command will affect (or %NULL if it doesn't matter)
 * @format: command format
 * @Varargs: arguments
 *
 * Basically the same as camel_imap4_command_new() except that this
 * function also places the command in the engine queue.
 *
 * Returns the CamelIMAP4Command.
 **/
CamelIMAP4Command *
camel_imap4_engine_queue (CamelIMAP4Engine *engine, CamelFolder *folder, const char *format, ...)
{
	CamelIMAP4Command *ic;
	va_list args;
	
	g_return_val_if_fail (CAMEL_IS_IMAP4_ENGINE (engine), NULL);
	
	va_start (args, format);
	ic = camel_imap4_command_newv (engine, (CamelIMAP4Folder *) folder, format, args);
	va_end (args);
	
	ic->id = engine->nextid++;
	e_dlist_addtail (&engine->queue, (EDListNode *) ic);
	camel_imap4_command_ref (ic);
	
	return ic;
}


/**
 * camel_imap4_engine_prequeue:
 * @engine: IMAP4 engine
 * @folder: IMAP4 folder that the command will affect (or %NULL if it doesn't matter)
 * @format: command format
 * @Varargs: arguments
 *
 * Same as camel_imap4_engine_queue() except this places the new
 * command at the head of the queue.
 *
 * Returns the CamelIMAP4Command.
 **/
CamelIMAP4Command *
camel_imap4_engine_prequeue (CamelIMAP4Engine *engine, CamelFolder *folder, const char *format, ...)
{
	CamelIMAP4Command *ic;
	va_list args;
	
	g_return_val_if_fail (CAMEL_IS_IMAP4_ENGINE (engine), NULL);
	
	va_start (args, format);
	ic = camel_imap4_command_newv (engine, (CamelIMAP4Folder *) folder, format, args);
	va_end (args);
	
	if (e_dlist_empty (&engine->queue)) {
		e_dlist_addtail (&engine->queue, (EDListNode *) ic);
		ic->id = engine->nextid++;
	} else {
		CamelIMAP4Command *nic;
		EDListNode *node;
		
		node = (EDListNode *) ic;
		e_dlist_addhead (&engine->queue, node);
		nic = (CamelIMAP4Command *) node->next;
		ic->id = nic->id - 1;
		
		if (ic->id == 0) {
			/* increment all command ids */
			node = engine->queue.head;
			while (node->next) {
				nic = (CamelIMAP4Command *) node;
				node = node->next;
				nic->id++;
			}
		}
	}
	
	camel_imap4_command_ref (ic);
	
	return ic;
}


void
camel_imap4_engine_dequeue (CamelIMAP4Engine *engine, CamelIMAP4Command *ic)
{
	EDListNode *node = (EDListNode *) ic;
	
	if (node->next == NULL && node->prev == NULL)
		return;
	
	e_dlist_remove (node);
	node->next = NULL;
	node->prev = NULL;
	
	camel_imap4_command_unref (ic);
}


int
camel_imap4_engine_next_token (CamelIMAP4Engine *engine, camel_imap4_token_t *token, CamelException *ex)
{
	if (camel_imap4_stream_next_token (engine->istream, token) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("IMAP4 server %s unexpectedly disconnected: %s"),
				      engine->url->host, errno ? g_strerror (errno) : _("Unknown"));
		
		engine->state = CAMEL_IMAP4_ENGINE_DISCONNECTED;
		
		return -1;
	}
	
	return 0;
}


int
camel_imap4_engine_eat_line (CamelIMAP4Engine *engine, CamelException *ex)
{
	camel_imap4_token_t token;
	unsigned char *literal;
	int retval;
	size_t n;
	
	do {
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			return -1;
		
		if (token.token == CAMEL_IMAP4_TOKEN_LITERAL) {
			while ((retval = camel_imap4_stream_literal (engine->istream, &literal, &n)) == 1)
				;
			
			if (retval == -1) {
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      _("IMAP4 server %s unexpectedly disconnected: %s"),
						      engine->url->host, errno ? g_strerror (errno) : _("Unknown"));
				
				engine->state = CAMEL_IMAP4_ENGINE_DISCONNECTED;
				
				return -1;
			}
		}
	} while (token.token != '\n');
	
	return 0;
}


int
camel_imap4_engine_line (CamelIMAP4Engine *engine, unsigned char **line, size_t *len, CamelException *ex)
{
	GByteArray *linebuf = NULL;
	unsigned char *buf;
	size_t buflen;
	int retval;
	
	if (line != NULL)
		linebuf = g_byte_array_new ();
	
	while ((retval = camel_imap4_stream_line (engine->istream, &buf, &buflen)) > 0) {
		if (linebuf != NULL)
			g_byte_array_append (linebuf, buf, buflen);
	}
	
	if (retval == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("IMAP4 server %s unexpectedly disconnected: %s"),
				      engine->url->host, errno ? g_strerror (errno) : _("Unknown"));
		
		if (linebuf != NULL)
			g_byte_array_free (linebuf, TRUE);
		
		engine->state = CAMEL_IMAP4_ENGINE_DISCONNECTED;
		
		return -1;
	}
	
	if (linebuf != NULL) {
		g_byte_array_append (linebuf, buf, buflen);
		
		*line = linebuf->data;
		*len = linebuf->len;
		
		g_byte_array_free (linebuf, FALSE);
	}
	
	return 0;
}


int
camel_imap4_engine_literal (CamelIMAP4Engine *engine, unsigned char **literal, size_t *len, CamelException *ex)
{
	GByteArray *literalbuf = NULL;
	unsigned char *buf;
	size_t buflen;
	int retval;
	
	if (literal != NULL)
		literalbuf = g_byte_array_new ();
	
	while ((retval = camel_imap4_stream_literal (engine->istream, &buf, &buflen)) > 0) {
		if (literalbuf != NULL)
			g_byte_array_append (literalbuf, buf, buflen);
	}
	
	if (retval == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("IMAP4 server %s unexpectedly disconnected: %s"),
				      engine->url->host, errno ? g_strerror (errno) : _("Unknown"));
		
		if (literalbuf != NULL)
			g_byte_array_free (literalbuf, TRUE);
		
		engine->state = CAMEL_IMAP4_ENGINE_DISCONNECTED;
		
		return -1;
	}
	
	if (literalbuf != NULL) {
		g_byte_array_append (literalbuf, buf, buflen);
		g_byte_array_append (literalbuf, "", 1);
		
		*literal = literalbuf->data;
		*len = literalbuf->len - 1;
		
		g_byte_array_free (literalbuf, FALSE);
	}
	
	return 0;
}


void
camel_imap4_resp_code_free (CamelIMAP4RespCode *rcode)
{
	switch (rcode->code) {
	case CAMEL_IMAP4_RESP_CODE_PARSE:
		g_free (rcode->v.parse);
		break;
	case CAMEL_IMAP4_RESP_CODE_NEWNAME:
		g_free (rcode->v.newname[0]);
		g_free (rcode->v.newname[1]);
		break;
	case CAMEL_IMAP4_RESP_CODE_COPYUID:
		g_free (rcode->v.copyuid.srcset);
		g_free (rcode->v.copyuid.destset);
		break;
	default:
		break;
	}
	
	g_free (rcode);
}
