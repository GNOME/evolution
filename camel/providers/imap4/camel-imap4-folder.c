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

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <camel/camel-utf8.h>
#include <camel/camel-private.h>
#include <camel/camel-file-utils.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter-crlf.h>

#include "camel-imap4-utils.h"
#include "camel-imap4-store.h"
#include "camel-imap4-engine.h"
#include "camel-imap4-folder.h"
#include "camel-imap4-stream.h"
#include "camel-imap4-command.h"
#include "camel-imap4-summary.h"

#define d(x) x

static void camel_imap4_folder_class_init (CamelIMAP4FolderClass *klass);
static void camel_imap4_folder_init (CamelIMAP4Folder *folder, CamelIMAP4FolderClass *klass);
static void camel_imap4_folder_finalize (CamelObject *object);

static void imap4_sync (CamelFolder *folder, gboolean expunge, CamelException *ex);
static void imap4_expunge (CamelFolder *folder, CamelException *ex);
static CamelMimeMessage *imap4_get_message (CamelFolder *folder, const char *uid, CamelException *ex);
static void imap4_append_message (CamelFolder *folder, CamelMimeMessage *message,
				  const CamelMessageInfo *info, char **appended_uid, CamelException *ex);
static void imap4_transfer_messages_to (CamelFolder *src, GPtrArray *uids, CamelFolder *dest,
					GPtrArray **transferred_uids, gboolean delete_originals, CamelException *ex);


static CamelFolderClass *parent_class = NULL;


CamelType
camel_imap4_folder_get_type (void)
{
	static CamelType type = 0;
	
	if (!type) {
		type = camel_type_register (CAMEL_FOLDER_TYPE,
					    "CamelIMAP4Folder",
					    sizeof (CamelIMAP4Folder),
					    sizeof (CamelIMAP4FolderClass),
					    (CamelObjectClassInitFunc) camel_imap4_folder_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_imap4_folder_init,
					    (CamelObjectFinalizeFunc) camel_imap4_folder_finalize);
	}
	
	return type;
}

static void
camel_imap4_folder_class_init (CamelIMAP4FolderClass *klass)
{
	CamelFolderClass *folder_class = (CamelFolderClass *) klass;
	
	parent_class = (CamelFolderClass *) camel_type_get_global_classfuncs (CAMEL_FOLDER_TYPE);
	
	folder_class->sync = imap4_sync;
	folder_class->expunge = imap4_expunge;
	folder_class->get_message = imap4_get_message;
	folder_class->append_message = imap4_append_message;
	folder_class->transfer_messages_to = imap4_transfer_messages_to;
}

static void
camel_imap4_folder_init (CamelIMAP4Folder *folder, CamelIMAP4FolderClass *klass)
{
	folder->utf7_name = NULL;
}

static void
camel_imap4_folder_finalize (CamelObject *object)
{
	CamelIMAP4Folder *folder = (CamelIMAP4Folder *) object;
	
	g_free (folder->utf7_name);
}

static char
imap4_get_path_delim (CamelIMAP4Engine *engine, const char *full_name)
{
	CamelIMAP4Namespace *namespace;
	const char *slash;
	size_t len;
	char *top;
	
	if ((slash = strchr (full_name, '/')))
		len = (slash - full_name);
	else
		len = strlen (full_name);
	
	top = g_alloca (len + 1);
	memcpy (top, full_name, len);
	top[len] = '\0';
	
	if (!g_ascii_strcasecmp (top, "INBOX"))
		top = "INBOX";
	
 retry:
	namespace = engine->namespaces.personal;
	while (namespace != NULL) {
		if (!strcmp (namespace->path, top))
			return namespace->sep;
		namespace = namespace->next;
	}
	
	namespace = engine->namespaces.other;
	while (namespace != NULL) {
		if (!strcmp (namespace->path, top))
			return namespace->sep;
		namespace = namespace->next;
	}
	
	namespace = engine->namespaces.shared;
	while (namespace != NULL) {
		if (!strcmp (namespace->path, top))
			return namespace->sep;
		namespace = namespace->next;
	}
	
	if (top[0] != '\0') {
		/* look for a default namespace? */
		top[0] = '\0';
		goto retry;
	}
	
	return '/';
}

