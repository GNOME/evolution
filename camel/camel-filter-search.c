/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *	     Michael Zucchi <NotZed@Ximian.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
 *  Copyright 2001 Ximian Inc. (www.ximian.com)
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

/* (from glibc headers:
   POSIX says that <sys/types.h> must be included (by the caller) before <regex.h>.  */
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <ctype.h>

#warning "Fixme: remove gal/widgets/e-unicode dependency"
#include <gal/widgets/e-unicode.h>

#include "e-util/e-sexp.h"

#include "camel-mime-message.h"
#include "camel-filter-search.h"
#include "camel-exception.h"
#include "camel-multipart.h"
#include "camel-stream-mem.h"

#define d(x)

typedef struct {
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	const char *source;
	CamelException *ex;
} FilterMessageSearch;

/* ESExp callbacks */
static ESExpResult *header_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_matches (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_starts_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_ends_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_exists (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_soundex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_full_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *match_all (struct _ESExp *f, int argc, struct _ESExpTerm **argv, FilterMessageSearch *fms);
static ESExpResult *body_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *body_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *user_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *user_tag (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *system_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *get_sent_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *get_received_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *get_current_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *get_score (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *get_source (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);

/* builtin functions */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "match-all",          (ESExpFunc *) match_all,          1 },
	{ "body-contains",      (ESExpFunc *) body_contains,      0 },
	{ "body-regex",         (ESExpFunc *) body_regex,         0 },
	{ "header-contains",    (ESExpFunc *) header_contains,    0 },
	{ "header-matches",     (ESExpFunc *) header_matches,     0 },
	{ "header-starts-with", (ESExpFunc *) header_starts_with, 0 },
	{ "header-ends-with",   (ESExpFunc *) header_ends_with,   0 },
	{ "header-exists",      (ESExpFunc *) header_exists,      0 },
	{ "header-soundex",     (ESExpFunc *) header_soundex,     0 },
	{ "header-regex",       (ESExpFunc *) header_regex,       0 },
	{ "header-full-regex",  (ESExpFunc *) header_full_regex,  0 },
	{ "user-tag",           (ESExpFunc *) user_tag,           0 },
	{ "user-flag",          (ESExpFunc *) user_flag,          0 },
	{ "system-flag",        (ESExpFunc *) system_flag,        0 },
	{ "get-sent-date",      (ESExpFunc *) get_sent_date,      0 },
	{ "get-received-date",  (ESExpFunc *) get_received_date,  0 },
	{ "get-current-date",   (ESExpFunc *) get_current_date,   0 },
	{ "get-score",          (ESExpFunc *) get_score,          0 },
	{ "get-source",         (ESExpFunc *) get_source,         0 },
};

/* builds the regex into pattern */
/* taken from camel-folder-search, with added isregex & exception parameter */
/* Basically, we build a new regex, either based on subset regex's, or substrings,
   that can be executed once over the whoel body, to match anything suitable.
   This is more efficient than multiple searches, and probably most (naive) strstr
   implementations, over long content.

   A small issue is that case-insenstivity wont work entirely correct for utf8 strings. */
static int
build_match_regex(regex_t *pattern, int isregex, int argc, struct _ESExpResult **argv, CamelException *ex)
{
	GString *match = g_string_new("");
	int c, i, count=0, err;
	char *word;

	/* build a regex pattern we can use to match the words, we OR them together */
	if (argc>1)
		g_string_append_c(match, '(');
	for (i=0;i<argc;i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			if (count > 0)
				g_string_append_c(match, '|');
			/* escape any special chars (not sure if this list is complete) */
			word = argv[i]->value.string;
			if (isregex) {
				g_string_append(match, word);
			} else {
				while ((c = *word++)) {
					if (strchr("*\\.()[]^$+", c) != NULL) {
						g_string_append_c(match, '\\');
					}
					g_string_append_c(match, c);
				}
			}
			count++;
		} else {
			g_warning("Invalid type passed to body-contains match function");
		}
	}
	if (argc>1)
		g_string_append_c(match, ')');
	err = regcomp(pattern, match->str, REG_EXTENDED|REG_ICASE|REG_NOSUB);
	if (err != 0) {
		/* regerror gets called twice to get the full error string 
		   length to do proper posix error reporting */
		int len = regerror(err, pattern, 0, 0);
		char *buffer = g_malloc0(len + 1);

		regerror(err, pattern, buffer, len);
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Regular expression compilation failed: %s: %s"),
				     match->str, buffer);

		regfree(pattern);
	}
	d(printf("Built regex: '%s'\n", match->str));
	g_string_free(match, TRUE);
	return err;
}

