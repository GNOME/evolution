/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <iconv.h>

#include "camel-imap-utils.h"
#include "camel-imap-summary.h"
#include "camel-imap-store.h"
#include "camel-folder.h"

#define d(x) x

const char *
imap_next_word (const char *buf)
{
	const char *word;
	
	/* skip over current word */
	for (word = buf; *word && *word != ' '; word++);
	
	/* skip over white space */
	for ( ; *word && *word == ' '; word++);
	
	return word;
}

/**
 * imap_parse_list_response:
 * @store: the IMAP store whose list response we're parsing
 * @buf: the LIST or LSUB response
 * @flags: a pointer to a variable to store the flags in, or %NULL
 * @sep: a pointer to a variable to store the hierarchy separator in, or %NULL
 * @folder: a pointer to a variable to store the folder name in, or %NULL
 *
 * Parses a LIST or LSUB response and returns the desired parts of it.
 * If @folder is non-%NULL, its value must be freed by the caller.
 *
 * Return value: whether or not the response was successfully parsed.
 **/
gboolean
imap_parse_list_response (CamelImapStore *store, const char *buf, int *flags, char *sep, char **folder)
{
	const char *word;
	size_t len;
	
	if (*buf != '*')
		return FALSE;
	
	word = imap_next_word (buf);
	if (g_strncasecmp (word, "LIST", 4) && g_strncasecmp (word, "LSUB", 4))
		return FALSE;
	
	/* get the flags */
	word = imap_next_word (word);
	if (*word != '(')
		return FALSE;
	
	if (flags)
		*flags = 0;
	
	word++;
	while (*word != ')') {
		len = strcspn (word, " )");
		if (flags) {
			if (!g_strncasecmp (word, "\\NoInferiors", len))
				*flags |= IMAP_LIST_FLAG_NOINFERIORS;
			else if (!g_strncasecmp (word, "\\NoSelect", len))
				*flags |= IMAP_LIST_FLAG_NOSELECT;
			else if (!g_strncasecmp (word, "\\Marked", len))
				*flags |= IMAP_LIST_FLAG_MARKED;
			else if (!g_strncasecmp (word, "\\Unmarked", len))
				*flags |= IMAP_LIST_FLAG_UNMARKED;
		}
		
		word += len;
		while (*word == ' ')
			word++;
	}
	
	/* get the directory separator */
	word = imap_next_word (word);
	if (!strncmp (word, "NIL", 3)) {
		if (sep)
			*sep = '\0';
	} else if (*word++ == '"') {
		if (*word == '\\')
			word++;
		if (sep)
			*sep = *word;
		word++;
		if (*word++ != '"')
			return FALSE;
	} else
		return FALSE;
	
	if (folder) {
		char *astring, *mailbox;
		size_t nlen;
		
		/* get the folder name */
		word = imap_next_word (word);
		astring = imap_parse_astring ((char **) &word, &len);
		if (!astring)
			return FALSE;
		
		mailbox = imap_mailbox_decode (astring, strlen (astring));
		g_free (astring);
		if (!mailbox)
			return FALSE;
		
		nlen = strlen (store->namespace);
		
		if (!strncmp (mailbox, store->namespace, nlen)) {
			/* strip off the namespace */
			if (nlen > 0)
				memmove (mailbox, mailbox + nlen, (len - nlen) + 1);
			*folder = mailbox;
		} else if (!g_strcasecmp (mailbox, "INBOX")) {
			*folder = mailbox;
		} else {
			g_warning ("IMAP folder name \"%s\" does not begin with \"%s\"", mailbox, store->namespace);
			*folder = mailbox;
		}
		
		return *folder != NULL;
	}
	
	return TRUE;
}


/**
 * imap_parse_folder_name:
 * @store:
 * @folder_name:
 *
 * Return an array of folder paths representing the folder heirarchy.
 * For example:
 *   Full/Path/"to / from"/Folder
 * Results in:
 *   Full, Full/Path, Full/Path/"to / from", Full/Path/"to / from"/Folder
 **/