CamelFolder *
camel_imap4_folder_new (CamelStore *store, const char *full_name, CamelException *ex)
{
	CamelIMAP4Folder *imap_folder;
	char *utf7_name, *name, *p;
	char sep;
	
	if (!(p = strrchr (full_name, '/')))
		p = (char *) full_name;
	else
		p++;
	
	name = g_alloca (strlen (p) + 1);
	strcpy (name, p);
	
	utf7_name = g_alloca (strlen (full_name) + 1);
	strcpy (utf7_name, full_name);
	
	sep = imap4_get_path_delim (((CamelIMAP4Store *) store)->engine, full_name);
	if (sep != '/') {
		p = utf7_name;
		while (*p != '\0') {
			if (*p == '/')
				*p = sep;
			p++;
		}
	}
	
	utf7_name = camel_utf8_utf7 (utf7_name);
	
	imap_folder = (CamelIMAP4Folder *) camel_object_new (CAMEL_TYPE_IMAP4_FOLDER);
	camel_folder_construct ((CamelFolder *) imap_folder, store, full_name, name);
	imap_folder->utf7_name = utf7_name;
	
	return (CamelFolder *) imap_folder;
}


const char *
camel_imap4_folder_utf7_name (CamelIMAP4Folder *folder)
{
	return folder->utf7_name;
}


static struct {
	const char *name;
	guint32 flag;
} imap4_flags[] = {
	{ "\\Answered", CAMEL_MESSAGE_ANSWERED  },
	{ "\\Deleted",  CAMEL_MESSAGE_DELETED   },
	{ "\\Draft",    CAMEL_MESSAGE_DRAFT     },
	{ "\\Flagged",  CAMEL_MESSAGE_FLAGGED   },
	/*{ "Forwarded",  CAMEL_MESSAGE_FORWARDED },*/
	{ "\\Seen",     CAMEL_MESSAGE_SEEN      },
};

static int
imap4_get_uid_set (CamelIMAP4Engine *engine, CamelFolderSummary *summary, GPtrArray *infos, int cur, size_t linelen, char **set)
{
	CamelMessageInfo *info;
	guint32 this, prev, next;
	gboolean range = FALSE;
	GString *uidset;
	int scount, i;
	size_t len;
	
	if (engine->maxlentype == CAMEL_IMAP4_ENGINE_MAXLEN_LINE)
		len = engine->maxlen - linelen;
	else
		len = engine->maxlen;
	
	i = cur + 1;
	info = (CamelMessageInfo *) infos->pdata[cur];
	fflush (stdout);
	uidset = g_string_new ("");
	g_string_append (uidset, camel_message_info_uid (info));
	
	if (!(i < infos->len))
		goto done;
	
	scount = summary->messages->len;
	
	/* init this info */
	for (this = 0; this < scount; this++) {
		if (info == summary->messages->pdata[this])
			break;
	}
	
	/* init next info */
	info = (CamelMessageInfo *) infos->pdata[i];
	for (next = this; next < scount; next++) {
		if (info == summary->messages->pdata[next])
			break;
	}
	
	for ( ; i < infos->len && uidset->len < len; i++) {
		prev = this;
		this = next;
		
		if (i + 1 < infos->len) {
			info = infos->pdata[i + 1];
			for (next = this; next < scount; next++) {
				if (info == summary->messages->pdata[next])
					break;
			}
		} else {
			next = scount;
		}
		
		if (this == (next - 1) || this == (prev + 1)) {
			range = TRUE;
		} else {
			if (range) {
				info = (CamelMessageInfo *) summary->messages->pdata[prev];
				g_string_append_printf (uidset, ":%s", camel_message_info_uid (info));
				range = FALSE;
			}
			
			info = infos->pdata[i];
			g_string_append_printf (uidset, ",%s", camel_message_info_uid (info));
		}
	}
	
	if (range) {
		info = (CamelMessageInfo *) summary->messages->pdata[this];
		g_string_append_printf (uidset, ":%s", camel_message_info_uid (info));
	}
	
 done:
	
	*set = uidset->str;
	g_string_free (uidset, FALSE);
	
	return (i - cur);
}

