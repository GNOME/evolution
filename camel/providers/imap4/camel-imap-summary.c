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

#include "camel-imap-store.h"
#include "camel-imap-engine.h"
#include "camel-imap-folder.h"
#include "camel-imap-stream.h"
#include "camel-imap-command.h"
#include "camel-imap-utils.h"

#include "camel-imap-summary.h"

#define IMAP_SUMMARY_VERSION  1

static void camel_imap_summary_class_init (CamelIMAPSummaryClass *klass);
static void camel_imap_summary_init (CamelIMAPSummary *summary, CamelIMAPSummaryClass *klass);
static void camel_imap_summary_finalize (CamelObject *object);

static int imap_header_load (CamelFolderSummary *summary, FILE *fin);
static int imap_header_save (CamelFolderSummary *summary, FILE *fout);
static CamelMessageInfo *imap_message_info_new (CamelFolderSummary *summary, struct _camel_header_raw *header);
static CamelMessageInfo *imap_message_info_load (CamelFolderSummary *summary, FILE *fin);
static int imap_message_info_save (CamelFolderSummary *summary, FILE *fout, CamelMessageInfo *info);


static CamelFolderSummaryClass *parent_class = NULL;


CamelType
camel_imap_summary_get_type (void)
{
	static CamelType type = 0;
	
	if (!type) {
		type = camel_type_register (CAMEL_TYPE_IMAP_SUMMARY,
					    "CamelIMAPSummary",
					    sizeof (CamelIMAPSummary),
					    sizeof (CamelIMAPSummaryClass),
					    (CamelObjectClassInitFunc) camel_imap_summary_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_imap_summary_init,
					    (CamelObjectFinalizeFunc) camel_imap_summary_finalize);
	}
	
	return type;
}


static void
camel_imap_summary_class_init (CamelIMAPSummaryClass *klass)
{
	CamelFolderSummaryClass *summary_class = (CamelFolderSummaryClass *) klass;
	
	parent_class = (CamelFolderSummaryClass *) camel_type_get_global_classfuncs (camel_folder_summary_get_type ());
	
	summary_class->summary_header_load = imap_header_load;
	summary_class->summary_header_save = imap_header_save;
	summary_class->message_info_new = imap_message_info_new;
	summary_class->message_info_load = imap_message_info_load;
	summary_class->message_info_save = imap_message_info_save;
}

static void
camel_imap_summary_init (CamelIMAPSummary *summary, CamelIMAPSummaryClass *klass)
{
	CamelFolderSummary *folder_summary = (CamelFolderSummary *) summary;
	
	folder_summary->version += IMAP_SUMMARY_VERSION;
	folder_summary->flags = CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_DELETED |
		CAMEL_MESSAGE_DRAFT | CAMEL_MESSAGE_FLAGGED | CAMEL_MESSAGE_SEEN;
	
	folder_summary->message_info_size = sizeof (CamelIMAPMessageInfo);
}

static void
camel_imap_summary_finalize (CamelObject *object)
{
	;
}


CamelFolderSummary *
camel_imap_summary_new (CamelFolder *folder)
{
	CamelFolderSummary *summary;
	
	summary = (CamelFolderSummary *) camel_object_new (CAMEL_TYPE_IMAP_SUMMARY);
	((CamelIMAPSummary *) summary)->folder = folder;
	
	return summary;
}

static int
imap_header_load (CamelFolderSummary *summary, FILE *fin)
{
	CamelIMAPSummary *imap_summary = (CamelIMAPSummary *) summary;
	
	if (CAMEL_FOLDER_SUMMARY_CLASS (parent_class)->summary_header_load (summary, fin) == -1)
		return -1;
	
	if (camel_file_util_decode_uint32 (fin, &imap_summary->uidvalidity) == -1)
		return -1;
	
	return 0;
}

