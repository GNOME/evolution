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

#include "e-util/e-sexp.h"

#include "camel-mime-message.h"
#include "camel-filter-search.h"
#include "camel-exception.h"
#include "camel-multipart.h"
#include "camel-stream-mem.h"
#include "camel-search-private.h"

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

static ESExpResult *
check_header(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms, camel_search_match_t how)
{
	gboolean matched = FALSE;
	ESExpResult *r;
	int i;

	if (argc > 1 && argv[0]->type == ESEXP_RES_STRING) {
		const char *header = camel_medium_get_header (CAMEL_MEDIUM (fms->message), argv[0]->value.string);

		if (header) {
			for (i=1;i<argc && !matched;i++) {
				if (argv[i]->type == ESEXP_RES_STRING
				    && camel_search_header_match(header, argv[i]->value.string, how)) {
					matched = TRUE;
					break;
				}
			}
		}
	}
	
	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = matched;
	
	return r;
}

static ESExpResult *
header_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header(f, argc, argv, fms, CAMEL_SEARCH_MATCH_CONTAINS);
}


static ESExpResult *
header_matches (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header(f, argc, argv, fms, CAMEL_SEARCH_MATCH_EXACT);
}

static ESExpResult *
header_starts_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header(f, argc, argv, fms, CAMEL_SEARCH_MATCH_STARTS);
}

static ESExpResult *
header_ends_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header(f, argc, argv, fms, CAMEL_SEARCH_MATCH_ENDS);
}

static ESExpResult *
header_soundex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header(f, argc, argv, fms, CAMEL_SEARCH_MATCH_SOUNDEX);
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
	
	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = matched;
	
	return r;
}

static ESExpResult *
header_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	regex_t pattern;
	const char *contents;

	if (argc>1
	    && argv[0]->type == ESEXP_RES_STRING
	    && (contents = camel_medium_get_header (CAMEL_MEDIUM (fms->message), argv[0]->value.string))
	    && camel_search_build_match_regex(&pattern, CAMEL_SEARCH_MATCH_REGEX|CAMEL_SEARCH_MATCH_ICASE, argc-1, argv+1, fms->ex) == 0) {
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
	ESExpResult *r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	regex_t pattern;
	char *contents;

	if (camel_search_build_match_regex(&pattern, CAMEL_SEARCH_MATCH_REGEX|CAMEL_SEARCH_MATCH_ICASE, argc-1, argv+1, fms->ex) == 0) {
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

	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
body_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	regex_t pattern;

	if (camel_search_build_match_regex(&pattern, CAMEL_SEARCH_MATCH_ICASE, argc, argv, fms->ex) == 0) {
		r->value.bool = camel_search_message_body_contains((CamelDataWrapper *)fms->message, &pattern);
		regfree(&pattern);
	} else
		r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
body_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	regex_t pattern;

	if (camel_search_build_match_regex(&pattern, CAMEL_SEARCH_MATCH_ICASE|CAMEL_SEARCH_MATCH_REGEX, argc, argv, fms->ex) == 0) {
		r->value.bool = camel_search_message_body_contains((CamelDataWrapper *)fms->message, &pattern);
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
	
	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
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
	
	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = truth;
	
	return r;
}

static ESExpResult *
user_tag (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	const char *tag;
	
	tag = camel_tag_get (&fms->info->user_tags, argv[0]->value.string);
	
	r = e_sexp_result_new(f, ESEXP_RES_STRING);
	r->value.string = g_strdup (tag ? tag : "");
	
	return r;
}

static ESExpResult *
get_sent_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	
	r = e_sexp_result_new(f, ESEXP_RES_INT);
	r->value.number = camel_mime_message_get_date(fms->message, NULL);
	
	return r;
}

static ESExpResult *
get_received_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	
	r = e_sexp_result_new(f, ESEXP_RES_INT);
	r->value.number = camel_mime_message_get_date_received(fms->message, NULL);
	
	return r;
}

static ESExpResult *
get_current_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	
	r = e_sexp_result_new(f, ESEXP_RES_INT);
	r->value.number = time (NULL);
	
	return r;
}

static ESExpResult *
get_score (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	const char *tag;
	
	tag = camel_tag_get (&fms->info->user_tags, "score");
	
	r = e_sexp_result_new(f, ESEXP_RES_INT);
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
	
	r = e_sexp_result_new(f, ESEXP_RES_STRING);
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
	if (e_sexp_parse (sexp) == -1) {
		if (!camel_exception_is_set(ex))
			camel_exception_setv(ex, 1, _("Error executing filter search: %s: %s"), e_sexp_error(sexp), expression);
		goto error;
	}
	result = e_sexp_eval (sexp);
	if (result == NULL) {
		if (!camel_exception_is_set(ex))
		camel_exception_setv(ex, 1, _("Error executing filter search: %s: %s"), e_sexp_error(sexp), expression);
		goto error;
	}

	if (result->type == ESEXP_RES_BOOL)
		retval = result->value.bool;
	else
		retval = FALSE;
	
	e_sexp_result_free (sexp, result);
	e_sexp_unref(sexp);
	
	return retval;

error:
	e_sexp_unref(sexp);
	return FALSE;
}