static int
imap4_sync_flag (CamelFolder *folder, GPtrArray *infos, char onoff, const char *flag, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) folder->parent_store)->engine;
	CamelIMAP4Command *ic;
	int i, id, retval = 0;
	char *set = NULL;
	
	for (i = 0; i < infos->len; ) {
		i += imap4_get_uid_set (engine, folder->summary, infos, i, 30 + strlen (flag), &set);
		
		ic = camel_imap4_engine_queue (engine, folder, "UID STORE %s %cFLAGS.SILENT (%s)\r\n", set, onoff, flag);
		while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
			;
		
		g_free (set);
		
		if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
			camel_exception_xfer (ex, &ic->ex);
			camel_imap4_command_unref (ic);
			
			return -1;
		}
		
		switch (ic->result) {
		case CAMEL_IMAP4_RESULT_NO:
			/* FIXME: would be good to save the NO reason into the err message */
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Cannot sync flags to folder `%s': Unknown"),
					      folder->full_name);
			retval = -1;
			break;
		case CAMEL_IMAP4_RESULT_BAD:
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Cannot sync flags to folder `%s': Bad command"),
					      folder->full_name);
			retval = -1;
			break;
		}
		
		camel_imap4_command_unref (ic);
		
		if (retval == -1)
			return -1;
	}
	
	return 0;
}

static int
imap4_sync_changes (CamelFolder *folder, GPtrArray *sync, CamelException *ex)
{
	CamelIMAP4MessageInfo *iinfo;
	GPtrArray *on_set, *off_set;
	CamelMessageInfo *info;
	flags_diff_t diff;
	int retval = 0;
	int i, j;
	
	if (folder->permanent_flags == 0)
		return 0;
	
	on_set = g_ptr_array_new ();
	off_set = g_ptr_array_new ();
	
	/* construct commands to sync system and user flags */
	for (i = 0; i < G_N_ELEMENTS (imap4_flags); i++) {
		if (!(imap4_flags[i].flag & folder->permanent_flags))
			continue;
		
		for (j = 0; j < sync->len; j++) {
			iinfo = (CamelIMAP4MessageInfo *) info = sync->pdata[j];
			camel_imap4_flags_diff (&diff, iinfo->server_flags, info->flags);
			if (diff.changed & imap4_flags[i].flag) {
				if (diff.bits & imap4_flags[i].flag) {
					g_ptr_array_add (on_set, info);
				} else {
					g_ptr_array_add (off_set, info);
				}
			}
		}
		
		if (on_set->len > 0) {
			if ((retval = imap4_sync_flag (folder, on_set, '+', imap4_flags[i].name, ex)) == -1)
				break;
			
			g_ptr_array_set_size (on_set, 0);
		}
		
		if (off_set->len > 0) {
			if ((retval = imap4_sync_flag (folder, off_set, '-', imap4_flags[i].name, ex)) == -1)
				break;
			
			g_ptr_array_set_size (off_set, 0);
		}
	}
	
	g_ptr_array_free (on_set, TRUE);
	g_ptr_array_free (off_set, TRUE);
	
	if (retval == -1)
		return-1;
	
	for (i = 0; i < sync->len; i++) {
		iinfo = (CamelIMAP4MessageInfo *) info = sync->pdata[i];
		info->flags &= ~CAMEL_MESSAGE_FOLDER_FLAGGED;
		iinfo->server_flags = info->flags & folder->permanent_flags;
	}
	
	return 0;
}

static void
imap4_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) folder->parent_store)->engine;
	CamelIMAP4MessageInfo *iinfo;
	CamelMessageInfo *info;
	CamelIMAP4Command *ic;
	flags_diff_t diff;
	GPtrArray *sync;
	int id, max, i;
	int retval;
	
	CAMEL_SERVICE_LOCK (folder->parent_store, connect_lock);
	
	/* gather a list of changes to sync to the server */
	sync = g_ptr_array_new ();
	max = camel_folder_summary_count (folder->summary);
	for (i = 0; i < max; i++) {
		iinfo = (CamelIMAP4MessageInfo *) info = camel_folder_summary_index (folder->summary, i);
		expunge = expunge && (info->flags & CAMEL_MESSAGE_DELETED);
		if (info->flags & CAMEL_MESSAGE_FOLDER_FLAGGED) {
			camel_imap4_flags_diff (&diff, iinfo->server_flags, info->flags);
			diff.changed &= folder->permanent_flags;
			
			/* weed out flag changes that we can't sync to the server */
			if (!diff.changed)
				camel_folder_summary_info_free (folder->summary, info);
			else
				g_ptr_array_add (sync, info);
		} else {
			camel_folder_summary_info_free (folder->summary, info);
		}
	}
	
	if (sync->len > 0) {
		retval = imap4_sync_changes (folder, sync, ex);
		
		for (i = 0; i < sync->len; i++)
			camel_folder_summary_info_free (folder->summary, sync->pdata[i]);
		
		g_ptr_array_free (sync, TRUE);
		
		if (retval == -1)
			goto done;
	} else {
		g_ptr_array_free (sync, TRUE);
	}
	
	if (expunge) {
		ic = camel_imap4_engine_queue (engine, folder, "EXPUNGE\r\n");
		while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
			;
		
		switch (ic->result) {
		case CAMEL_IMAP4_RESULT_NO:
			/* FIXME: would be good to save the NO reason into the err message */
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Cannot expunge folder `%s': Unknown"),
					      folder->full_name);
			break;
		case CAMEL_IMAP4_RESULT_BAD:
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Cannot expunge folder `%s': Bad command"),
					      folder->full_name);
			break;
		}
		
		camel_imap4_command_unref (ic);
	}
	
	camel_folder_summary_save (folder->summary);
	
 done:
	
	CAMEL_SERVICE_UNLOCK (folder->parent_store, connect_lock);
}

