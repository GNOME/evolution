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
#include <time.h>

#include <gtk/gtk.h>
#include "camel-imap-utils.h"
#include "string-utils.h"
#include <e-sexp.h>

#define d(x) x

static char *get_quoted_token (char *string, int *len);
static char *get_token (char *string, int *len);


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
		if (!strncmp (word, "NIL", 3)) {
			*sep = NULL;
		} else {
			for (ep = word; *ep && *ep != ' '; ep++);
			*sep = g_strndup (word, (gint)(ep - word));
			string_unquote (*sep);
		}
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

static char *
get_quoted_token (char *string, int *len)
{
	char *ep;
	
	for (ep = string + 1; *ep; ep++)
		if (*ep == '"' && *(ep - 1) != '\\')
			break;
	if (*ep)
		ep++;
	
	*len = ep - string;
	
	return g_strndup (string, *len);
}

static char *
get_token (char *string, int *len)
{
	char *p, *ep;
	
	for (p = string; *p && *p == ' '; p++);
	
	if (*p == '"') {
		char *token;
		int i;

		token = get_quoted_token (p, &i);

		*len = i + (p - string);
		
		return token;
	}
	
	for (ep = p; *ep && *ep != ' ' && *ep != ')'; ep++);
	
	*len = ep - string;
	
	return g_strndup (p, *len);
}