char **
imap_parse_folder_name (CamelImapStore *store, const char *folder_name)
{
	GPtrArray *heirarchy;
	char **paths;
	const char *p;
	
	p = folder_name;
	if (*p == store->dir_sep)
		p++;
	
	heirarchy = g_ptr_array_new ();
	
	while (*p) {
		if (*p == '"') {
			p++;
			while (*p && *p != '"')
				p++;
			if (*p)
				p++;
			continue;
		}
		
		if (*p == store->dir_sep)
			g_ptr_array_add (heirarchy, g_strndup (folder_name, p - folder_name));
		
		p++;
	}
	
	g_ptr_array_add (heirarchy, g_strdup (folder_name));
	g_ptr_array_add (heirarchy, NULL);
	
	paths = (char **) heirarchy->pdata;
	g_ptr_array_free (heirarchy, FALSE);
	
	return paths;
}

char *
imap_create_flag_list (guint32 flags)
{
	GString *gstr;
	char *flag_list;
	
	gstr = g_string_new ("(");
	
	if (flags & CAMEL_MESSAGE_ANSWERED)
		g_string_append (gstr, "\\Answered ");
	if (flags & CAMEL_MESSAGE_DELETED)
		g_string_append (gstr, "\\Deleted ");
	if (flags & CAMEL_MESSAGE_DRAFT)
		g_string_append (gstr, "\\Draft ");
	if (flags & CAMEL_MESSAGE_FLAGGED)
		g_string_append (gstr, "\\Flagged ");
	if (flags & CAMEL_MESSAGE_SEEN)
		g_string_append (gstr, "\\Seen ");
	
	if (gstr->str[gstr->len - 1] == ' ')
		gstr->str[gstr->len - 1] = ')';
	else
		g_string_append_c (gstr, ')');
	
	flag_list = gstr->str;
	g_string_free (gstr, FALSE);
	return flag_list;
}

guint32
imap_parse_flag_list (char **flag_list_p)
{
	char *flag_list = *flag_list_p;
	guint32 flags = 0;
	int len;
	
	if (*flag_list++ != '(') {
		*flag_list_p = NULL;
		return 0;
	}
	
	while (*flag_list && *flag_list != ')') {
		len = strcspn (flag_list, " )");
		if (!g_strncasecmp (flag_list, "\\Answered", len))
			flags |= CAMEL_MESSAGE_ANSWERED;
		else if (!g_strncasecmp (flag_list, "\\Deleted", len))
			flags |= CAMEL_MESSAGE_DELETED;
		else if (!g_strncasecmp (flag_list, "\\Draft", len))
			flags |= CAMEL_MESSAGE_DRAFT;
		else if (!g_strncasecmp (flag_list, "\\Flagged", len))
			flags |= CAMEL_MESSAGE_FLAGGED;
		else if (!g_strncasecmp (flag_list, "\\Seen", len))
			flags |= CAMEL_MESSAGE_SEEN;
		else if (!g_strncasecmp (flag_list, "\\Recent", len))
			flags |= CAMEL_IMAP_MESSAGE_RECENT;
		
		flag_list += len;
		if (*flag_list == ' ')
			flag_list++;
	}
	
	if (*flag_list++ != ')') {
		*flag_list_p = NULL;
		return 0;
	}
	
	*flag_list_p = flag_list;
	return flags;
}

static char imap_atom_specials[128] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 
};
#define imap_is_atom_char(ch) (isprint (ch) && !imap_atom_specials[ch])