static void
imap4_expunge (CamelFolder *folder, CamelException *ex)
{
	imap4_sync (folder, TRUE, ex);
}


static int
untagged_fetch (CamelIMAP4Engine *engine, CamelIMAP4Command *ic, guint32 index, camel_imap4_token_t *token, CamelException *ex)
{
	CamelStream *fstream, *stream = ic->user_data;
	CamelMimeFilter *crlf;
	int left = 2;
	
	if (camel_imap4_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	/* parse the FETCH response list */
	if (token->token != '(') {
		camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
		return -1;
	}
	
	do {
		if (camel_imap4_engine_next_token (engine, token, ex) == -1)
			goto exception;
		
		if (token->token != CAMEL_IMAP4_TOKEN_ATOM)
			goto unexpected;
		
		if (!strcmp (token->v.atom, "BODY[")) {
			if (camel_imap4_engine_next_token (engine, token, ex) == -1)
				goto exception;
			
			if (token->token != ']')
				goto unexpected;
			
			if (camel_imap4_engine_next_token (engine, token, ex) == -1)
				goto exception;
			
			if (token->token != CAMEL_IMAP4_TOKEN_LITERAL)
				goto unexpected;
			
			fstream = (CamelStream *) camel_stream_filter_new_with_stream (stream);
			crlf = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_DECODE, CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
			camel_stream_filter_add ((CamelStreamFilter *) fstream, crlf);
			camel_object_unref (crlf);
			
			camel_stream_write_to_stream ((CamelStream *) engine->istream, fstream);
			camel_stream_flush (fstream);
			camel_object_unref (fstream);
			
			left--;
		} else if (!strcmp (token->v.atom, "UID")) {
			if (camel_imap4_engine_next_token (engine, token, ex) == -1)
				goto exception;
			
			if (token->token != CAMEL_IMAP4_TOKEN_NUMBER || token->v.number == 0)
				goto unexpected;
			
			left--;
		} else {
			/* wtf? */
			fprintf (stderr, "huh? %s?...\n", token->v.atom);
		}
	} while (left);
	
	if (camel_imap4_engine_next_token (engine, token, ex) == -1)
		goto exception;
	
	if (token->token != ')') {
		fprintf (stderr, "expected ')' to close untagged FETCH response\n");
		goto unexpected;
	}
	
	return 0;
	
 unexpected:
	
	camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
	
 exception:
	
	return -1;
}

static CamelMimeMessage *
imap4_get_message (CamelFolder *folder, const char *uid, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) folder->parent_store)->engine;
	CamelMimeMessage *message = NULL;
	CamelIMAP4Command *ic;
	CamelStream *stream;
	int id;
	
	CAMEL_SERVICE_LOCK (folder->parent_store, connect_lock);
	
	ic = camel_imap4_engine_queue (engine, folder, "UID FETCH %s BODY.PEEK[]\r\n", uid);
	camel_imap4_command_register_untagged (ic, "FETCH", untagged_fetch);
	ic->user_data = stream = camel_stream_mem_new ();
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		camel_object_unref (stream);
		goto done;
	}
	
	switch (ic->result) {
	case CAMEL_IMAP4_RESULT_OK:
		camel_stream_reset (stream);
		message = camel_mime_message_new ();
		camel_data_wrapper_construct_from_stream ((CamelDataWrapper *) message, stream);
		break;
	case CAMEL_IMAP4_RESULT_NO:
		/* FIXME: would be good to save the NO reason into the err message */
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot get message %s from folder `%s': No such message"),
				      uid, folder->full_name);
		break;
	case CAMEL_IMAP4_RESULT_BAD:
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot get message %s from folder `%s': Bad command"),
				      uid, folder->full_name);
		break;
	}
	
	camel_imap4_command_unref (ic);
	
	camel_object_unref (stream);
	
 done:
	
	CAMEL_SERVICE_UNLOCK (folder->parent_store, connect_lock);
	
	return message;
}