static ESExpResult *
func_and (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	
	fprintf (stderr, "in AND func (argc = %d)\n", argc);
	if (argc > 0) {
		char **strings;
		int i;
		
		strings = g_new (char*, argc+1);
		strings[argc] = NULL;
		
		for (i = 0; i < argc; i++) {
			GList *list_head = *list;
			
			fprintf (stderr, "\tAND func: %s\n", (*list) ? (char *) (*list)->data : "(null)");
			strings[argc - (i+1)] = (*list) ? (*list)->data : g_strdup ("");
			*list = g_list_remove_link (*list, *list);
			g_list_free_1 (list_head);
		}
		
		*list = g_list_prepend (*list, g_strjoinv (" ", strings));
		fprintf (stderr, "%s\n", (char *) (*list)->data);
		
		for (i = 0 ; i < argc; i ++)
			g_free (strings[i]);
		
		g_free (strings);
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_or (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	
	fprintf (stderr, "in OR func (argc = %d)\n", argc);
	if (argc == 2 && (*list)->data && (*list)->next && (*list)->next->data) {
		char **strings;
		int i;
		
		strings = g_new (char*, argc+2);
		strings[0] = g_strdup ("OR");
		strings[argc+2 - 1] = NULL;
		
		for (i = 0; i < 2; i++) {
			GList *list_head = *list;
			
			fprintf (stderr, "\tOR func: %s\n", (*list) ? (char *) (*list)->data : "(null)");
			strings[argc - i] = (*list) ? (*list)->data : g_strdup ("");
			*list = g_list_remove_link (*list, *list);
			g_list_free_1 (list_head);
		}
		
		*list = g_list_prepend (*list, g_strjoinv (" ", strings));
		fprintf (stderr, "%s\n", (char *) (*list)->data);
		
		for (i = 0 ; i < argc + 2; i ++)
			g_free (strings[i]);
		
		g_free (strings);
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_not (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	
	fprintf (stderr, "in NOT func\n");
	/* just replace the head of the list with the NOT of it. */
	if (argc > 0) {
		char *term = (*list)->data;
		
		(*list)->data = g_strdup_printf ("NOT %s", term);
		fprintf (stderr, "%s\n", (char *) (*list)->data);
		g_free (term);
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static char *tz_months [] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static char *
format_date (time_t time, int offset)
{
	struct tm tm;
	
	time += ((offset / 100) * (60*60)) + (offset % 100)*60;
	
	d(printf("converting date %s", ctime (&time)));
	
	memcpy (&tm, gmtime (&time), sizeof (tm));

	return g_strdup_printf ("%d-%s-%04d",
				tm.tm_mday, tz_months[tm.tm_mon],
				tm.tm_year + 1900);
}

static ESExpResult *
func_lt (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	char *type = (*list)->data;
	time_t date = (time_t) (argv[1])->value.number;
	ESExpResult *r;
	
	fprintf (stderr, "in less-than func: (%d) (%s) (%d)\n", argc, type, (int) date);
	if (argc > 0) {
		char *string, *date_str;
		
		date_str = format_date (date, 0);
		
		if (!strcmp ("SENT", type)) {
			string = g_strdup_printf ("SENTBEFORE \"%s\"", date_str);
		} else {
			string = g_strdup_printf ("BEFORE \"%s\"", date_str);
		}
		
		(*list)->data = string;
		g_free (type);
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_gt (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	char *type = (*list)->data;
	time_t date = (time_t) (argv[1])->value.number;
	ESExpResult *r;
	
	fprintf (stderr, "in greater-than func: (%d) (%s) (%d)\n", argc, type, (int) date);
	if (argc > 0) {
		char *string, *date_str;
		
		date_str = format_date (date, 0);
		
		if (!strcmp ("SENT", type)) {
			string = g_strdup_printf ("SENTSINCE \"%s\"", date_str);
		} else {
			string = g_strdup_printf ("SINCE \"%s\"", date_str);
		}
		
		(*list)->data = string;
		g_free (type);
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_eq (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	char *type = (*list)->data;
	time_t date = (time_t) (argv[1])->value.number;
	ESExpResult *r;
	
	fprintf (stderr, "in equal-to func: (%d) (%s) (%d)\n", argc, type, (int) date);
	if (argc > 0) {
		char *string, *date_str;
		
		date_str = format_date (date, 0);
		
		if (!strcmp ("SENT", type)) {
			string = g_strdup_printf ("SENTON \"%s\"", date_str);
		} else {
			string = g_strdup_printf ("ON \"%s\"", date_str);
		}
		
		(*list)->data = string;
		g_free (type);
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_match_all (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	/* match-all doesn't have a IMAP equiv */
	ESExpResult *r;
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_body_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	char *value = (*argv)->value.string;
	ESExpResult *r;
	
	if (argc > 0) {
		char *string;
		
		string = g_strdup_printf ("BODY \"%s\"", value);
		
		*list = g_list_prepend (*list, string);
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_header_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	char *header = (argv[0])->value.string;
	char *match = (argv[1])->value.string;
	ESExpResult *r;
	
	if (argc == 2) {
		char *string;
		string = g_strdup_printf ("HEADER %s \"%s\"", header, match);
		
		*list = g_list_prepend (*list, string);
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_user_tag (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	/* FIXME: what do I do here? */
	ESExpResult *r;
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_user_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	/* FIXME: what do I do here? */
	ESExpResult *r;
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_get_sent_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	
	*list = g_list_prepend (*list, g_strdup ("SENT"));
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_get_received_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	
	*list = g_list_prepend (*list, g_strdup ("RECEIVED"));
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_get_current_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	/* FIXME: what do I do here? */
	ESExpResult *r;
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

/* builtin functions */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "and",               (ESExpFunc *) func_and,               0 },
	{ "or",                (ESExpFunc *) func_or,                0 },
	{ "not",               (ESExpFunc *) func_not,               0 },
	{ "<",                 (ESExpFunc *) func_lt,                0 },
	{ ">",                 (ESExpFunc *) func_gt,                0 },
	{ "=",                 (ESExpFunc *) func_eq,                0 },
	{ "match-all",         (ESExpFunc *) func_match_all,         0 },
	{ "body-contains",     (ESExpFunc *) func_body_contains,     0 },
	{ "header-contains",   (ESExpFunc *) func_header_contains,   0 },
	{ "user-tag",          (ESExpFunc *) func_user_tag,          1 },
	{ "user-flag",         (ESExpFunc *) func_user_flag,         1 },
	{ "get-sent-date",     (ESExpFunc *) func_get_sent_date,     1 },
	{ "get-received-date", (ESExpFunc *) func_get_received_date, 1 },
	{ "get-current-date",  (ESExpFunc *) func_get_current_date,  1 }
};

char *
imap_translate_sexp (const char *expression)
{
	ESExp *sexp;
	ESExpResult *r;
	gchar *retval;
	GList *list = NULL;
	int i;
	
	sexp = e_sexp_new ();
	
	for (i = 0; i < sizeof (symbols) / sizeof (symbols[0]); i++) {
		if (symbols[i].type == 1) {
			e_sexp_add_ifunction (sexp, 0, symbols[i].name,
					      (ESExpIFunc *)symbols[i].func, &list);
		} else {
			e_sexp_add_function (sexp, 0, symbols[i].name,
					     symbols[i].func, &list);
		}
	}
	
	e_sexp_input_text (sexp, expression, strlen (expression));
	
	e_sexp_parse (sexp);
	
	r = e_sexp_eval (sexp);
	
	gtk_object_unref (GTK_OBJECT (sexp));
	e_sexp_result_free (r);
	
	if (list->next) {
		g_warning ("conversion to IMAP SEARCH string failed");
		retval = NULL;
		g_list_foreach (list, (GFunc)g_free, NULL);
	} else {
		retval = list->data;
	}
	
	g_list_free (list);
	
	return retval;
}