/**
 * imap_parse_string_generic:
 * @str_p: a pointer to a string
 * @len: a pointer to a size_t to return the length in
 * @type: type of string (#IMAP_STRING, #IMAP_ASTRING, or #IMAP_NSTRING)
 * to parse.
 *
 * This parses an IMAP "string" (quoted string or literal), "nstring"
 * (NIL or string), or "astring" (atom or string) starting at *@str_p.
 * On success, *@str_p will point to the first character after the end
 * of the string, and *@len will contain the length of the returned
 * string. On failure, *@str_p will be set to %NULL.
 *
 * This assumes that the string is in the form returned by
 * camel_imap_command(): that line breaks are indicated by LF rather
 * than CRLF.
 *
 * Return value: the parsed string, or %NULL if a NIL or no string
 * was parsed. (In the former case, *@str_p will be %NULL; in the
 * latter, it will point to the character after the NIL.)
 **/
char *
imap_parse_string_generic (char **str_p, size_t *len, int type)
{
	char *str = *str_p;
	char *out;
	
	if (!str)
		return NULL;
	else if (*str == '"') {
		char *p;
		size_t size;
		
		str++;
		size = strcspn (str, "\"") + 1;
		p = out = g_malloc (size);
		
		/* a quoted string cannot be broken into multiple lines */
		while (*str && *str != '"' && *str != '\n') {
			if (*str == '\\')
				str++;
			*p++ = *str++;
			if (p - out == size) {
				out = g_realloc (out, size * 2);
				p = out + size;
				size *= 2;
			}
		}
		if (*str != '"') {
			*str_p = NULL;
			g_free (out);
			return NULL;
		}
		*p = '\0';
		*str_p = str + 1;
		*len = strlen (out);
		return out;
	} else if (*str == '{') {
		*len = strtoul (str + 1, (char **)&str, 10);
		if (*str++ != '}' || *str++ != '\n' || strlen (str) < *len) {
			*str_p = NULL;
			return NULL;
		}
		
		out = g_strndup (str, *len);
		*str_p = str + *len;
		return out;
	} else if (type == IMAP_NSTRING && !g_strncasecmp (str, "nil", 3)) {
		*str_p += 3;
		*len = 0;
		return NULL;
	} else if (type == IMAP_ASTRING && imap_is_atom_char ((unsigned char)*str)) {
		while (imap_is_atom_char ((unsigned char)*str))
			str++;
		
		*len = str - *str_p;
		str = g_strndup (*str_p, *len);
		*str_p += *len;
		return str;
	} else {
		*str_p = NULL;
		return NULL;
	}
}

static inline void
skip_char (char **str_p, char ch)
{
	if (*str_p && **str_p == ch)
		*str_p = *str_p + 1;
	else
		*str_p = NULL;
}

/* Skip atom, string, or number */
static void
skip_asn (char **str_p)
{
	char *str = *str_p;
	
	if (!str)
		return;
	else if (*str == '"') {
		while (*++str && *str != '"') {
			if (*str == '\\') {
				str++;
				if (!*str)
					break;
			}
		}
		if (*str == '"')
			*str_p = str + 1;
		else
			*str_p = NULL;
	} else if (*str == '{') {
		unsigned long len;
		
		len = strtoul (str + 1, &str, 10);
		if (*str != '}' || *(str + 1) != '\n' ||
		    strlen (str + 2) < len) {
			*str_p = NULL;
			return;
		}
		*str_p = str + 2 + len;
	} else {
		/* We assume the string is well-formed and don't
		 * bother making sure it's a valid atom.
		 */
		while (*str && *str != ')' && *str != ' ')
			str++;
		*str_p = str;
	}
}

void
imap_skip_list (char **str_p)
{
	skip_char (str_p, '(');
	while (*str_p && **str_p != ')') {
		if (**str_p == '(')
			imap_skip_list (str_p);
		else
			skip_asn (str_p);
		if (*str_p && **str_p == ' ')
			skip_char (str_p, ' ');
	}
	skip_char (str_p, ')');
}