static char *tm_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static void
imap4_append_message (CamelFolder *folder, CamelMimeMessage *message,
		     const CamelMessageInfo *info, char **appended_uid, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) folder->parent_store)->engine;
	CamelIMAP4Summary *summary = (CamelIMAP4Summary *) folder->summary;
	CamelIMAP4RespCode *resp;
	CamelIMAP4Command *ic;
	CamelFolderInfo *fi;
	CamelException lex;
	char flags[100], *p;
	char date[50];
	struct tm tm;
	int id, i;
	
	CAMEL_SERVICE_LOCK (folder->parent_store, connect_lock);
	
	/* construct the option flags list */
	if (info->flags & folder->permanent_flags) {
		p = g_stpcpy (flags, " (");
		
		for (i = 0; i < G_N_ELEMENTS (imap4_flags); i++) {
			if ((info->flags & imap4_flags[i].flag) & folder->permanent_flags) {
				p = g_stpcpy (p, imap4_flags[i].name);
				*p++ = ' ';
			}
		}
		
		p[-1] = ')';
		*p = '\0';
	} else {
		flags[0] = '\0';
	}
	
	/* construct the optional date_time string */
	if (info->date_received != (time_t) -1) {
		int tzone;
		
#ifdef HAVE_LOCALTIME_R
		localtime_r (&info->date_received, &tm);
#else
		memcpy (&tm, localtime (&info->date_received), sizeof (tm));
#endif
		
#if defined (HAVE_TM_GMTOFF)
		tzone = -tm.tm_gmtoff;
#elif defined (HAVE_TIMEZONE)
		if (tm.tm_isdst > 0) {
#if defined (HAVE_ALTZONE)
			tzone = altzone;
#else /* !defined (HAVE_ALTZONE) */
			tzone = (timezone - 3600);
#endif
		} else {
			tzone = timezone;
		}
#else
#error Neither HAVE_TIMEZONE nor HAVE_TM_GMTOFF defined. Rerun autoheader, autoconf, etc.
#endif
		
		sprintf (date, " \"%02d-%s-%04d %02d:%02d:%02d %+05d\"",
			 tm.tm_mday, tm_months[tm.tm_mon], tm.tm_year + 1900,
			 tm.tm_hour, tm.tm_min, tm.tm_sec, tzone);
	} else {
		date[0] = '\0';
	}
	
 retry:
	
	if (engine->capa & CAMEL_IMAP4_CAPABILITY_UIDPLUS)
		ic = camel_imap4_engine_queue (engine, NULL, "UID APPEND %F%s%s %L\r\n", flags, date, message);
	else
		ic = camel_imap4_engine_queue (engine, NULL, "APPEND %F%s%s %L\r\n", flags, date, message);
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		CAMEL_SERVICE_UNLOCK (folder->parent_store, connect_lock);
		return;
	}
	
	switch (ic->result) {
	case CAMEL_IMAP4_RESULT_OK:
		if (!(engine->capa & CAMEL_IMAP4_CAPABILITY_UIDPLUS))
			break;
		
		if (!appended_uid)
			break;
		
		for (i = 0; i < ic->resp_codes->len; i++) {
			resp = ic->resp_codes->pdata[i];
			if (resp->code == CAMEL_IMAP4_RESP_CODE_APPENDUID) {
				if (resp->v.appenduid.uidvalidity == summary->uidvalidity)
					*appended_uid = g_strdup_printf ("%u", resp->v.appenduid.uid);
				break;
			}
		}
		break;
	case CAMEL_IMAP4_RESULT_NO:
		/* FIXME: can we give the user any more information? */
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot append message to folder `%s': Unknown error"),
				      folder->full_name);
		
		for (i = 0; i < ic->resp_codes->len; i++) {
			resp = ic->resp_codes->pdata[i];
			if (resp->code == CAMEL_IMAP4_RESP_CODE_TRYCREATE) {
				char *parent_name, *p;
				
				parent_name = g_alloca (strlen (folder->full_name) + 1);
				if (!(p = strrchr (parent_name, '/')))
					*parent_name = '\0';
				else
					*p = '\0';
				
				if (!(fi = camel_store_create_folder (folder->parent_store, parent_name, folder->name, &lex))) {
					camel_exception_clear (&lex);
					break;
				}
				
				camel_store_free_folder_info (folder->parent_store, fi);
				camel_imap4_command_unref (ic);
				camel_exception_clear (ex);
				goto retry;
			}
		}
		
		break;
	case CAMEL_IMAP4_RESULT_BAD:
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot append message to folder `%s': Bad command"),
				      folder->full_name);
		
		break;
	default:
		g_assert_not_reached ();
	}
	
	camel_imap4_command_unref (ic);
	
	CAMEL_SERVICE_UNLOCK (folder->parent_store, connect_lock);
}


