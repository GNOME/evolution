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

static char *esexp_keys[] = { "and", "or", "body-contains", "header-contains", "match-all", NULL };
static char *imap_keys[]  = { "", "OR", "BODY", "HEADER", NULL };

struct sexp_node {
	struct sexp_node *l_node, *r_node;
	char *function;
	char *data;
};

static char *get_quoted_token (char *string, int *len);
static char *get_token (char *string, int *len);
struct sexp_node *get_sexp_node (const char *exp);
static void print_node (struct sexp_node *node, int depth);
static const char *get_func (struct sexp_node *node);
static char *get_data (struct sexp_node *node);
static char *str_sexp_node (struct sexp_node *node);
static void free_sexp_node (struct sexp_node *node);


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

struct sexp_node *
get_sexp_node (const char *exp)
{
	struct sexp_node *node = NULL;
	char *left_exp, *right_exp, *this_exp;
	char *p, *ep;
	int len, pbal;
	
	if (exp && *exp) {	
		node = g_malloc0 (sizeof (struct sexp_node));
		node->l_node = NULL;
		node->r_node = NULL;
		node->function = NULL;
		node->data = NULL;
		
		p = (char *) exp + 1;
		for (ep = p, pbal = 1; *ep && pbal; ep++) {
			if (*ep == '(')
				pbal++;
			if (*ep == ')')
				pbal--;
		}
		
		this_exp = g_strndup (p, (gint)(ep - p));
		
		for (left_exp = ep; *left_exp && *left_exp != '('; left_exp++);
		left_exp = g_strdup (left_exp);
		
		for (right_exp = this_exp; *right_exp && *right_exp != '('; right_exp++);
		pbal = 1;
		for (ep = right_exp; *ep && pbal; ep++) {
			if (*ep == '(')
				pbal++;
			if (*ep == ')')
				pbal--;
		}
		right_exp = g_strndup (right_exp, (gint)(ep - right_exp));
		
		/* fill in the node */
		node->function = get_token (this_exp, &len);
		p = this_exp + len;
		for (ep = p; *ep && *ep != '(' && *ep != ')'; ep++);
		node->data = g_strndup (p, (gint)(ep - p));
		
		g_strstrip (node->data);

		node->l_node = get_sexp_node (left_exp);
		node->r_node = get_sexp_node (right_exp);

		g_free (this_exp);
		g_free (left_exp);
		g_free (right_exp);
	}
	
	return node;
}

static void
print_node (struct sexp_node *node, int depth)
{
	int i;
	
	for (i = 0; i < depth; i++)
		d(fprintf (stderr, "   "));
	
	d(fprintf (stderr, "%s\n", node->function));
	
	if (*node->data) {
		for (i = 0; i < depth + 1; i++)
			d(fprintf (stderr, "   "));
		
		d(fprintf (stderr, "%s\n", node->data));
	}
	
	if (node->r_node)
		print_node (node->r_node, depth + 1);
	
	if (node->l_node)
		print_node (node->l_node, depth);
}

static const char *
get_func (struct sexp_node *node)
{
	int i;
	
	for (i = 0; esexp_keys[i]; i++)
		if (!strncmp (esexp_keys[i], node->function, strlen (node->function)))
			break;
	
	if (esexp_keys[i])
		return imap_keys[i];
	else
		return node->function;
}

static char *
get_data (struct sexp_node *node)
{
	GPtrArray *args;
	const char *func;
	char *data, *token, *p;
	int i, len;
	
	func = get_func (node);
	
	args = g_ptr_array_new ();
	
	p = node->data;
	while (p && *p) {
		token = get_token (p, &len);
		g_ptr_array_add (args, token);
		p += len;
	}
	
	if (func && !strcmp ("HEADER", func) && args->len > 0)
		string_unquote (args->pdata[0]);
	
	if (args->len > 0) {
		data = g_strjoinv (" ", (char **) args->pdata);
	} else {
		data = g_strdup ("");
	}
	
	for (i = 0; i < args->len; i++)
		g_free (args->pdata[i]);
	
	g_ptr_array_free (args, TRUE);
	
	return data;
}