static void
parse_params (char **parms_p, CamelContentType *type)
{
	char *parms = *parms_p, *name, *value;
	int len;
	
	if (!g_strncasecmp (parms, "nil", 3)) {
		*parms_p += 3;
		return;
	}
	
	if (*parms++ != '(') {
		*parms_p = NULL;
		return;
	}
	
	while (parms && *parms != ')') {
		name = imap_parse_nstring (&parms, &len);
		skip_char (&parms, ' ');
		value = imap_parse_nstring (&parms, &len);
		
		if (name && value)
			header_content_type_set_param (type, name, value);
		g_free (name);
		g_free (value);
		
		if (parms && *parms == ' ')
			parms++;
	}
	
	if (!parms || *parms++ != ')') {
		*parms_p = NULL;
		return;
	}
	*parms_p = parms;
}

/**
 * imap_parse_body:
 * @body_p: pointer to the start of an IMAP "body"
 * @folder: an imap folder
 * @ci: a CamelMessageContentInfo to fill in
 *
 * This filles in @ci with data from *@body_p. On success *@body_p
 * will point to the character after the body. On failure, it will be
 * set to %NULL and @ci will be unchanged.
 **/
void
imap_parse_body (char **body_p, CamelFolder *folder,
		 CamelMessageContentInfo *ci)
{
	char *body = *body_p;
	CamelMessageContentInfo *child;
	CamelContentType *type;
	size_t len;
	
	if (!body || *body++ != '(') {
		*body_p = NULL;
		return;
	}
	
	if (*body == '(') {
		/* multipart */
		GPtrArray *children;
		char *subtype;
		int i;
		
		/* Parse the child body parts */
		children = g_ptr_array_new ();
		i = 0;
		while (body && *body == '(') {
			child = camel_folder_summary_content_info_new (folder->summary);
			g_ptr_array_add (children, child);
			imap_parse_body (&body, folder, child);
			if (!body)
				break;
			child->parent = ci;
		}
		skip_char (&body, ' ');
		
		/* Parse the multipart subtype */
		subtype = imap_parse_string (&body, &len);
		
		/* If there is a parse error, abort. */
		if (!body) {
			for (i = 0; i < children->len; i++) {
				child = children->pdata[i];
				camel_folder_summary_content_info_free (folder->summary, child);
			}
			g_ptr_array_free (children, TRUE);
			*body_p = NULL;
			return;
		}
		
		g_strdown (subtype);
		ci->type = header_content_type_new ("multipart", subtype);
		g_free (subtype);
		
		/* Chain the children. */
		ci->childs = children->pdata[0];
		ci->size = 0;
		for (i = 0; i < children->len - 1; i++) {
			child = children->pdata[i];
			child->next = children->pdata[i + 1];
			ci->size += child->size;
		}
		g_ptr_array_free (children, TRUE);
	} else {
		/* single part */
		char *main_type, *subtype;
		char *id, *description, *encoding;
		guint32 size = 0;
		
		main_type = imap_parse_string (&body, &len);
		skip_char (&body, ' ');
		subtype = imap_parse_string (&body, &len);
		skip_char (&body, ' ');
		if (!body) {
			g_free (main_type);
			g_free (subtype);
			*body_p = NULL;
			return;
		}
		g_strdown (main_type);
		g_strdown (subtype);
		type = header_content_type_new (main_type, subtype);
		g_free (main_type);
		g_free (subtype);
		parse_params (&body, type);
		skip_char (&body, ' ');
		
		id = imap_parse_nstring (&body, &len);
		skip_char (&body, ' ');
		description = imap_parse_nstring (&body, &len);
		skip_char (&body, ' ');
		encoding = imap_parse_string (&body, &len);
		skip_char (&body, ' ');
		if (body)
			size = strtoul (body, &body, 10);
		
		child = NULL;
		if (header_content_type_is (type, "message", "rfc822")) {
			skip_char (&body, ' ');
			imap_skip_list (&body); /* envelope */
			skip_char (&body, ' ');
			child = camel_folder_summary_content_info_new (folder->summary);
			imap_parse_body (&body, folder, child);
			if (!body)
				camel_folder_summary_content_info_free (folder->summary, child);
			skip_char (&body, ' ');
			if (body)
				strtoul (body, &body, 10);
		} else if (header_content_type_is (type, "text", "*")) {
			if (body)
				strtoul (body, &body, 10);
		}
		
		if (body) {
			ci->type = type;
			ci->id = id;
			ci->description = description;
			ci->encoding = encoding;
			ci->size = size;
			ci->childs = child;
		} else {
			header_content_type_unref (type);
			g_free (id);
			g_free (description);
			g_free (encoding);
		}
	}
	
	if (!body || *body++ != ')') {
		*body_p = NULL;
		return;
	}
	
	*body_p = body;
}

