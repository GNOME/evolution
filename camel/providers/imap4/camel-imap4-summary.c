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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <utime.h>
#include <fcntl.h>
#include <ctype.h>

#include <e-util/md5-utils.h>

#include <camel/camel-file-utils.h>

#include "camel-imap4-store.h"
#include "camel-imap4-engine.h"
#include "camel-imap4-folder.h"
#include "camel-imap4-stream.h"
#include "camel-imap4-command.h"
#include "camel-imap4-utils.h"

#include "camel-imap4-summary.h"

#define IMAP4_SUMMARY_VERSION  1

static void camel_imap4_summary_class_init (CamelIMAP4SummaryClass *klass);
static void camel_imap4_summary_init (CamelIMAP4Summary *summary, CamelIMAP4SummaryClass *klass);
static void camel_imap4_summary_finalize (CamelObject *object);

static int imap4_header_load (CamelFolderSummary *summary, FILE *fin);
static int imap4_header_save (CamelFolderSummary *summary, FILE *fout);
static CamelMessageInfo *imap4_message_info_new (CamelFolderSummary *summary, struct _camel_header_raw *header);
static CamelMessageInfo *imap4_message_info_load (CamelFolderSummary *summary, FILE *fin);
static int imap4_message_info_save (CamelFolderSummary *summary, FILE *fout, CamelMessageInfo *info);


static CamelFolderSummaryClass *parent_class = NULL;


CamelType
camel_imap4_summary_get_type (void)
{
	static CamelType type = 0;
	
	if (!type) {
		type = camel_type_register (CAMEL_FOLDER_SUMMARY_TYPE,
					    "CamelIMAP4Summary",
					    sizeof (CamelIMAP4Summary),
					    sizeof (CamelIMAP4SummaryClass),
					    (CamelObjectClassInitFunc) camel_imap4_summary_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_imap4_summary_init,
					    (CamelObjectFinalizeFunc) camel_imap4_summary_finalize);
	}
	
	return type;
}


static void
camel_imap4_summary_class_init (CamelIMAP4SummaryClass *klass)
{
	CamelFolderSummaryClass *summary_class = (CamelFolderSummaryClass *) klass;
	
	parent_class = (CamelFolderSummaryClass *) camel_type_get_global_classfuncs (camel_folder_summary_get_type ());
	
	summary_class->summary_header_load = imap4_header_load;
	summary_class->summary_header_save = imap4_header_save;
	summary_class->message_info_new = imap4_message_info_new;
	summary_class->message_info_load = imap4_message_info_load;
	summary_class->message_info_save = imap4_message_info_save;
}

static void
camel_imap4_summary_init (CamelIMAP4Summary *summary, CamelIMAP4SummaryClass *klass)
{
	CamelFolderSummary *folder_summary = (CamelFolderSummary *) summary;
	
	folder_summary->version += IMAP4_SUMMARY_VERSION;
	folder_summary->flags = CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_DELETED |
		CAMEL_MESSAGE_DRAFT | CAMEL_MESSAGE_FLAGGED | CAMEL_MESSAGE_SEEN;
	
	folder_summary->message_info_size = sizeof (CamelIMAP4MessageInfo);
	
	summary->update_flags = TRUE;
	summary->uidvalidity_changed = FALSE;
}

static void
camel_imap4_summary_finalize (CamelObject *object)
{
	;
}


CamelFolderSummary *
camel_imap4_summary_new (CamelFolder *folder)
{
	CamelFolderSummary *summary;
	
	summary = (CamelFolderSummary *) camel_object_new (CAMEL_TYPE_IMAP4_SUMMARY);
	((CamelIMAP4Summary *) summary)->folder = folder;
	
	return summary;
}

static int
imap4_header_load (CamelFolderSummary *summary, FILE *fin)
{
	CamelIMAP4Summary *imap4_summary = (CamelIMAP4Summary *) summary;
	
	if (CAMEL_FOLDER_SUMMARY_CLASS (parent_class)->summary_header_load (summary, fin) == -1)
		return -1;
	
	if (camel_file_util_decode_uint32 (fin, &imap4_summary->uidvalidity) == -1)
		return -1;
	
	return 0;
}

static int
imap4_header_save (CamelFolderSummary *summary, FILE *fout)
{
	CamelIMAP4Summary *imap4_summary = (CamelIMAP4Summary *) summary;
	
	if (CAMEL_FOLDER_SUMMARY_CLASS (parent_class)->summary_header_save (summary, fout) == -1)
		return -1;
	
	if (camel_file_util_encode_uint32 (fout, imap4_summary->uidvalidity) == -1)
		return -1;
	
	return 0;
}