static int
imap_header_save (CamelFolderSummary *summary, FILE *fout)
{
	CamelIMAPSummary *imap_summary = (CamelIMAPSummary *) summary;
	
	if (CAMEL_FOLDER_SUMMARY_CLASS (parent_class)->summary_header_save (summary, fout) == -1)
		return -1;
	
	if (camel_file_util_encode_uint32 (fout, imap_summary->uidvalidity) == -1)
		return -1;
	
	return 0;
}

static int
envelope_decode_address (CamelIMAPEngine *engine, GString *addrs, CamelException *ex)
{
	camel_imap_token_t token;
	gboolean had_name = FALSE;
	int part = 0;
	
	if (camel_imap_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	if (token.token == CAMEL_IMAP_TOKEN_NIL) {
		return 0;
	} else if (token.token != '(') {
		camel_imap_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	if (addrs->len > 0)
		g_string_append (addrs, ", ");
	
	do {
		if (camel_imap_engine_next_token (engine, &token, ex) == -1)
			return -1;
		
		switch (token.token) {
		case CAMEL_IMAP_TOKEN_NIL:
			break;
		case CAMEL_IMAP_TOKEN_ATOM:
		case CAMEL_IMAP_TOKEN_QSTRING:
			switch (part) {
			case 0:
				g_string_append_printf (addrs, "\"%s\" <", token.v.qstring);
				had_name = TRUE;
				break;
			case 2:
				g_string_append (addrs, token.v.qstring);
				break;
			case 3:
				g_string_append_printf (addrs, "@%s%s", token.v.qstring, had_name ? ">" : "");
				break;
			}
			break;
		default:
			camel_imap_utils_set_unexpected_token_error (ex, engine, &token);
			return -1;
		}
		
		part++;
	} while (part < 4);
	
	if (camel_imap_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	if (token.token != ')') {
		camel_imap_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	return 0;
}

static int
envelope_decode_addresses (CamelIMAPEngine *engine, char **addrlist, CamelException *ex)
{
	camel_imap_token_t token;
	GString *addrs;
	
	if (camel_imap_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	if (token.token == CAMEL_IMAP_TOKEN_NIL) {
		*addrlist = NULL;
		return 0;
	} else if (token.token != '(') {
		camel_imap_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	addrs = g_string_new ("");
	
	do {
		if (camel_imap_engine_next_token (engine, &token, ex) == -1) {
			g_string_free (addrs, TRUE);
			return -1;
		}
		
		if (token.token == '(') {
			camel_imap_stream_unget_token (engine->istream, &token);
			
			if (envelope_decode_address (engine, addrs, ex) == -1) {
				g_string_free (addrs, TRUE);
				return -1;
			}
		} else if (token.token == ')') {
			break;
		} else {
			camel_imap_utils_set_unexpected_token_error (ex, engine, &token);
			return -1;
		}
	} while (1);
	
	*addrlist = addrs->str;
	g_string_free (addrs, FALSE);
	
	return 0;
}

static int
envelope_decode_date (CamelIMAPEngine *engine, time_t *date, CamelException *ex)
{
	camel_imap_token_t token;
	const char *nstring;
	
	if (camel_imap_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	switch (token.token) {
	case CAMEL_IMAP_TOKEN_NIL:
		*date = (time_t) -1;
		return 0;
	case CAMEL_IMAP_TOKEN_ATOM:
		nstring = token.v.atom;
		break;
	case CAMEL_IMAP_TOKEN_QSTRING:
		nstring = token.v.qstring;
		break;
	default:
		camel_imap_utils_set_unexpected_token_error (ex, engine, &token);
		return -1;
	}
	
	*date = camel_header_decode_date (nstring, NULL);
	
	return 0;
}

static int
envelope_decode_nstring (CamelIMAPEngine *engine, char **nstring, CamelException *ex)
{
	camel_imap_token_t token;
	
	if (camel_imap_engine_next_token (engine, &token, ex) == -1)
		return -1;
	
	switch (token.token) {
	case CAMEL_IMAP_TOKEN_NIL:
		*nstring = NULL;
		break;
	case CAMEL_IMAP_TOKEN_ATOM:
		*nstring = g_strdup (token.v.atom);
		break;
	case CAMEL_IMAP_TOKEN_QSTRING:
		*nstring = g_strdup (token.v.qstring);
		break;
	default:
		camel_imap_utils_set_unexpected_token_error (ex, engine, &token);
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
decode_envelope (CamelIMAPEngine *engine, CamelMessageInfo *info, camel_imap_token_t *token, CamelException *ex)
{
	unsigned char md5sum[16];
	char *nstring;
	
	if (camel_imap_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	if (token->token != '(') {
		camel_imap_utils_set_unexpected_token_error (ex, engine, token);
		return -1;
	}
	
	if (envelope_decode_date (engine, &info->date_sent, ex) == -1)
		goto exception;
	
	/* subject */
	if (envelope_decode_nstring (engine, &nstring, ex) == -1)
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
	if (envelope_decode_nstring (engine, &nstring, ex) == -1)
		goto exception;
	
	if (nstring != NULL) {
		info->references = decode_references (nstring);
		g_free (nstring);
	}
	
	/* message-id */
	if (envelope_decode_nstring (engine, &nstring, ex) == -1)
		goto exception;
	
	if (nstring != NULL) {
		md5_get_digest (nstring, strlen (nstring), md5sum);
		memcpy (info->message_id.id.hash, md5sum, sizeof (info->message_id.id.hash));
		g_free (nstring);
	}
	
	if (camel_imap_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	if (token->token != ')') {
		camel_imap_utils_set_unexpected_token_error (ex, engine, token);
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
	IMAP_FETCH_ENVELOPE     = (1 << 1),
	IMAP_FETCH_FLAGS        = (1 << 2),
	IMAP_FETCH_INTERNALDATE = (1 << 3),
	IMAP_FETCH_RFC822SIZE   = (1 << 4),
	IMAP_FETCH_UID          = (1 << 5),
};

#define IMAP_FETCH_ALL (IMAP_FETCH_ENVELOPE | IMAP_FETCH_FLAGS | IMAP_FETCH_INTERNALDATE | IMAP_FETCH_RFC822SIZE | IMAP_FETCH_UID)

struct imap_envelope_t {
	CamelMessageInfo *info;
	guint changed;
};

struct imap_fetch_all_t {
	CamelFolderSummary *summary;
	GHashTable *uid_hash;
	GPtrArray *added;
};

static void
imap_fetch_all_free (struct imap_fetch_all_t *fetch)
{
	struct imap_envelope_t *envelope;
	int i;
	
	for (i = 0; i < fetch->added->len; i++) {
		if (!(envelope = fetch->added->pdata[i]))
			continue;
		
		camel_folder_summary_info_free (fetch->summary, envelope->info);
		g_free (envelope);
	}
	
	g_ptr_array_free (fetch->added, TRUE);
	g_hash_table_destroy (fetch->uid_hash);
	
	g_free (fetch);
}

static void
imap_fetch_all_add (struct imap_fetch_all_t *fetch)
{
	struct imap_envelope_t *envelope;
	CamelMessageInfo *info;
	int i;
	
	for (i = 0; i < fetch->added->len; i++) {
		if (!(envelope = fetch->added->pdata[i]))
			continue;
		
		if (envelope->changed != IMAP_FETCH_ALL) {
			fprintf (stderr, "Hmmm, IMAP server didn't give us everything for message %d\n", i + 1);
			camel_folder_summary_info_free (fetch->summary, envelope->info);
			g_free (envelope);
			continue;
		}
		
		if ((info = camel_folder_summary_uid (fetch->summary, camel_message_info_uid (envelope->info)))) {
			camel_folder_summary_info_free (fetch->summary, envelope->info);
			g_free (envelope);
			continue;
		}
		
		camel_folder_summary_add (fetch->summary, envelope->info);
		g_free (envelope);
	}
	
	g_ptr_array_free (fetch->added, TRUE);
	g_hash_table_destroy (fetch->uid_hash);
	
	g_free (fetch);
}

static guint32
imap_fetch_all_update (struct imap_fetch_all_t *fetch)
{
	CamelIMAPMessageInfo *iinfo, *new_iinfo;
	struct imap_envelope_t *envelope;
	CamelMessageInfo *info;
	guint32 first = 0;
	int scount, i;
	
	scount = camel_folder_summary_count (fetch->summary);
	for (i = 0; i < scount; i++) {
		info = camel_folder_summary_index (fetch->summary, i);
		if (!(envelope = g_hash_table_lookup (fetch->uid_hash, camel_message_info_uid (info)))) {
			/* remove it */
			camel_folder_summary_remove (fetch->summary, info);
			scount--;
			i--;
		} else if (envelope->changed & IMAP_FETCH_FLAGS) {
			/* update it with the new flags */
			new_iinfo = (CamelIMAPMessageInfo *) envelope->info;
			iinfo = (CamelIMAPMessageInfo *) info;
			
			info->flags = camel_imap_merge_flags (iinfo->server_flags, info->flags, new_iinfo->server_flags);
			iinfo->server_flags = new_iinfo->server_flags;
		}
		
		camel_folder_summary_info_free (fetch->summary, info);
	}
	
	for (i = 0; i < fetch->added->len; i++) {
		if (!(envelope = fetch->added->pdata[i]))
			continue;
		
		info = envelope->info;
		if (!first && camel_message_info_uid (info)) {
			if ((info = camel_folder_summary_uid (fetch->summary, camel_message_info_uid (info)))) {
				camel_folder_summary_info_free (fetch->summary, info);
			} else {
				first = i + 1;
			}
		}
		
		camel_folder_summary_info_free (fetch->summary, envelope->info);
		g_free (envelope);
	}
	
	g_ptr_array_free (fetch->added, TRUE);
	g_hash_table_destroy (fetch->uid_hash);
	
	g_free (fetch);
	
	return first;
}

static int
untagged_fetch_all (CamelIMAPEngine *engine, CamelIMAPCommand *ic, guint32 index, camel_imap_token_t *token, CamelException *ex)
{
	struct imap_fetch_all_t *fetch = ic->user_data;
	CamelFolderSummary *summary = fetch->summary;
	struct imap_envelope_t *envelope;
	GPtrArray *added = fetch->added;
	CamelIMAPMessageInfo *iinfo;
	CamelMessageInfo *info;
	char uid[12];
	
	if (index > added->len)
		g_ptr_array_set_size (added, index);
	
	if (!(envelope = added->pdata[index - 1])) {
		iinfo = (CamelIMAPMessageInfo *) info = camel_folder_summary_info_new (summary);
		envelope = g_new (struct imap_envelope_t, 1);
		added->pdata[index - 1] = envelope;
		envelope->info = info;
		envelope->changed = 0;
	} else {
		iinfo = (CamelIMAPMessageInfo *) info = envelope->info;
	}
	
	if (camel_imap_engine_next_token (engine, token, ex) == -1)
		return -1;
	
	/* parse the FETCH response list */
	if (token->token != '(') {
		camel_imap_utils_set_unexpected_token_error (ex, engine, token);
		return -1;
	}
	
	do {
		if (camel_imap_engine_next_token (engine, token, ex) == -1)
			goto exception;
		
		if (token->token == ')' || token->token == '\n')
			break;
		
		if (token->token != CAMEL_IMAP_TOKEN_ATOM)
			goto unexpected;
		
		if (!strcmp (token->v.atom, "ENVELOPE")) {
			if (decode_envelope (engine, info, token, ex) == -1)
				goto exception;
			
			envelope->changed |= IMAP_FETCH_ENVELOPE;
		} else if (!strcmp (token->v.atom, "FLAGS")) {
			guint32 server_flags = 0;
			
			if (camel_imap_parse_flags_list (engine, &server_flags, ex) == -1)
				return -1;
			
			info->flags = camel_imap_merge_flags (iinfo->server_flags, info->flags, server_flags);
			iinfo->server_flags = server_flags;
			
			envelope->changed |= IMAP_FETCH_FLAGS;
		} else if (!strcmp (token->v.atom, "INTERNALDATE")) {
			if (camel_imap_engine_next_token (engine, token, ex) == -1)
				goto exception;
			
			switch (token->token) {
			case CAMEL_IMAP_TOKEN_NIL:
				info->date_received = (time_t) -1;
				break;
			case CAMEL_IMAP_TOKEN_ATOM:
			case CAMEL_IMAP_TOKEN_QSTRING:
				info->date_received = decode_internaldate (token->v.qstring);
				break;
			default:
				goto unexpected;
			}
			
			envelope->changed |= IMAP_FETCH_INTERNALDATE;
		} else if (!strcmp (token->v.atom, "RFC822.SIZE")) {
			if (camel_imap_engine_next_token (engine, token, ex) == -1)
				goto exception;
			
			if (token->token != CAMEL_IMAP_TOKEN_NUMBER)
				goto unexpected;
			
			info->size = token->v.number;
			
			envelope->changed |= IMAP_FETCH_RFC822SIZE;
		} else if (!strcmp (token->v.atom, "UID")) {
			if (camel_imap_engine_next_token (engine, token, ex) == -1)
				goto exception;
			
			if (token->token != CAMEL_IMAP_TOKEN_NUMBER || token->v.number == 0)
				goto unexpected;
			
			sprintf (uid, "%u", token->v.number);
			if (camel_message_info_uid (info) != NULL) {
				if (strcmp (camel_message_info_uid (info), uid) != 0)
					fprintf (stderr, "Hmmm, UID mismatch for message %u\n", index);
				else
					fprintf (stderr, "Hmmm, got UID for messages %d again?\n", index);
				
				g_hash_table_remove (fetch->uid_hash, camel_message_info_uid (info));
			}
			
			camel_message_info_set_uid (info, g_strdup (uid));
			g_hash_table_insert (fetch->uid_hash, (void *) camel_message_info_uid (info), envelope);
			
			envelope->changed |= IMAP_FETCH_UID;
		} else {
			/* wtf? */
			fprintf (stderr, "huh? %s?...\n", token->v.atom);
		}
	} while (1);
	
	if (token->token != ')')
		goto unexpected;
	
	return 0;
	
 unexpected:
	
	camel_imap_utils_set_unexpected_token_error (ex, engine, token);
	
 exception:
	
	return -1;
}

static CamelIMAPCommand *
imap_summary_fetch_all (CamelFolderSummary *summary, guint32 first, guint32 last)
{
	CamelIMAPSummary *imap_summary = (CamelIMAPSummary *) summary;
	CamelFolder *folder = imap_summary->folder;
	struct imap_fetch_all_t *fetch;
	CamelIMAPEngine *engine;
	CamelIMAPCommand *ic;
	
	engine = ((CamelIMAPStore *) folder->parent_store)->engine;
	
	fetch = g_new (struct imap_fetch_all_t, 1);
	fetch->uid_hash = g_hash_table_new (g_str_hash, g_str_equal);
	fetch->added = g_ptr_array_new ();
	fetch->summary = summary;
	
	/* From rfc2060, Section 6.4.5:
	 * 
	 * The currently defined data items that can be fetched are:
	 *
	 * ALL            Macro equivalent to: (FLAGS INTERNALDATE
	 *                RFC822.SIZE ENVELOPE)
	 **/
	
	if (last != 0)
		ic = camel_imap_engine_queue (engine, folder, "FETCH %u:%u (UID ALL)\r\n", first, last);
	else
		ic = camel_imap_engine_queue (engine, folder, "FETCH %u:* (UID ALL)\r\n", first);
	
	camel_imap_command_register_untagged (ic, "FETCH", untagged_fetch_all);
	ic->user_data = fetch;
	
	return ic;
}

static CamelIMAPCommand *
imap_summary_fetch_flags (CamelFolderSummary *summary, guint32 first, guint32 last)
{
	CamelIMAPSummary *imap_summary = (CamelIMAPSummary *) summary;
	CamelFolder *folder = imap_summary->folder;
	struct imap_fetch_all_t *fetch;
	CamelIMAPEngine *engine;
	CamelIMAPCommand *ic;
	
	engine = ((CamelIMAPStore *) folder->parent_store)->engine;
	
	fetch = g_new (struct imap_fetch_all_t, 1);
	fetch->uid_hash = g_hash_table_new (g_str_hash, g_str_equal);
	fetch->added = g_ptr_array_new ();
	fetch->summary = summary;
	
	if (last != 0)
		ic = camel_imap_engine_queue (engine, folder, "FETCH %u:%u (UID FLAGS)\r\n", first, last);
	else
		ic = camel_imap_engine_queue (engine, folder, "FETCH %u:* (UID FLAGS)\r\n", first);
	
	camel_imap_command_register_untagged (ic, "FETCH", untagged_fetch_all);
	ic->user_data = fetch;
	
	return ic;
}

#if 0
static int
imap_build_summary (CamelFolderSummary *summary, guint32 first, guint32 last)
{
	CamelIMAPSummary *imap_summary = (CamelIMAPSummary *) summary;
	CamelFolder *folder = imap_summary->folder;
	struct imap_fetch_all_t *fetch;
	CamelIMAPEngine *engine;
	CamelIMAPCommand *ic;
	int id;
	
	engine = ((CamelIMAPStore *) folder->store)->engine;
	
	ic = imap_summary_fetch_all (summary, first, last);
	while ((id = camel_imap_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	fetch = ic->user_data;
	
	if (id == -1 || ic->status != CAMEL_IMAP_COMMAND_COMPLETE) {
		camel_imap_command_unref (ic);
		imap_fetch_all_free (fetch);
		return -1;
	}
	
	imap_fetch_all_add (fetch);
	
	camel_imap_command_unref (ic);
	
	return 0;
}
#endif

static CamelMessageInfo *
imap_message_info_new (CamelFolderSummary *summary, struct _camel_header_raw *header)
{
	CamelMessageInfo *info;
	
	info = CAMEL_FOLDER_SUMMARY_CLASS (parent_class)->message_info_new (summary, header);
	
	((CamelIMAPMessageInfo *) info)->server_flags = 0;
	
	return info;
}

static CamelMessageInfo *
imap_message_info_load (CamelFolderSummary *summary, FILE *fin)
{
	CamelIMAPMessageInfo *minfo;
	CamelMessageInfo *info;
	
	if (!(info = CAMEL_FOLDER_SUMMARY_CLASS (parent_class)->message_info_load (summary, fin)))
		return NULL;
	
	minfo = (CamelIMAPMessageInfo *) info;
	
	if (camel_file_util_decode_uint32 (fin, &minfo->server_flags) == -1)
		goto exception;
	
	return info;
	
 exception:
	
	camel_folder_summary_info_free (summary, info);
	
	return NULL;
}

static int
imap_message_info_save (CamelFolderSummary *summary, FILE *fout, CamelMessageInfo *info)
{
	CamelIMAPMessageInfo *minfo = (CamelIMAPMessageInfo *) info;
	
	if (CAMEL_FOLDER_SUMMARY_CLASS (parent_class)->message_info_save (summary, fout, info) == -1)
		return -1;
	
	if (camel_file_util_encode_uint32 (fout, minfo->server_flags) == -1)
		return -1;
	
	return 0;
}


void
camel_imap_summary_set_exists (CamelFolderSummary *summary, guint32 exists)
{
	CamelIMAPSummary *imap_summary = (CamelIMAPSummary *) summary;
	
	g_return_if_fail (CAMEL_IS_IMAP_SUMMARY (summary));
	
	imap_summary->exists = exists;
	
	imap_summary->exists_changed = TRUE;
}

void
camel_imap_summary_set_recent (CamelFolderSummary *summary, guint32 recent)
{
	CamelIMAPSummary *imap_summary = (CamelIMAPSummary *) summary;
	
	g_return_if_fail (CAMEL_IS_IMAP_SUMMARY (summary));
	
	imap_summary->recent = recent;
}

void
camel_imap_summary_set_unseen (CamelFolderSummary *summary, guint32 unseen)
{
	CamelIMAPSummary *imap_summary = (CamelIMAPSummary *) summary;
	
	g_return_if_fail (CAMEL_IS_IMAP_SUMMARY (summary));
	
	imap_summary->unseen = unseen;
}

void
camel_imap_summary_set_uidnext (CamelFolderSummary *summary, guint32 uidnext)
{
	g_return_if_fail (CAMEL_IS_IMAP_SUMMARY (summary));
	
	summary->nextuid = uidnext;
}

void
camel_imap_summary_set_uidvalidity (CamelFolderSummary *summary, guint32 uidvalidity)
{
	CamelIMAPSummary *imap_summary = (CamelIMAPSummary *) summary;
	
	g_return_if_fail (CAMEL_IS_IMAP_SUMMARY (summary));
	
	if (imap_summary->uidvalidity == uidvalidity)
		return;
	
	/* FIXME: emit a signal or something first? */
	camel_folder_summary_clear (summary);
	
	imap_summary->uidvalidity = uidvalidity;
	
	imap_summary->uidvalidity_changed = TRUE;
}

void
camel_imap_summary_expunge (CamelFolderSummary *summary, int seqid)
{
	CamelMessageInfo *info;
	
	g_return_if_fail (CAMEL_IS_IMAP_SUMMARY (summary));
	
	if (!(info = camel_folder_summary_index (summary, seqid)))
		return;
	
	/* FIXME: emit a signal or something that our Folder can proxy
	 * up to the app so that it can update its display and
	 * whatnot? */
	
	/* emit signal */
	
	camel_folder_summary_info_free (summary, info);
	camel_folder_summary_remove_index (summary, seqid);
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

int
camel_imap_summary_flush_updates (CamelFolderSummary *summary, CamelException *ex)
{
	CamelIMAPSummary *imap_summary = (CamelIMAPSummary *) summary;
	CamelIMAPEngine *engine;
	CamelIMAPCommand *ic;
	guint32 first = 0;
	int scount, id;
	
	g_return_val_if_fail (CAMEL_IS_IMAP_SUMMARY (summary), -1);
	
	engine = ((CamelIMAPStore *) imap_summary->folder->parent_store)->engine;
	
	if (imap_summary->uidvalidity_changed) {
		first = 1;
	} else if (imap_summary->exists_changed) {
		scount = camel_folder_summary_count (summary);
		ic = imap_summary_fetch_flags (summary, 1, scount);
		
		while ((id = camel_imap_engine_iterate (engine)) < ic->id && id != -1)
			;
		
		if (id == -1 || ic->status != CAMEL_IMAP_COMMAND_COMPLETE) {
			imap_fetch_all_free (ic->user_data);
			camel_exception_xfer (ex, &ic->ex);
			camel_imap_command_unref (ic);
			return -1;
		}
		
		if (!(first = imap_fetch_all_update (ic->user_data)) && imap_summary->exists > scount)
			first = scount + 1;
		
		camel_imap_command_unref (ic);
	}
	
	if (first != 0) {
		ic = imap_summary_fetch_all (summary, first, 0);
		
		while ((id = camel_imap_engine_iterate (engine)) < ic->id && id != -1)
			;
		
		if (id == -1 || ic->status != CAMEL_IMAP_COMMAND_COMPLETE) {
			imap_fetch_all_free (ic->user_data);
			camel_exception_xfer (ex, &ic->ex);
			camel_imap_command_unref (ic);
			return -1;
		}
		
		imap_fetch_all_add (ic->user_data);
		camel_imap_command_unref (ic);
		
		/* it's important for these to be sorted sequentially for EXPUNGE events to work */
		g_ptr_array_sort (summary->messages, (GCompareFunc) info_uid_sort);
	}
	
	imap_summary->exists_changed = FALSE;
	imap_summary->uidvalidity_changed = FALSE;
	
	return 0;
}
