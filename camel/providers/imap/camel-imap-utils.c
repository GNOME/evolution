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

#include "camel-imap-utils.h"
#include "string-utils.h"

#define d(x) x

struct sexp_node {
	struct sexp_node *l_node, *r_node;
	char *function;
	char *data;
};

static char *get_quoted_token (char *string, int *len);
static char *get_token (char *string, int *len);
struct sexp_node *get_sexp_node (const char *exp);
static void print_node (struct sexp_node *node, int depth);
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
	
	for (ep = p; *ep && *ep != ' '; ep++);
	
	*len = ep - string;
	
	return g_strndup (p, *len);
}

struct sexp_node *
get_sexp_node (const char *exp)
{
	struct sexp_node *node = NULL;
	char *p, *ep;
	int len;
	
	switch (*exp) {
	case '(':
		node = g_malloc0 (sizeof (struct sexp_node));
		node->l_node = NULL;
		node->r_node = NULL;
		node->data = NULL;
		
		p = (char *) exp + 1;
		
		node->function = get_token (p, &len);
		
		p += len;
		for (ep = p; *ep && *ep != '(' && *ep != ')'; ep++);
		node->data = g_strndup (p, (gint)(ep - p));
		g_strstrip (node->data);
		
		p = ep;
		
		if (*p == '(')
			node->r_node = get_sexp_node (p);
		else
			node->l_node = get_sexp_node (p);
		
		return node;
		break;
	case '\0':
		return NULL;
		break;
	case ')':
		for (p = (char *) exp + 1; *p && *p == ' '; p++);
		return get_sexp_node (p);
		break;
	default:
		node = g_malloc0 (sizeof (struct sexp_node));
		node->l_node = NULL;
		node->r_node = NULL;
		node->data = NULL;
		
		p = (char *) exp;
		
		node->function = get_token (p, &len);

		p += len;
		for (ep = p; *ep && *ep != '(' && *ep != ')'; ep++);
		node->data = g_strndup (p, (gint)(ep - p));
		g_strstrip (node->data);
		
		p = ep;
		
		if (*p == '(')
			node->r_node = get_sexp_node (p);
		else
			node->l_node = get_sexp_node (p);
		
		return node;
	}
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

static char *esexp_keys[] = { "or", "body-contains", "header-contains", "match-all", NULL };
static char *imap_keys[]  = { "OR", "BODY", "HEADER", NULL };

static char *
str_sexp_node (struct sexp_node *node)
{
	char *node_str, *func, *str, *l_str, *r_str;
	int i;
	
	for (i = 0; esexp_keys[i]; i++)
		if (!strncmp (esexp_keys[i], node->function, strlen (node->function)))
			break;
	
	if (esexp_keys[i])
		func = imap_keys[i];
	else
		func = node->function;
	
	if (func) {
		if (*node->data)
			str = g_strdup_printf ("%s %s", func, node->data);
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
	
	sexp = str_sexp_node (root);
	sexp[strlen (sexp) - 1] = '\0';
	strcpy (sexp, sexp + 1);
	
	free_sexp_node (root);
	
	return sexp;
}
