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

#include <gtk/gtk.h>
#include "camel-imap-utils.h"
#include "string-utils.h"
#include <e-sexp.h>

#define d(x) x

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
		for (ep = word; *ep && *ep != ' '; ep++);
		*sep = g_strndup (word, (gint)(ep - word));
		string_unquote (*sep);
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

struct prop_info {
	char *query_prop;
	char *imap_attr;
} prop_info_table[] = {
	/* query prop,            imap attr */
	{ "body-contains",        "BODY"     },
	{ "header-contains",      "HEADER"   }
};

static int num_prop_infos = sizeof (prop_info_table) / sizeof (prop_info_table[0]);

static gchar *
query_prop_to_imap (gchar *query_prop)
{
	int i;

	for (i = 0; i < num_prop_infos; i ++)
		if (!strcmp (query_prop, prop_info_table[i].query_prop))
			return prop_info_table[i].imap_attr;

	return NULL;
}

static ESExpResult *
func_and (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	char **strings;
	
	if (argc > 0) {
		int i;
		
		strings = g_malloc0 (argc + 3);
		strings[0] = g_strdup ("(AND");
		strings[argc+3 - 2] = g_strdup (")");
		strings[argc+3 - 1] = NULL;
		
		for (i = 0; i < argc; i++) {
			GList *list_head = *list;
			strings[argc - i] = (*list)->data;
			*list = g_list_remove_link (*list, *list);
			g_list_free_1 (list_head);
		}
		
		*list = g_list_prepend (*list, g_strjoinv (" ", strings));
		
		for (i = 0 ; i < argc + 2; i++)
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
	char **strings;
	
	if (argc > 0) {
		int i;
		
		strings = g_malloc0 (argc+3);
		strings[0] = g_strdup ("(OR");
		strings[argc+3 - 2] = g_strdup (")");
		strings[argc+3 - 1] = NULL;
		for (i = 0; i < argc; i++) {
			GList *list_head = *list;
			strings[argc - i] = (*list)->data;
			*list = g_list_remove_link (*list, *list);
			g_list_free_1 (list_head);
		}
		
		*list = g_list_prepend (*list, g_strjoinv (" ", strings));
		
		for (i = 0 ; i < argc + 2; i++)
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
	
	/* just replace the head of the list with the NOT of it. */
	if (argc > 0) {
		char *term = (*list)->data;
		(*list)->data = g_strdup_printf ("(NOT %s)", term);
		g_free (term);
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	
	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname = argv[0]->value.string;
		char *str = argv[1]->value.string;
		char *imap_attr = query_prop_to_imap (propname);
		gboolean one_star = FALSE;
		
		if (strlen (str) == 0)
			one_star = TRUE;
		
		if (imap_attr)
			*list = g_list_prepend (*list,
						g_strdup_printf ("(%s=*%s%s)",
								 imap_attr,
								 str,
								 one_star ? "" : "*"));
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_is (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	
	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname = argv[0]->value.string;
		char *str = argv[1]->value.string;
		char *imap_attr = query_prop_to_imap (propname);
		
		if (imap_attr)
			*list = g_list_prepend (*list,
						g_strdup_printf ("(%s=%s)",
								 imap_attr, str));
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
func_beginswith (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	
	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname = argv[0]->value.string;
		char *str = argv[1]->value.string;
		char *imap_attr = query_prop_to_imap (propname);
		gboolean one_star = FALSE;
		
		if (strlen(str) == 0)
			one_star = TRUE;
		
		if (imap_attr)
			*list = g_list_prepend (*list,
						g_strdup_printf ("(%s=%s*)",
								 imap_attr,
								 str));
	}
	
	r = e_sexp_result_new (ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
func_endswith (struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	
	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname = argv[0]->value.string;
		char *str = argv[1]->value.string;
		char *imap_attr = query_prop_to_imap (propname);
		gboolean one_star = FALSE;
		
		if (strlen (str) == 0)
			one_star = TRUE;
		
		if (imap_attr)
			*list = g_list_prepend (*list,
						g_strdup_printf ("(%s=*%s)",
								 imap_attr,
								 str));
	}
	
	r = e_sexp_result_new(ESEXP_RES_BOOL);
	r->value.bool = FALSE;
	
	return r;
}

/* 'builtin' functions */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "and", func_and, 0 },
	{ "or", func_or, 0 },
	{ "not", func_not, 0 },
	{ "contains", func_contains, 0 },
	{ "is", func_is, 0 },
	{ "beginswith", func_beginswith, 0 },
	{ "endswith", func_endswith, 0 },
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
		g_warning ("conversion to imap expression string failed");
		retval = NULL;
		g_list_foreach (list, (GFunc)g_free, NULL);
	} else {
		retval = list->data;
	}
	
	g_list_free (list);
	
	return retval;
}