/**
 * imap_quote_string:
 * @str: the string to quote, which must not contain CR or LF
 *
 * Return value: an IMAP "quoted" corresponding to the string, which
 * the caller must free.
 **/
char *
imap_quote_string (const char *str)
{
	const char *p;
	char *quoted, *q;
	int len;
	
	g_assert (strchr (str, '\r') == NULL);
	
	len = strlen (str);
	p = str;
	while ((p = strpbrk (p, "\"\\"))) {
		len++;
		p++;
	}
	
	quoted = q = g_malloc (len + 3);
	*q++ = '"';
	for (p = str; *p; ) {
		if (strchr ("\"\\", *p))
			*q++ = '\\';
		*q++ = *p++;
	}
	*q++ = '"';
	*q = '\0';
	
	return quoted;
}


static inline unsigned long
get_summary_uid_numeric (CamelFolderSummary *summary, int index)
{
	CamelMessageInfo *info;
	unsigned long uid;
	
	info = camel_folder_summary_index (summary, index);
	uid = strtoul (camel_message_info_uid (info), NULL, 10);
	camel_folder_summary_info_free (summary, info);
	return uid;
}

/* the max number of chars that an unsigned 32-bit int can be is 10 chars plus 1 for a possible : */
#define UID_SET_FULL(setlen, maxlen) (maxlen > 0 ? setlen + 11 >= maxlen : FALSE)

/**
 * imap_uid_array_to_set:
 * @summary: summary for the folder the UIDs come from
 * @uids: a (sorted) array of UIDs
 * @uid: uid index to start at
 * @maxlen: max length of the set string (or -1 for infinite)
 * @lastuid: index offset of the last uid used
 *
 * Creates an IMAP "set" up to @maxlen bytes long, covering the listed
 * UIDs starting at index @uid and not covering any UIDs that are in
 * @summary but not in @uids. It doesn't actually require that all (or
 * any) of the UIDs be in @summary.
 *
 * After calling, @lastuid will be set the index of the first uid
 * *not* included in the returned set string.
 * 
 * Return value: the set, which the caller must free with g_free()
 **/
char *
imap_uid_array_to_set (CamelFolderSummary *summary, GPtrArray *uids, int uid, ssize_t maxlen, int *lastuid)
{
	unsigned long last_uid, next_summary_uid, this_uid;
	gboolean range = FALSE;
	int si, scount;
	GString *gset;
	char *set;
	
	g_return_val_if_fail (uids->len > uid, NULL);
	
	gset = g_string_new (uids->pdata[uid]);
	last_uid = strtoul (uids->pdata[uid], NULL, 10);
	next_summary_uid = 0;
	scount = camel_folder_summary_count (summary);
	
	for (uid++, si = 0; uid < uids->len && !UID_SET_FULL (gset->len, maxlen); uid++) {
		/* Find the next UID in the summary after the one we
		 * just wrote out.
		 */
		for ( ; last_uid >= next_summary_uid && si < scount; si++)
			next_summary_uid = get_summary_uid_numeric (summary, si);
		if (last_uid >= next_summary_uid)
			next_summary_uid = (unsigned long) -1;
		
		/* Now get the next UID from @uids */
		this_uid = strtoul (uids->pdata[uid], NULL, 10);
		if (this_uid == next_summary_uid || this_uid == last_uid + 1)
			range = TRUE;
		else {
			if (range) {
				g_string_sprintfa (gset, ":%lu", last_uid);
				range = FALSE;
			}
			g_string_sprintfa (gset, ",%lu", this_uid);
		}
		
		last_uid = this_uid;
	}
	
	if (range)
		g_string_sprintfa (gset, ":%lu", last_uid);
	
	*lastuid = uid;
	
	set = gset->str;
	g_string_free (gset, FALSE);
	
	return set;
}