static int
envelope_decode_address (CamelIMAP4Engine *engine, GString *addrs, CamelException *ex)
{
	char *addr, *name = NULL, *user = NULL;
	struct _camel_header_address *cia;
	camel_imap4_token_t token;
	const char *domain = NULL;
	int part = 0;
	
	if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	if (token.token == CAMEL_IMAP4_TOKEN_NIL) {
		return 0;
	} else if (token.token != '(') {
		camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	if (addrs->len > 0)
		g_string_append (addrs, ", ");
	
	do {
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
			return -1;
		
		switch (token.token) {
		case CAMEL_IMAP4_TOKEN_NIL:
			break;
		case CAMEL_IMAP4_TOKEN_ATOM:
		case CAMEL_IMAP4_TOKEN_QSTRING:
			switch (part) {
			case 0:
				name = camel_header_decode_string (token.v.qstring, NULL);
				break;
			case 2:
				user = g_strdup (token.v.qstring);
				break;
			case 3:
				domain = token.v.qstring;
				break;
			}
			break;
		default:
			camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
			g_free (name);
			g_free (user);
			return -1;
		}
		
		part++;
	} while (part < 4);
	
	addr = g_strdup_printf ("%s@%s", user, domain);
	g_free (user);
	
	cia = camel_header_address_new_name (name, addr);
	g_free (name);
	g_free (addr);
	
	addr = camel_header_address_list_format (cia);
	camel_header_address_unref (cia);
	
	g_string_append (addrs, addr);
	g_free (addr);
	
	if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	if (token.token != ')') {
		camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	return 0;
}

static int
envelope_decode_addresses (CamelIMAP4Engine *engine, char **addrlist, CamelException *ex)
{
	camel_imap4_token_t token;
	GString *addrs;
	
	if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	if (token.token == CAMEL_IMAP4_TOKEN_NIL) {
		*addrlist = NULL;
		return 0;
	} else if (token.token != '(') {
		camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	addrs = g_string_new ("");
	
	do {
		if (camel_imap4_engine_next_token (engine, &token, ex) == -1) {
			g_string_free (addrs, TRUE);
			return -1;
		}
		
		if (token.token == '(') {
			camel_imap4_stream_unget_token (engine->istream, &token);
			
			if (envelope_decode_address (engine, addrs, ex) == -1) {
				g_string_free (addrs, TRUE);
				return -1;
			}
		} else if (token.token == ')') {
			break;
		} else {
			camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
			return -1;
		}
	} while (1);
	
	*addrlist = addrs->str;
	g_string_free (addrs, FALSE);
	
	return 0;
}

static int
envelope_decode_date (CamelIMAP4Engine *engine, time_t *date, CamelException *ex)
{
	camel_imap4_token_t token;
	const char *nstring;
	
	if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	switch (token.token) {
	case CAMEL_IMAP4_TOKEN_NIL:
		*date = (time_t) -1;
		return 0;
	case CAMEL_IMAP4_TOKEN_ATOM:
		nstring = token.v.atom;
		break;
	case CAMEL_IMAP4_TOKEN_QSTRING:
		nstring = token.v.qstring;
		break;
	default:
		camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	*date = camel_header_decode_date (nstring, NULL);
	
	return 0;
}

static int
envelope_decode_nstring (CamelIMAP4Engine *engine, char **nstring, gboolean rfc2047, CamelException *ex)
{
	camel_imap4_token_t token;
	
	if (camel_imap4_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	switch (token.token) {
	case CAMEL_IMAP4_TOKEN_NIL:
		*nstring = NULL;
		break;
	case CAMEL_IMAP4_TOKEN_ATOM:
		if (rfc2047)
			*nstring = camel_header_decode_string (token.v.atom, NULL);
		else
			*nstring = g_strdup (token.v.atom);
		break;
	case CAMEL_IMAP4_TOKEN_QSTRING:
		if (rfc2047)
			*nstring = camel_header_decode_string (token.v.qstring, NULL);
		else
			*nstring = g_strdup (token.v.qstring);
		break;
	default:
		camel_imap4_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	return 0;
}

static CamelSummaryReferences *
decode_references (const char *string)
{
	struct _camel_header_references *refs, *r;
	CamelSummaryReferences *references;
	unsigned char md5sum[16];
	guint32 i, n = 0;
	MD5Context md5;
	
	if (!(r = refs = camel_header_references_inreplyto_decode (string)))
		return NULL;
	
	while (r != NULL) {
		r = r->next;
		n++;
	}
	
	references = g_malloc (sizeof (CamelSummaryReferences) + (sizeof (CamelSummaryMessageID) * (n - 1)));
	references->size = n;
	
	for (i = 0, r = refs; i < n; i++, r = r->next) {
		md5_init (&md5);
		md5_update (&md5, r->id, strlen (r->id));
		md5_final (&md5, md5sum);
		memcpy (references->references[i].id.hash, md5sum, sizeof (references->references[i].id.hash));
	}
	
	camel_header_references_list_clear (&refs);
	
	return references;
}

static int
decode_envelope (CamelIMAP4Engine *engine, CamelMessageInfo *info, camel_imap4_token_t *token, CamelException *ex)
{
	unsigned char md5sum[16];
	char *nstring;
	
	if (camel_imap4_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	if (token->token != '(') {
		camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
		return -1;
	}
	
	if (envelope_decode_date (engine, &info->date_sent, ex) == -1)
		goto exception;
	
	/* subject */
	if (envelope_decode_nstring (engine, &nstring, TRUE, ex) == -1)
		goto exception;
	camel_message_info_set_subject (info, nstring);
	
	/* from */
	if (envelope_decode_addresses (engine, &nstring, ex) == -1)
		goto exception;
	camel_message_info_set_from (info, nstring);
	
	/* sender */
	if (envelope_decode_addresses (engine, &nstring, ex) == -1)
		goto exception;
	g_free (nstring);
	
	/* reply-to */
	if (envelope_decode_addresses (engine, &nstring, ex) == -1)
		goto exception;
	g_free (nstring);
	
	/* to */
	if (envelope_decode_addresses (engine, &nstring, ex) == -1)
		goto exception;
	camel_message_info_set_to (info, nstring);
	
	/* cc */
	if (envelope_decode_addresses (engine, &nstring, ex) == -1)
		goto exception;
	camel_message_info_set_cc (info, nstring);
	
	/* bcc */
	if (envelope_decode_addresses (engine, &nstring, ex) == -1)
		goto exception;
	g_free (nstring);
	
	/* in-reply-to */
	if (envelope_decode_nstring (engine, &nstring, FALSE, ex) == -1)
		goto exception;
	
	if (nstring != NULL) {
		info->references = decode_references (nstring);
		g_free (nstring);
	}
	
	/* message-id */
	if (envelope_decode_nstring (engine, &nstring, FALSE, ex) == -1)
		goto exception;
	
	if (nstring != NULL) {
		md5_get_digest (nstring, strlen (nstring), md5sum);
		memcpy (info->message_id.id.hash, md5sum, sizeof (info->message_id.id.hash));
		g_free (nstring);
	}
	
	if (camel_imap4_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	if (token->token != ')') {
		camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
		goto exception;
	}
	
	return 0;
	
 exception:
	
	return -1;
}

static char *tm_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static gboolean
decode_time (const char **in, int *hour, int *min, int *sec)
{
	register const unsigned char *inptr = (const unsigned char *) *in;
	int *val, colons = 0;
	
	*hour = *min = *sec = 0;
	
	val = hour;
	for ( ; *inptr && !isspace ((int) *inptr); inptr++) {
		if (*inptr == ':') {
			colons++;
			switch (colons) {
			case 1:
				val = min;
				break;
			case 2:
				val = sec;
				break;
			default:
				return FALSE;
			}
		} else if (!isdigit ((int) *inptr))
			return FALSE;
		else
			*val = (*val * 10) + (*inptr - '0');
	}
	
	*in = inptr;
	
	return TRUE;
}

static time_t
mktime_utc (struct tm *tm)
{
	time_t tt;
	
	tm->tm_isdst = -1;
	tt = mktime (tm);
	
#if defined (HAVE_TM_GMTOFF)
	tt += tm->tm_gmtoff;
#elif defined (HAVE_TIMEZONE)
	if (tm->tm_isdst > 0) {
#if defined (HAVE_ALTZONE)
		tt -= altzone;
#else /* !defined (HAVE_ALTZONE) */
		tt -= (timezone - 3600);
#endif
	} else
		tt -= timezone;
#endif
	
	return tt;
}

static time_t
decode_internaldate (const char *in)
{
	const char *inptr = in;
	int hour, min, sec, n;
	struct tm tm;
	time_t date;
	char *buf;
	
	memset ((void *) &tm, 0, sizeof (struct tm));
	
	tm.tm_mday = strtoul (inptr, &buf, 10);
	if (buf == inptr || *buf != '-')
		return (time_t) -1;
	
	inptr = buf + 1;
	if (inptr[3] != '-')
		return (time_t) -1;
	
	for (n = 0; n < 12; n++) {
		if (!strncasecmp (inptr, tm_months[n], 3))
			break;
	}
	
	if (n >= 12)
		return (time_t) -1;
	
	tm.tm_mon = n;
	
	inptr += 4;
	
	n = strtoul (inptr, &buf, 10);
	if (buf == inptr || *buf != ' ')
		return (time_t) -1;
	
	tm.tm_year = n - 1900;
	
	inptr = buf + 1;
	if (!decode_time (&inptr, &hour, &min, &sec))
		return (time_t) -1;
	
	tm.tm_hour = hour;
	tm.tm_min = min;
	tm.tm_sec = sec;
	
	n = strtol (inptr, NULL, 10);
	
	date = mktime_utc (&tm);
	
	/* date is now GMT of the time we want, but not offset by the timezone ... */
	
	/* this should convert the time to the GMT equiv time */
	date -= ((n / 100) * 60 * 60) + (n % 100) * 60;
	
	return date;
}

enum {
	IMAP4_FETCH_ENVELOPE     = (1 << 1),
	IMAP4_FETCH_FLAGS        = (1 << 2),
	IMAP4_FETCH_INTERNALDATE = (1 << 3),
	IMAP4_FETCH_RFC822SIZE   = (1 << 4),
	IMAP4_FETCH_UID          = (1 << 5),
};

#define IMAP4_FETCH_ALL (IMAP4_FETCH_ENVELOPE | IMAP4_FETCH_FLAGS | IMAP4_FETCH_INTERNALDATE | IMAP4_FETCH_RFC822SIZE | IMAP4_FETCH_UID)

struct imap4_envelope_t {
	CamelMessageInfo *info;
	guint changed;
};

struct imap4_fetch_all_t {
	CamelFolderChangeInfo *changes;
	CamelFolderSummary *summary;
	GHashTable *uid_hash;
	GPtrArray *added;
	guint32 first;
};

static void
imap4_fetch_all_free (struct imap4_fetch_all_t *fetch)
{
	struct imap4_envelope_t *envelope;
	int i;
	
	for (i = 0; i < fetch->added->len; i++) {
		if (!(envelope = fetch->added->pdata[i]))
			continue;
		
		camel_folder_summary_info_free (fetch->summary, envelope->info);
		g_free (envelope);
	}
	
	g_ptr_array_free (fetch->added, TRUE);
	g_hash_table_destroy (fetch->uid_hash);
	camel_folder_change_info_free (fetch->changes);
	g_free (fetch);
}

static void
courier_imap_is_a_piece_of_shit (CamelFolderSummary *summary, guint32 msg)
{
	CamelIMAP4Summary *imap = (CamelIMAP4Summary *) summary;
	CamelSession *session = ((CamelService *) ((CamelFolder *) imap->folder)->parent_store)->session;
	char *warning;
	
	warning = g_strdup_printf ("IMAP server did not respond with an untagged FETCH response for\n"
				   "message #%u. This is illegal according to rfc3501 (and the older\n"
				   "rfc2060). You will need to contact your Administrator(s) (or ISP)\n"
				   "and have them resolve this issue.\n\n"
				   "Hint: If your IMAP server is Courier-IMAP, it is likely that this\n"
				   "message is simply unreadable by the IMAP server and will need to\n"
				   "be given read permissions.\n", msg);
	
	camel_session_alert_user (session, CAMEL_SESSION_ALERT_WARNING, warning, FALSE);
	g_free (warning);
}

static void
imap4_fetch_all_add (struct imap4_fetch_all_t *fetch)
{
	CamelIMAP4Summary *imap4_summary = (CamelIMAP4Summary *) fetch->summary;
	CamelFolderChangeInfo *changes = NULL;
	struct imap4_envelope_t *envelope;
	CamelMessageInfo *info;
	int i;
	
	changes = fetch->changes;
	
	for (i = 0; i < fetch->added->len; i++) {
		if (!(envelope = fetch->added->pdata[i])) {
			courier_imap_is_a_piece_of_shit (fetch->summary, i + fetch->first);
			break;
		}
		
		if (envelope->changed != IMAP4_FETCH_ALL) {
			fprintf (stderr, "Hmmm, IMAP4 server didn't give us everything for message %d\n", i + 1);
			camel_folder_summary_info_free (fetch->summary, envelope->info);
			g_free (envelope);
			continue;
		}
		
		if ((info = camel_folder_summary_uid (fetch->summary, camel_message_info_uid (envelope->info)))) {
			camel_folder_summary_info_free (fetch->summary, envelope->info);
			g_free (envelope);
			continue;
		}
		
		camel_folder_change_info_add_uid (changes, camel_message_info_uid (envelope->info));
		
		camel_folder_summary_add (fetch->summary, envelope->info);
		g_free (envelope);
	}
	
	g_ptr_array_free (fetch->added, TRUE);
	g_hash_table_destroy (fetch->uid_hash);
	
	if (camel_folder_change_info_changed (changes))
		camel_object_trigger_event (imap4_summary->folder, "folder_changed", changes);
	camel_folder_change_info_free (changes);
	
	g_free (fetch);
}

static guint32
imap4_fetch_all_update (struct imap4_fetch_all_t *fetch)
{
	CamelIMAP4Summary *imap4_summary = (CamelIMAP4Summary *) fetch->summary;
	CamelIMAP4MessageInfo *iinfo, *new_iinfo;
	CamelFolderChangeInfo *changes = NULL;
	struct imap4_envelope_t *envelope;
	CamelMessageInfo *info;
	guint32 first = 0;
	guint32 flags;
	int scount, i;
	
	changes = fetch->changes;
	
	scount = camel_folder_summary_count (fetch->summary);
	for (i = fetch->first - 1; i < scount; i++) {
		info = camel_folder_summary_index (fetch->summary, i);
		if (!(envelope = g_hash_table_lookup (fetch->uid_hash, camel_message_info_uid (info)))) {
			/* remove it */
			camel_folder_change_info_remove_uid (changes, camel_message_info_uid (info));
			camel_folder_summary_remove (fetch->summary, info);
			scount--;
			i--;
		} else if (envelope->changed & IMAP4_FETCH_FLAGS) {
			/* update it with the new flags */
			new_iinfo = (CamelIMAP4MessageInfo *) envelope->info;
			iinfo = (CamelIMAP4MessageInfo *) info;
			
			flags = info->flags;
			info->flags = camel_imap4_merge_flags (iinfo->server_flags, info->flags, new_iinfo->server_flags);
			iinfo->server_flags = new_iinfo->server_flags;
			if (info->flags != flags)
				camel_folder_change_info_change_uid (changes, camel_message_info_uid (info));
		}
		
		camel_folder_summary_info_free (fetch->summary, info);
	}
	
	for (i = 0; i < fetch->added->len; i++) {
		if (!(envelope = fetch->added->pdata[i])) {
			courier_imap_is_a_piece_of_shit (fetch->summary, i + fetch->first);
			break;
		}
		
		info = envelope->info;
		if (!first && camel_message_info_uid (info)) {
			if ((info = camel_folder_summary_uid (fetch->summary, camel_message_info_uid (info)))) {
				camel_folder_summary_info_free (fetch->summary, info);
			} else {
				first = i + fetch->first;
			}
		}
		
		camel_folder_summary_info_free (fetch->summary, envelope->info);
		g_free (envelope);
	}
	
	g_ptr_array_free (fetch->added, TRUE);
	g_hash_table_destroy (fetch->uid_hash);
	
	if (camel_folder_change_info_changed (changes))
		camel_object_trigger_event (imap4_summary->folder, "folder_changed", changes);
	camel_folder_change_info_free (changes);
	
	g_free (fetch);
	
	return first;
}

static int
untagged_fetch_all (CamelIMAP4Engine *engine, CamelIMAP4Command *ic, guint32 index, camel_imap4_token_t *token, CamelException *ex)
{
	struct imap4_fetch_all_t *fetch = ic->user_data;
	CamelFolderSummary *summary = fetch->summary;
	struct imap4_envelope_t *envelope = NULL;
	GPtrArray *added = fetch->added;
	CamelIMAP4MessageInfo *iinfo;
	CamelMessageInfo *info;
	guint32 changed = 0;
	const char *iuid;
	char uid[12];
	
	if (index < fetch->first) {
		/* we already have this message envelope cached -
		 * server is probably notifying us of a FLAGS change
		 * by another client? */
		g_assert (index < summary->messages->len);
		iinfo = (CamelIMAP4MessageInfo *) info = summary->messages->pdata[index - 1];
		g_assert (info != NULL);
	} else {
		if (index > (added->len + fetch->first - 1))
			g_ptr_array_set_size (added, index - fetch->first + 1);
		
		if (!(envelope = added->pdata[index - fetch->first])) {
			iinfo = (CamelIMAP4MessageInfo *) info = camel_folder_summary_info_new (summary);
			envelope = g_new (struct imap4_envelope_t, 1);
			added->pdata[index - fetch->first] = envelope;
			envelope->info = info;
			envelope->changed = 0;
		} else {
			iinfo = (CamelIMAP4MessageInfo *) info = envelope->info;
		}
	}
	
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
		
		if (token->token == ')' || token->token == '\n')
			break;
		
		if (token->token != CAMEL_IMAP4_TOKEN_ATOM)
			goto unexpected;
		
		if (!strcmp (token->v.atom, "ENVELOPE")) {
			if (envelope) {
				if (decode_envelope (engine, info, token, ex) == -1)
					goto exception;
				
				changed |= IMAP4_FETCH_ENVELOPE;
			} else {
				CamelMessageInfo *tmp;
				int rv;
				
				g_warning ("Hmmm, server is sending us ENVELOPE data for a message we didn't ask for (message %u)\n",
					   index);
				tmp = camel_folder_summary_info_new (summary);
				rv = decode_envelope (engine, tmp, token, ex);
				camel_folder_summary_info_free (summary, tmp);
				
				if (rv == -1)
					goto exception;
			}
		} else if (!strcmp (token->v.atom, "FLAGS")) {
			guint32 server_flags = 0;
			
			if (camel_imap4_parse_flags_list (engine, &server_flags, ex) == -1)
				return -1;
			
			info->flags = camel_imap4_merge_flags (iinfo->server_flags, info->flags, server_flags);
			iinfo->server_flags = server_flags;
			
			changed |= IMAP4_FETCH_FLAGS;
		} else if (!strcmp (token->v.atom, "INTERNALDATE")) {
			if (camel_imap4_engine_next_token (engine, token, ex) == -1)
				goto exception;
			
			switch (token->token) {
			case CAMEL_IMAP4_TOKEN_NIL:
				info->date_received = (time_t) -1;
				break;
			case CAMEL_IMAP4_TOKEN_ATOM:
			case CAMEL_IMAP4_TOKEN_QSTRING:
				info->date_received = decode_internaldate (token->v.qstring);
				break;
			default:
				goto unexpected;
			}
			
			changed |= IMAP4_FETCH_INTERNALDATE;
		} else if (!strcmp (token->v.atom, "RFC822.SIZE")) {
			if (camel_imap4_engine_next_token (engine, token, ex) == -1)
				goto exception;
			
			if (token->token != CAMEL_IMAP4_TOKEN_NUMBER)
				goto unexpected;
			
			info->size = token->v.number;
			
			changed |= IMAP4_FETCH_RFC822SIZE;
		} else if (!strcmp (token->v.atom, "UID")) {
			if (camel_imap4_engine_next_token (engine, token, ex) == -1)
				goto exception;
			
			if (token->token != CAMEL_IMAP4_TOKEN_NUMBER || token->v.number == 0)
				goto unexpected;
			
			sprintf (uid, "%u", token->v.number);
			iuid = camel_message_info_uid (info);
			if (iuid != NULL && iuid[0] != '\0') {
				if (strcmp (iuid, uid) != 0) {
					fprintf (stderr, "Hmmm, UID mismatch for message %u\n", index);
					g_assert_not_reached ();
				}
			} else {
				camel_message_info_set_uid (info, g_strdup (uid));
				g_hash_table_insert (fetch->uid_hash, (void *) camel_message_info_uid (info), envelope);
				changed |= IMAP4_FETCH_UID;
			}
		} else {
			/* wtf? */
			fprintf (stderr, "huh? %s?...\n", token->v.atom);
		}
	} while (1);
	
	if (envelope) {
		envelope->changed |= changed;
	} else if (changed & IMAP4_FETCH_FLAGS) {
		camel_folder_change_info_change_uid (fetch->changes, camel_message_info_uid (info));
	}
	
	if (token->token != ')')
		goto unexpected;
	
	return 0;
	
 unexpected:
	
	camel_imap4_utils_set_unexpected_token_error (ex, engine, token);
	
 exception:
	
	return -1;
}

static CamelIMAP4Command *
imap4_summary_fetch_all (CamelFolderSummary *summary, guint32 first, guint32 last)
{
	CamelIMAP4Summary *imap4_summary = (CamelIMAP4Summary *) summary;
	CamelFolder *folder = imap4_summary->folder;
	struct imap4_fetch_all_t *fetch;
	CamelIMAP4Engine *engine;
	CamelIMAP4Command *ic;
	
	engine = ((CamelIMAP4Store *) folder->parent_store)->engine;
	
	/* FIXME: would be a nice optimisation if we could size the
	 * 'added' array here rather than possibly having to grow it
	 * one element at a time (in the common case) in the
	 * untagged_fetch_all() callback */
	fetch = g_new (struct imap4_fetch_all_t, 1);
	fetch->uid_hash = g_hash_table_new (g_str_hash, g_str_equal);
	fetch->changes = camel_folder_change_info_new ();
	fetch->added = g_ptr_array_new ();
	fetch->summary = summary;
	fetch->first = first;
	
	/* From rfc2060, Section 6.4.5:
	 * 
	 * The currently defined data items that can be fetched are:
	 *
	 * ALL            Macro equivalent to: (FLAGS INTERNALDATE
	 *                RFC822.SIZE ENVELOPE)
	 **/
	
	if (last != 0)
		ic = camel_imap4_engine_queue (engine, folder, "FETCH %u:%u (UID ALL)\r\n", first, last);
	else
		ic = camel_imap4_engine_queue (engine, folder, "FETCH %u:* (UID ALL)\r\n", first);
	
	camel_imap4_command_register_untagged (ic, "FETCH", untagged_fetch_all);
	ic->user_data = fetch;
	
	return ic;
}

static CamelIMAP4Command *
imap4_summary_fetch_flags (CamelFolderSummary *summary, guint32 first, guint32 last)
{
	CamelIMAP4Summary *imap4_summary = (CamelIMAP4Summary *) summary;
	CamelFolder *folder = imap4_summary->folder;
	struct imap4_fetch_all_t *fetch;
	CamelIMAP4Engine *engine;
	CamelIMAP4Command *ic;
	
	engine = ((CamelIMAP4Store *) folder->parent_store)->engine;
	
	/* FIXME: would be a nice optimisation if we could size the
	 * 'added' array here rather than possibly having to grow it
	 * one element at a time (in the common case) in the
	 * untagged_fetch_all() callback */
	fetch = g_new (struct imap4_fetch_all_t, 1);
	fetch->uid_hash = g_hash_table_new (g_str_hash, g_str_equal);
	fetch->changes = camel_folder_change_info_new ();
	fetch->added = g_ptr_array_new ();
	fetch->summary = summary;
	fetch->first = first;
	
	if (last != 0)
		ic = camel_imap4_engine_queue (engine, folder, "FETCH %u:%u (UID FLAGS)\r\n", first, last);
	else
		ic = camel_imap4_engine_queue (engine, folder, "FETCH %u:* (UID FLAGS)\r\n", first);
	
	camel_imap4_command_register_untagged (ic, "FETCH", untagged_fetch_all);
	ic->user_data = fetch;
	
	return ic;
}

#if 0
static int
imap4_build_summary (CamelFolderSummary *summary, guint32 first, guint32 last)
{
	CamelIMAP4Summary *imap4_summary = (CamelIMAP4Summary *) summary;
	CamelFolder *folder = imap4_summary->folder;
	struct imap4_fetch_all_t *fetch;
	CamelIMAP4Engine *engine;
	CamelIMAP4Command *ic;
	int id;
	
	engine = ((CamelIMAP4Store *) folder->store)->engine;
	
	ic = imap4_summary_fetch_all (summary, first, last);
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	fetch = ic->user_data;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_imap4_command_unref (ic);
		imap4_fetch_all_free (fetch);
		return -1;
	}
	
	imap4_fetch_all_add (fetch);
	
	camel_imap4_command_unref (ic);
	
	return 0;
}
#endif

static CamelMessageInfo *
imap4_message_info_new (CamelFolderSummary *summary, struct _camel_header_raw *header)
{
	CamelMessageInfo *info;
	
	info = CAMEL_FOLDER_SUMMARY_CLASS (parent_class)->message_info_new (summary, header);
	
	((CamelIMAP4MessageInfo *) info)->server_flags = 0;
	
	return info;
}

static CamelMessageInfo *
imap4_message_info_load (CamelFolderSummary *summary, FILE *fin)
{
	CamelIMAP4MessageInfo *minfo;
	CamelMessageInfo *info;
	
	if (!(info = CAMEL_FOLDER_SUMMARY_CLASS (parent_class)->message_info_load (summary, fin)))
		return NULL;
	
	minfo = (CamelIMAP4MessageInfo *) info;
	
	if (camel_file_util_decode_uint32 (fin, &minfo->server_flags) == -1)
		goto exception;
	
	return info;
	
 exception:
	
	camel_folder_summary_info_free (summary, info);
	
	return NULL;
}

static int
imap4_message_info_save (CamelFolderSummary *summary, FILE *fout, CamelMessageInfo *info)
{
	CamelIMAP4MessageInfo *minfo = (CamelIMAP4MessageInfo *) info;
	
	if (CAMEL_FOLDER_SUMMARY_CLASS (parent_class)->message_info_save (summary, fout, info) == -1)
		return -1;
	
	if (camel_file_util_encode_uint32 (fout, minfo->server_flags) == -1)
		return -1;
	
	return 0;
}


void
camel_imap4_summary_set_exists (CamelFolderSummary *summary, guint32 exists)
{
	CamelIMAP4Summary *imap4_summary = (CamelIMAP4Summary *) summary;
	
	g_return_if_fail (CAMEL_IS_IMAP4_SUMMARY (summary));
	
	imap4_summary->exists = exists;
}

void
camel_imap4_summary_set_recent (CamelFolderSummary *summary, guint32 recent)
{
	CamelIMAP4Summary *imap4_summary = (CamelIMAP4Summary *) summary;
	
	g_return_if_fail (CAMEL_IS_IMAP4_SUMMARY (summary));
	
	imap4_summary->recent = recent;
}

void
camel_imap4_summary_set_unseen (CamelFolderSummary *summary, guint32 unseen)
{
	CamelIMAP4Summary *imap4_summary = (CamelIMAP4Summary *) summary;
	
	g_return_if_fail (CAMEL_IS_IMAP4_SUMMARY (summary));
	
	imap4_summary->unseen = unseen;
}

void
camel_imap4_summary_set_uidnext (CamelFolderSummary *summary, guint32 uidnext)
{
	g_return_if_fail (CAMEL_IS_IMAP4_SUMMARY (summary));
	
	summary->nextuid = uidnext;
}

void
camel_imap4_summary_set_uidvalidity (CamelFolderSummary *summary, guint32 uidvalidity)
{
	CamelIMAP4Summary *imap4_summary = (CamelIMAP4Summary *) summary;
	CamelFolderChangeInfo *changes;
	CamelMessageInfo *info;
	int i, count;
	
	g_return_if_fail (CAMEL_IS_IMAP4_SUMMARY (summary));
	
	if (imap4_summary->uidvalidity == uidvalidity)
		return;
	
	changes = camel_folder_change_info_new ();
	count = camel_folder_summary_count (summary);
	for (i = 0; i < count; i++) {
		if (!(info = camel_folder_summary_index (summary, i)))
			continue;
		
		camel_folder_change_info_remove_uid (changes, camel_message_info_uid (info));
		camel_folder_summary_info_free (summary, info);
	}
	
	camel_folder_summary_clear (summary);
	
	if (camel_folder_change_info_changed (changes))
		camel_object_trigger_event (imap4_summary->folder, "folder_changed", changes);
	camel_folder_change_info_free (changes);
	
	imap4_summary->uidvalidity = uidvalidity;
	
	imap4_summary->uidvalidity_changed = TRUE;
}

void
camel_imap4_summary_expunge (CamelFolderSummary *summary, int seqid)
{
	CamelIMAP4Summary *imap4_summary = (CamelIMAP4Summary *) summary;
	CamelFolderChangeInfo *changes;
	CamelMessageInfo *info;
	
	g_return_if_fail (CAMEL_IS_IMAP4_SUMMARY (summary));
	
	seqid--;
	if (!(info = camel_folder_summary_index (summary, seqid)))
		return;
	
	imap4_summary->exists--;
	
	changes = camel_folder_change_info_new ();
	camel_folder_change_info_remove_uid (changes, camel_message_info_uid (info));
	
	camel_folder_summary_info_free (summary, info);
	camel_folder_summary_remove_index (summary, seqid);
	
	camel_object_trigger_event (imap4_summary->folder, "folder_changed", changes);
	camel_folder_change_info_free (changes);
}

#if 0
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
#endif

int
camel_imap4_summary_flush_updates (CamelFolderSummary *summary, CamelException *ex)
{
	CamelIMAP4Summary *imap4_summary = (CamelIMAP4Summary *) summary;
	CamelIMAP4Engine *engine;
	CamelIMAP4Command *ic;
	guint32 first = 0;
	int scount, id;
	
	g_return_val_if_fail (CAMEL_IS_IMAP4_SUMMARY (summary), -1);
	
	engine = ((CamelIMAP4Store *) imap4_summary->folder->parent_store)->engine;
	scount = camel_folder_summary_count (summary);
	
	if (imap4_summary->uidvalidity_changed) {
		first = 1;
	} else if (imap4_summary->update_flags || imap4_summary->exists < scount) {
		/* this both updates flags and removes messages which
		 * have since been expunged from the server by another
		 * client */
		ic = imap4_summary_fetch_flags (summary, 1, scount);
		
		while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
			;
		
		if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
			imap4_fetch_all_free (ic->user_data);
			camel_exception_xfer (ex, &ic->ex);
			camel_imap4_command_unref (ic);
			return -1;
		}
		
		if (!(first = imap4_fetch_all_update (ic->user_data)) && imap4_summary->exists > scount)
			first = scount + 1;
		
		camel_imap4_command_unref (ic);
	} else {
		first = scount + 1;
	}
	
	if (first != 0 && first <= imap4_summary->exists) {
		ic = imap4_summary_fetch_all (summary, first, 0);
		
		while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
			;
		
		if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
			imap4_fetch_all_free (ic->user_data);
			camel_exception_xfer (ex, &ic->ex);
			camel_imap4_command_unref (ic);
			return -1;
		}
		
		imap4_fetch_all_add (ic->user_data);
		camel_imap4_command_unref (ic);
		
#if 0
		/* Note: this should not be needed - the code that adds envelopes to the summary
		 * adds them in proper order */
		
		/* it's important for these to be sorted sequentially for EXPUNGE events to work */
		g_ptr_array_sort (summary->messages, (GCompareFunc) info_uid_sort);
#endif
	}
	
	imap4_summary->update_flags = FALSE;
	imap4_summary->uidvalidity_changed = FALSE;
	
	return 0;
}