static unsigned char soundex_table[256] = {
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0, 49, 50, 51,  0, 49, 50,  0,  0, 50, 50, 52, 53, 53,  0,
	 49, 50, 54, 50, 51,  0, 49,  0, 50,  0, 50,  0,  0,  0,  0,  0,
	  0,  0, 49, 50, 51,  0, 49, 50,  0,  0, 50, 50, 52, 53, 53,  0,
	 49, 50, 54, 50, 51,  0, 49,  0, 50,  0, 50,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static void
soundexify (const gchar *sound, gchar code[5])
{
	guchar *c, last = '\0';
	gint n;
	
	for (c = (guchar *) sound; *c && !isalpha (*c); c++);
	code[0] = toupper (*c);
	memset (code + 1, '0', 3);
	for (n = 1; *c && n < 5; c++) {
		guchar ch = soundex_table[*c];
		
		if (ch && ch != last) {
			code[n++] = ch;
			last = ch;
		}
	}
	code[4] = '\0';
}

static gint
soundexcmp (const gchar *sound1, const gchar *sound2)
{
	gchar code1[5], code2[5];
	
	soundexify (sound1, code1);
	soundexify (sound2, code2);
	
	return strcmp (code1, code2);
}

static gboolean check_match(const char *value, const char *match, int how)
{
	const char *p;

	while (*value && isspace(*value))
		value++;

	if (strlen(value) < strlen(match))
		return FALSE;

	/* from dan the man, if we have mixed case, perform a case-sensitive match,
	   otherwise not */
	p = match;
	while (*p) {
		if (isupper(*p)) {
			switch(how) {
			case 0:	/* is */
				return strcmp(value, match) == 0;
			case 1:	/* contains */
				return strstr(value, match) != NULL;
			case 2:	/* starts with */
				return strncmp(value, match, strlen(match)) == 0;
			case 3:	/* ends with */
				return strcmp(value+strlen(value)-strlen(match), match) == 0;
			case 4:	/* soundex */
				return soundexcmp(value, match) == 0;
			}
			return FALSE;
		}
		p++;
	}
	switch(how) {
	case 0:	/* is */
		return strcasecmp(value, match) == 0;
	case 1:	/* contains */
		return e_utf8_strstrcase(value, match) != NULL;
	case 2:	/* starts with */
		return strncasecmp(value, match, strlen(match)) == 0;
	case 3:	/* ends with */
		return strcasecmp(value+strlen(value)-strlen(match), match) == 0;
	case 4:	/* soundex */
		return soundexcmp(value, match) == 0;
	}

	return FALSE;
}

static ESExpResult *
check_header(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms, int how)
{
	gboolean matched = FALSE;
	ESExpResult *r;
	int i;

	if (argc > 1 && argv[0]->type == ESEXP_RES_STRING) {
		const char *header = camel_medium_get_header (CAMEL_MEDIUM (fms->message), argv[0]->value.string);

		if (header) {
			for (i=1;i<argc && !matched;i++) {
				if (argv[i]->type == ESEXP_RES_STRING
				    && check_match(header, argv[i]->value.string, how)) {
					matched = TRUE;
					break;
				}
			}
		}
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = matched;
	
	return r;
}

static ESExpResult *
header_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header(f, argc, argv, fms, 1);
}


static ESExpResult *
header_matches (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header(f, argc, argv, fms, 0);
}

static ESExpResult *
header_starts_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header(f, argc, argv, fms, 2);
}