/**
 * imap_uid_set_to_array:
 * @summary: summary for the folder the UIDs come from
 * @uids: a pointer to the start of an IMAP "set" of UIDs
 *
 * Fills an array with the UIDs corresponding to @uids and @summary.
 * There can be text after the uid set in @uids, which will be
 * ignored.
 *
 * If @uids specifies a range of UIDs that extends outside the range
 * of @summary, the function will assume that all of the "missing" UIDs
 * do exist.
 *
 * Return value: the array of uids, which the caller must free with
 * imap_uid_array_free(). (Or %NULL if the uid set can't be parsed.)
 **/
GPtrArray *
imap_uid_set_to_array (CamelFolderSummary *summary, const char *uids)
{
	GPtrArray *arr;
	char *p, *q;
	unsigned long uid, suid;
	int si, scount;
	
	arr = g_ptr_array_new ();
	scount = camel_folder_summary_count (summary);
	
	p = (char *)uids;
	si = 0;
	do {
		uid = strtoul (p, &q, 10);
		if (p == q)
			goto lose;
		g_ptr_array_add (arr, g_strndup (p, q - p));
		
		if (*q == ':') {
			/* Find the summary entry for the UID after the one
			 * we just saw.
			 */
			while (++si < scount) {
				suid = get_summary_uid_numeric (summary, si);
				if (suid > uid)
					break;
			}
			if (si >= scount)
				suid = uid + 1;
			
			uid = strtoul (q + 1, &p, 10);
			if (p == q + 1)
				goto lose;
			
			/* Add each summary UID until we find one
			 * larger than the end of the range
			 */
			while (suid <= uid) {
				g_ptr_array_add (arr, g_strdup_printf ("%lu", suid));
				if (++si < scount)
					suid = get_summary_uid_numeric (summary, si);
				else
					suid++;
			}
		} else
			p = q;
	} while (*p++ == ',');
	
	return arr;
	
 lose:
	g_warning ("Invalid uid set %s", uids);
	imap_uid_array_free (arr);
	return NULL;
}

/**
 * imap_uid_array_free:
 * @arr: an array returned from imap_uid_set_to_array()
 *
 * Frees @arr
 **/
void
imap_uid_array_free (GPtrArray *arr)
{
	int i;
	
	for (i = 0; i < arr->len; i++)
		g_free (arr->pdata[i]);
	g_ptr_array_free (arr, TRUE);
}

char *
imap_concat (CamelImapStore *imap_store, const char *prefix, const char *suffix)
{
	size_t len;
	
	len = strlen (prefix);
	if (len == 0 || prefix[len - 1] == imap_store->dir_sep)
		return g_strdup_printf ("%s%s", prefix, suffix);
	else
		return g_strdup_printf ("%s%c%s", prefix, imap_store->dir_sep, suffix);
}

char *
imap_namespace_concat (CamelImapStore *store, const char *name)
{
	if (!name || *name == '\0') {
		if (store->namespace)
			return g_strdup (store->namespace);
		else
			return g_strdup ("");
	}
	
	if (!g_strcasecmp (name, "INBOX"))
		return g_strdup ("INBOX");
	
	if (store->namespace == NULL) {
		g_warning ("Trying to concat NULL namespace to \"%s\"!", name);
		return g_strdup (name);
	}
	
	return imap_concat (store, store->namespace, name);
}