static char *
str_sexp_node (struct sexp_node *node)
{
	char *node_str, *data, *str, *l_str, *r_str;
	const char *func;
	
	func = get_func (node);
	data = get_data (node);
	
	if (func) {
		if (*data)
			str = g_strdup_printf ("%s %s", func, data);
		else
			str = g_strdup (func);
	} else {
		str = NULL;
	}
	
	g_free (data);
	
	r_str = NULL;
	if (node->r_node)
		r_str = str_sexp_node (node->r_node);
	
	l_str = NULL;
	if (node->l_node)
		l_str = str_sexp_node (node->l_node);
	
	if (str) {
		if (r_str) {
			if (l_str)
				node_str = g_strdup_printf ("%s %s %s", str, r_str, l_str);
			else
				node_str = g_strdup_printf ("%s %s", str, r_str);
		} else {
			if (l_str)
				node_str = g_strdup_printf ("%s %s", str, l_str);
			else
				node_str = g_strdup_printf ("%s", str);
		}
	} else {
		if (r_str) {
			if (l_str)
				node_str = g_strdup_printf ("%s %s", r_str, l_str);
			else
				node_str = g_strdup_printf ("%s", r_str);
		} else {
			if (l_str)
				node_str = g_strdup_printf ("%s", l_str);
			else
				node_str = g_strdup ("");
		}
	}
	
	g_free (str);
	g_free (l_str);
	g_free (r_str);
	
	return node_str;
}

static void
free_sexp_node (struct sexp_node *node)
{
	if (node->r_node)
		free_sexp_node (node->r_node);
	
	if (node->l_node)
		free_sexp_node (node->l_node);
	
	g_free (node->function);
	g_free (node->data);
	g_free (node);
}

char *
imap_translate_sexp (const char *expression)
{
	struct sexp_node *root;
	char *sexp;
	
	root = get_sexp_node (expression);
	
	d(print_node (root, 0));
	d(fprintf (stderr, "\n"));
	
	sexp = str_sexp_node (root);
	
	free_sexp_node (root);
	
	return sexp;
}










#ifdef _ALL_HELL_BROKE_LOOSE_
static char *
stresexptree (ESExpTerm *node)
{
	char *node_str, *func, *str, *l_str, *r_str;
	int i;
	
	for (i = 0; esexp_keys[i]; i++)
		if (!strncmp (esexp_keys[i], node->func->sym->name, strlen (node->func->sym->name)))
			break;
	
	if (esexp_keys[i])
		func = imap_keys[i];
	else
		func = node->func->sym->name;
	
	if (func) {
		if (*node->var->name)
			str = g_strdup_printf ("%s %s", func, node->var->name);
		else
			str = g_strdup (func);
	} else {
		str = NULL;
	}
	
	r_str = NULL;
	if (node->r_node)
		r_str = str_sexp_node (node->r_node);
	
	l_str = NULL;
	if (node->l_node)
		l_str = str_sexp_node (node->l_node);
	
	if (str) {
		if (r_str) {
			if (l_str)
				node_str = g_strdup_printf ("(%s (%s)) %s", str, r_str, l_str);
			else
				node_str = g_strdup_printf ("(%s %s)", str, r_str);
		} else {
			if (l_str)
				node_str = g_strdup_printf ("(%s) %s", str, l_str);
			else
				node_str = g_strdup_printf ("(%s)", str);
		}
	} else {
		if (r_str) {
			if (l_str)
				node_str = g_strdup_printf ("(%s) %s", r_str, l_str);
			else
				node_str = g_strdup_printf ("%s", r_str);
		} else {
			if (l_str)
				node_str = g_strdup_printf ("%s", l_str);
			else
				node_str = g_strdup ("");
		}
	}
	
	g_free (str);
	g_free (l_str);
	g_free (r_str);
	
	return node_str;
}

char *
imap_translate_sexp (const char *expression)
{
	ESExp *esexp;
	char *sexp;
	
	esexp = e_sexp_new ();
	
	e_sexp_input_text (esexp, expression, strlen (expression));
	e_sexp_parse (esexp);
	
	sexp = stresexptree (esexp->tree);
	
	gtk_object_unref (GTK_OBJECT (esexp));
	
	return sexp;
}
#endif /* _ALL_HELL_BROKE_LOOSE_ */