static ESExpResult *
header_ends_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header(f, argc, argv, fms, 3);
}

static ESExpResult *
header_soundex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header(f, argc, argv, fms, 4);
}

static ESExpResult *
header_exists (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	gboolean matched = FALSE;
	ESExpResult *r;
	int i;

	for (i=0;i<argc && !matched;i++) {
		if (argv[i]->type == ESEXP_RES_STRING)
			matched = camel_medium_get_header (CAMEL_MEDIUM (fms->message), argv[i]->value.string) != NULL;
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = matched;
	
	return r;
}

static ESExpResult *
header_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r = e_sexp_result_new (ESEXP_RES_BOOL);
	regex_t pattern;
	const char *contents;

	if (argc>1
	    && argv[0]->type == ESEXP_RES_STRING
	    && (contents = camel_medium_get_header (CAMEL_MEDIUM (fms->message), argv[0]->value.string))
	    && build_match_regex(&pattern, TRUE, argc-1, argv+1, fms->ex) == 0) {
		r->value.bool = regexec(&pattern, contents, 0, NULL, 0) == 0;
		regfree(&pattern);
	} else
		r->value.bool = FALSE;
	
	return r;
}

static gchar *
get_full_header (CamelMimeMessage *message)
{
	CamelMimePart *mp = CAMEL_MIME_PART (message);
	GString *str = g_string_new ("");
	char   *ret;
	struct _header_raw *h;
	
	for (h = mp->headers; h; h = h->next) {
		if (h->value != NULL) {
			g_string_append(str, h->name);
			if (isspace(h->value[0]))
				g_string_append(str, ":");
			else
				g_string_append(str, ": ");
			g_string_append(str, h->value);
		}
	}
	
	ret = str->str;
	g_string_free (str, FALSE);
	
	return ret;
}

static ESExpResult *
header_full_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r = e_sexp_result_new (ESEXP_RES_BOOL);
	regex_t pattern;
	char *contents;

	if (build_match_regex(&pattern, TRUE, argc, argv, fms->ex) == 0) {
		contents = get_full_header (fms->message);
		r->value.bool = regexec(&pattern, contents, 0, NULL, 0) == 0;
		g_free(contents);
		regfree(&pattern);
	} else
		r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
match_all (struct _ESExp *f, int argc, struct _ESExpTerm **argv, FilterMessageSearch *fms)
{
	/* match-all: when dealing with single messages is a no-op */
	ESExpResult *r;
	
	if (argc > 0)
		return e_sexp_term_eval(f, argv[0]);

	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

/* performs a 'slow' content-based match */
/* taken directly from camel-folder-search.c */
static gboolean
message_body_contains(CamelDataWrapper *object, regex_t *pattern)
{
	CamelDataWrapper *containee;
	int truth = FALSE;
	int parts, i;

	containee = camel_medium_get_content_object(CAMEL_MEDIUM(object));

	if (containee == NULL)
		return FALSE;

	/* TODO: I find it odd that get_part and get_content_object do not
	   add a reference, probably need fixing for multithreading */

	/* using the object types is more accurate than using the mime/types */
	if (CAMEL_IS_MULTIPART(containee)) {
		parts = camel_multipart_get_number(CAMEL_MULTIPART(containee));
		for (i=0;i<parts && truth==FALSE;i++) {
			CamelDataWrapper *part = (CamelDataWrapper *)camel_multipart_get_part(CAMEL_MULTIPART(containee), i);
			if (part) {
				truth = message_body_contains(part, pattern);
			}
		}
	} else if (CAMEL_IS_MIME_MESSAGE(containee)) {
		/* for messages we only look at its contents */
		truth = message_body_contains((CamelDataWrapper *)containee, pattern);
	} else if (header_content_type_is(CAMEL_DATA_WRAPPER(containee)->mime_type, "text", "*")) {
		/* for all other text parts, we look inside, otherwise we dont care */
		CamelStreamMem *mem = (CamelStreamMem *)camel_stream_mem_new();

		camel_data_wrapper_write_to_stream(containee, (CamelStream *)mem);
		camel_stream_write((CamelStream *)mem, "", 1);
		truth = regexec(pattern, mem->buffer->data, 0, NULL, 0) == 0;
		camel_object_unref((CamelObject *)mem);
	}
	return truth;
}

static ESExpResult *
body_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r = e_sexp_result_new (ESEXP_RES_BOOL);
	regex_t pattern;

	if (build_match_regex(&pattern, FALSE, argc, argv, fms->ex) == 0) {
		r->value.bool = message_body_contains((CamelDataWrapper *)fms->message, &pattern);
		regfree(&pattern);
	} else
		r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
body_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r = e_sexp_result_new (ESEXP_RES_BOOL);
	regex_t pattern;

	if (build_match_regex(&pattern, TRUE, argc, argv, fms->ex) == 0) {
		r->value.bool = message_body_contains((CamelDataWrapper *)fms->message, &pattern);
		regfree(&pattern);
	} else
		r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
user_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	gboolean truth = FALSE;
	int i;
	
	/* performs an OR of all words */
	for (i = 0; i < argc && !truth; i++) {
		if (argv[i]->type == ESEXP_RES_STRING
		    && camel_flag_get (&fms->info->user_flags, argv[i]->value.string)) {
			truth = TRUE;
			break;
		}
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = truth;
	
	return r;
}

static ESExpResult *
system_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	gboolean truth = FALSE;
	
	if (argc == 1)
		truth = camel_system_flag_get (fms->info->flags, argv[0]->value.string);
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = truth;
	
	return r;
}