#define UTF8_TO_UTF7_LEN(len)  ((len * 3) + 8)
#define UTF7_TO_UTF8_LEN(len)  (len)

enum {
	MODE_USASCII,
	MODE_AMPERSAND,
	MODE_MODUTF7
};

#define is_usascii(c)  (((c) >= 0x20 && (c) <= 0x25) || ((c) >= 0x27 && (c) <= 0x7e))
#define encode_mode(c) (is_usascii (c) ? MODE_USASCII : (c) == '&' ? MODE_AMPERSAND : MODE_MODUTF7)

char *
imap_mailbox_encode (const unsigned char *in, size_t inlen)
{
	const unsigned char *start, *inptr, *inend;
	unsigned char *mailbox, *m, *mend;
	size_t inleft, outleft, conv;
	char *inbuf, *outbuf;
	iconv_t cd;
	int mode;
	
	cd = (iconv_t) -1;
	m = mailbox = g_malloc (UTF8_TO_UTF7_LEN (inlen) + 1);
	mend = mailbox + UTF8_TO_UTF7_LEN (inlen);
	
	start = inptr = in;
	inend = in + inlen;
	mode = MODE_USASCII;
	
	while (inptr < inend) {
		int new_mode;
		
		new_mode = encode_mode (*inptr);
		
		if (new_mode != mode) {
			switch (mode) {
			case MODE_USASCII:
				memcpy (m, start, inptr - start);
				m += (inptr - start);
				break;
			case MODE_AMPERSAND:
				while (start < inptr) {
					*m++ = '&';
					*m++ = '-';
					start++;
				}
				break;
			case MODE_MODUTF7:
				inbuf = (char *) start;
				inleft = inptr - start;
				outbuf = (char *) m;
				outleft = mend - m;
				
				if (cd == (iconv_t) -1)
					cd = iconv_open ("UTF-7", "UTF-8");
				
				conv = iconv (cd, &inbuf, &inleft, &outbuf, &outleft);
				if (conv == (size_t) -1) {
					g_warning ("error converting mailbox to UTF-7!");
				}
				iconv (cd, NULL, NULL, &outbuf, &outleft);
				
				/* shift into modified UTF-7 mode (overwrite UTF-7's '+' shift)... */
				*m++ = '&';
				
				while (m < (unsigned char *) outbuf) {
					/* replace '/' with ',' */
					if (*m == '/')
						*m = ',';
					
					m++;
				}
				
				break;
			}
			
			mode = new_mode;
			start = inptr;
		}
		
		inptr++;
	}
	
	switch (mode) {
	case MODE_USASCII:
		memcpy (m, start, inptr - start);
		m += (inptr - start);
		break;
	case MODE_AMPERSAND:
		while (start < inptr) {
			*m++ = '&';
			*m++ = '-';
			start++;
		}
		break;
	case MODE_MODUTF7:
		inbuf = (char *) start;
		inleft = inptr - start;
		outbuf = (char *) m;
		outleft = mend - m;
		
		if (cd == (iconv_t) -1)
			cd = iconv_open ("UTF-7", "UTF-8");
		
		conv = iconv (cd, &inbuf, &inleft, &outbuf, &outleft);
		if (conv == (size_t) -1) {
			g_warning ("error converting mailbox to UTF-7!");
		}
		iconv (cd, NULL, NULL, &outbuf, &outleft);
		
		/* shift into modified UTF-7 mode (overwrite UTF-7's '+' shift)... */
		*m++ = '&';
		
		while (m < (unsigned char *) outbuf) {
			/* replace '/' with ',' */
			if (*m == '/')
				*m = ',';
			
			m++;
		}
		
		break;
	}
	
	*m = '\0';
	
	if (cd != (iconv_t) -1)
		iconv_close (cd);
	
	return mailbox;
}