static int
info_uid_sort (const CamelMessageInfo **info0, const CamelMessageInfo **info1)
{
	guint32 uid0, uid1;
	
	uid0 = strtoul (camel_message_info_uid (*info0), NULL, 10);
	uid1 = strtoul (camel_message_info_uid (*info1), NULL, 10);
	
	if (uid0 == uid1)
		return 0;
	
	return uid0 < uid1 ? -1 : 1;
}

static void
imap4_transfer_messages_to (CamelFolder *src, GPtrArray *uids, CamelFolder *dest,
			   GPtrArray **transferred_uids, gboolean delete_originals, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) src->parent_store)->engine;
	int i, j, n, id, dest_namelen;
	CamelMessageInfo *info;
	CamelIMAP4Command *ic;
	GPtrArray *infos;
	char *set;
	
	infos = g_ptr_array_new ();
	for (i = 0; i < uids->len; i++) {
		if (!(info = camel_folder_summary_uid (src->summary, uids->pdata[i])))
			continue;
		
		g_ptr_array_add (infos, info);
	}
	
	if (infos->len == 0) {
		g_ptr_array_free (infos, TRUE);
		return;
	}
	
	g_ptr_array_sort (infos, (GCompareFunc) info_uid_sort);
	
	CAMEL_SERVICE_LOCK (src->parent_store, connect_lock);
	
	dest_namelen = strlen (camel_imap4_folder_utf7_name ((CamelIMAP4Folder *) dest));
	
	for (i = 0; i < infos->len; i += n) {
		n = imap4_get_uid_set (engine, src->summary, infos, i, 10 + dest_namelen, &set);
		
		ic = camel_imap4_engine_queue (engine, src, "UID COPY %s %F\r\n", set, dest);
		while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
			;
		
		g_free (set);
		
		if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
			camel_exception_xfer (ex, &ic->ex);
			camel_imap4_command_unref (ic);
			g_free (set);
			goto done;
		}
		
		switch (ic->result) {
		case CAMEL_IMAP4_RESULT_NO:
			/* FIXME: would be good to save the NO reason into the err message */
			if (delete_originals) {
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      _("Cannot move messages from folder `%s' to folder `%s': Unknown"),
						      src->full_name, dest->full_name);
			} else {
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      _("Cannot copy messages from folder `%s' to folder `%s': Unknown"),
						      src->full_name, dest->full_name);
			}
			
			goto done;
		case CAMEL_IMAP4_RESULT_BAD:
			if (delete_originals) {
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      _("Cannot move messages from folder `%s' to folder `%s': Bad command"),
						      src->full_name, dest->full_name);
			} else {
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      _("Cannot copy messages from folder `%s' to folder `%s': Bad command"),
						      src->full_name, dest->full_name);
			}
			
			goto done;
		}
		
		camel_imap4_command_unref (ic);
		
		if (delete_originals) {
			for (j = i; j < n; j++) {
				info = infos->pdata[j];
				camel_folder_set_message_flags (src, camel_message_info_uid (info),
								CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);
			}
			
			camel_folder_summary_touch (src->summary);
		}
	}
	
 done:
	
	for (i = 0; i < infos->len; i++)
		camel_folder_summary_info_free (src->summary, infos->pdata[i]);
	g_ptr_array_free (infos, TRUE);
	
	CAMEL_SERVICE_LOCK (src->parent_store, connect_lock);
}