static ESExpResult *
user_tag (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	const char *tag;
	
	tag = camel_tag_get (&fms->info->user_tags, argv[0]->value.string);
	
	r = e_sexp_result_new (ESEXP_RES_STRING);
	r->value.string = g_strdup (tag ? tag : "");
	
	return r;
}

static ESExpResult *
get_sent_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	
	r = e_sexp_result_new(ESEXP_RES_INT);
	r->value.number = camel_mime_message_get_date(fms->message, NULL);
	
	return r;
}

static ESExpResult *
get_received_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	
	r = e_sexp_result_new(ESEXP_RES_INT);
	r->value.number = camel_mime_message_get_date_received(fms->message, NULL);
	
	return r;
}

static ESExpResult *
get_current_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	
	r = e_sexp_result_new (ESEXP_RES_INT);
	r->value.number = time (NULL);
	
	return r;
}

static ESExpResult *
get_score (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	const char *tag;
	
	tag = camel_tag_get (&fms->info->user_tags, "score");
	
	r = e_sexp_result_new (ESEXP_RES_INT);
	if (tag)
		r->value.number = atoi (tag);
	else
		r->value.number = 0;
	
	return r;
}

static ESExpResult *
get_source (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	
	r = e_sexp_result_new (ESEXP_RES_STRING);
	r->value.string = g_strdup (fms->source);
	
	return r;
}

gboolean camel_filter_search_match(CamelMimeMessage *message, CamelMessageInfo *info,
				   const char *source, const char *expression, CamelException *ex)
{
	FilterMessageSearch fms;
	ESExp *sexp;
	ESExpResult *result;
	gboolean retval;
	int i;
	
	fms.message = message;
	fms.info = info;
	fms.source = source;
	fms.ex = ex;
	
	sexp = e_sexp_new ();
	
	for (i = 0; i < sizeof (symbols) / sizeof (symbols[0]); i++) {
		if (symbols[i].type == 1)
			e_sexp_add_ifunction (sexp, 0, symbols[i].name, (ESExpIFunc *)symbols[i].func, &fms);
		else
			e_sexp_add_function (sexp, 0, symbols[i].name, symbols[i].func, &fms);
	}
	
	e_sexp_input_text (sexp, expression, strlen (expression));
	e_sexp_parse (sexp);
	result = e_sexp_eval (sexp);
	
	if (result->type == ESEXP_RES_BOOL)
		retval = result->value.bool;
	else
		retval = FALSE;
	
	e_sexp_unref(sexp);
	e_sexp_result_free (result);
	
	return retval;
}