char *
imap_mailbox_decode (const unsigned char *in, size_t inlen)
{
	const unsigned char *start, *inptr, *inend;
	unsigned char *mailbox, *m, *mend;
	unsigned char mode_switch;
	iconv_t cd;
	
	cd = (iconv_t) -1;
	m = mailbox = g_malloc (UTF7_TO_UTF8_LEN (inlen) + 1);
	mend = mailbox + UTF7_TO_UTF8_LEN (inlen);
	
	start = inptr = in;
	inend = in + inlen;
	mode_switch = '&';
	
	while (inptr < inend) {
		if (*inptr == mode_switch) {
			if (mode_switch == '&') {
				/* mode switch from US-ASCII to UTF-7 */
				mode_switch = '-';
				memcpy (m, start, inptr - start);
				m += (inptr - start);
				start = inptr;
			} else if (mode_switch == '-') {
				/* mode switch from UTF-7 to US-ASCII or an ampersand (&) */
				mode_switch = '&';
				start++;
				if (start == inptr) {
					/* we had the sequence "&-" which becomes "&" when decoded */
					*m++ = '&';
				} else {
					char *buffer, *inbuf, *outbuf;
					size_t buflen, outleft, conv;
					
					buflen = (inptr - start) + 2;
					inbuf = buffer = alloca (buflen);
					*inbuf++ = '+';
					while (start < inptr) {
						*inbuf++ = *start == ',' ? '/' : *start;
						start++;
					}
					*inbuf = '-';
					
					inbuf = buffer;
					outbuf = (char *) m;
					outleft = mend - m;
					
					if (cd == (iconv_t) -1)
						cd = iconv_open ("UTF-8", "UTF-7");
					
					conv = iconv (cd, &inbuf, &buflen, &outbuf, &outleft);
					if (conv == (size_t) -1) {
						g_warning ("error decoding mailbox: %.*s", inlen, in);
					}
					iconv (cd, NULL, NULL, NULL, NULL);
					
					m = (unsigned char *) outbuf;
				}
				
				/* point to the char after the '-' */
				start = inptr + 1;
			}
		}
		
		inptr++;
	}
	
	if (*inptr == mode_switch) {
		if (mode_switch == '&') {
			/* the remaining text is US-ASCII */
			memcpy (m, start, inptr - start);
			m += (inptr - start);
			start = inptr;
		} else if (mode_switch == '-') {
			/* We've got encoded UTF-7 or else an ampersand */
			start++;
			if (start == inptr) {
				/* we had the sequence "&-" which becomes "&" when decoded */
				*m++ = '&';
			} else {
				char *buffer, *inbuf, *outbuf;
				size_t buflen, outleft, conv;
				
				buflen = (inptr - start) + 2;
				inbuf = buffer = alloca (buflen);
				*inbuf++ = '+';
				while (start < inptr) {
					*inbuf++ = *start == ',' ? '/' : *start;
					start++;
				}
				*inbuf = '-';
				
				inbuf = buffer;
				outbuf = (char *) m;
				outleft = mend - m;
				
				if (cd == (iconv_t) -1)
					cd = iconv_open ("UTF-8", "UTF-7");
				
				conv = iconv (cd, &inbuf, &buflen, &outbuf, &outleft);
				if (conv == (size_t) -1) {
					g_warning ("error decoding mailbox: %.*s", inlen, in);
				}
				iconv (cd, NULL, NULL, NULL, NULL);
				
				m = (unsigned char *) outbuf;
			}
		}
	} else {
		if (mode_switch == '-') {
			/* illegal encoded mailbox... */
			g_warning ("illegal mailbox name encountered: %.*s", inlen, in);
		}
		
		memcpy (m, start, inptr - start);
		m += (inptr - start);
	}
	
	*m = '\0';
	
	if (cd != (iconv_t) -1)
		iconv_close (cd);
	
	return mailbox;
}
