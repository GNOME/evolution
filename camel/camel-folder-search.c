/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 */

/* This is a helper class for folders to implement the search function.
   It implements enough to do basic searches on folders that can provide
   an in-memory summary and a body index. */

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "camel-folder-search.h"
#include "string-utils.h"

#define d(x)
#define r(x)

struct _CamelFolderSearchPrivate {
};

#define _PRIVATE(o) (((CamelFolderSearch *)(o))->priv)

static ESExpResult *search_header_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_match_all(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *search);
static ESExpResult *search_body_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_user_flag(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
static ESExpResult *search_user_tag(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);

static ESExpResult *search_dummy(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);

static void camel_folder_search_class_init (CamelFolderSearchClass *klass);
static void camel_folder_search_init       (CamelFolderSearch *obj);
static void camel_folder_search_finalise   (GtkObject *obj);

static CamelObjectClass *camel_folder_search_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
camel_folder_search_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelFolderSearch",
			sizeof (CamelFolderSearch),
			sizeof (CamelFolderSearchClass),
			(GtkClassInitFunc) camel_folder_search_class_init,
			(GtkObjectInitFunc) camel_folder_search_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (camel_object_get_type (), &type_info);
	}
	
	return type;
}

static void
camel_folder_search_class_init (CamelFolderSearchClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	
	camel_folder_search_parent = gtk_type_class (camel_object_get_type ());

	object_class->finalize = camel_folder_search_finalise;

	klass->match_all = search_match_all;
	klass->body_contains = search_body_contains;
	klass->header_contains = search_header_contains;
	klass->user_flag = search_user_flag;
	klass->user_flag = search_user_tag;

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
camel_folder_search_init (CamelFolderSearch *obj)
{
	struct _CamelFolderSearchPrivate *p;

	p = _PRIVATE(obj) = g_malloc0(sizeof(*p));

	obj->sexp = e_sexp_new();
}

static void
camel_folder_search_finalise (GtkObject *obj)
{
	CamelFolderSearch *search = (CamelFolderSearch *)obj;
	if (search->sexp)
		gtk_object_unref((GtkObject *)search->sexp);

	g_free(search->last_search);

	((GtkObjectClass *)(camel_folder_search_parent))->finalize((GtkObject *)obj);
}

struct {
	char *name;
	int offset;
	int flags;		/* 0x02 = immediate, 0x01 = always enter */
} builtins[] = {
	/* these have default implementations in e-sexp */
	{ "and", GTK_STRUCT_OFFSET(CamelFolderSearchClass, and), 2 },
	{ "or", GTK_STRUCT_OFFSET(CamelFolderSearchClass, or), 2 },
	{ "not", GTK_STRUCT_OFFSET(CamelFolderSearchClass, not), 2 },
	{ "<", GTK_STRUCT_OFFSET(CamelFolderSearchClass, lt), 2 },
	{ ">", GTK_STRUCT_OFFSET(CamelFolderSearchClass, gt), 2 },
	{ "=", GTK_STRUCT_OFFSET(CamelFolderSearchClass, eq), 2 },

	/* these we have to use our own default if there is none */
	/* they should all be defined in the language? so it poarses, or should they not?? */
	{ "match-all", GTK_STRUCT_OFFSET(CamelFolderSearchClass, match_all), 3 },
	{ "body-contains", GTK_STRUCT_OFFSET(CamelFolderSearchClass, body_contains), 1 },
	{ "header-contains", GTK_STRUCT_OFFSET(CamelFolderSearchClass, header_contains), 1 },
	{ "user-flag", GTK_STRUCT_OFFSET(CamelFolderSearchClass, user_flag), 1 },
	{ "user-tag", GTK_STRUCT_OFFSET(CamelFolderSearchClass, user_flag), 1 },
};

void
camel_folder_search_construct (CamelFolderSearch *search)
{
	int i;
	CamelFolderSearchClass *klass = (CamelFolderSearchClass *)GTK_OBJECT(search)->klass;

	for (i=0;i<sizeof(builtins)/sizeof(builtins[0]);i++) {
		void *func;
		/* c is sure messy sometimes */
		func = *((void **)(((char *)klass)+builtins[i].offset));
		if (func == NULL && builtins[i].flags&1) {
			g_warning("Search class doesn't implement '%s' method: %s", builtins[i].name, gtk_type_name(GTK_OBJECT(search)->klass->type));
			func = (void *)search_dummy;
		}
		if (func != NULL) {
			if (builtins[i].flags&2) {
				e_sexp_add_ifunction(search->sexp, 0, builtins[i].name, (ESExpIFunc *)func, search);
			} else {
				e_sexp_add_function(search->sexp, 0, builtins[i].name, (ESExpFunc *)func, search);
			}
		}
	}
}

/**
 * camel_folder_search_new:
 *
 * Create a new CamelFolderSearch object.
 * 
 * A CamelFolderSearch is a subclassable, extensible s-exp
 * evaluator which enforces a particular set of s-expressions.
 * Particular methods may be overriden by an implementation to
 * implement a search for any sort of backend.
 *
 * Return value: A new CamelFolderSearch widget.
 **/
CamelFolderSearch *
camel_folder_search_new (void)
{
	CamelFolderSearch *new = CAMEL_FOLDER_SEARCH ( gtk_type_new (camel_folder_search_get_type ()));

	camel_folder_search_construct(new);
	return new;
}

/**
 * camel_folder_search_set_folder:
 * @search:
 * @folder: A folder.
 * 
 * Set the folder attribute of the search.  This is currently unused, but
 * could be used to perform a slow-search when indexes and so forth are not
 * available.  Or for use by subclasses.
 **/
void
camel_folder_search_set_folder(CamelFolderSearch *search, CamelFolder *folder)
{
	search->folder = folder;
}

/**
 * camel_folder_search_set_summary:
 * @search: 
 * @summary: An array of CamelMessageInfo pointers.
 * 
 * Set the array of summary objects representing the span of the search.
 *
 * If this is not set, then a subclass must provide the functions
 * for searching headers and for the match-all operator.
 **/
void
camel_folder_search_set_summary(CamelFolderSearch *search, GPtrArray *summary)
{
	search->summary = summary;
}

/**
 * camel_folder_search_set_body_index:
 * @search: 
 * @index: 
 * 
 * Set the index (ibex) representing the contents of all messages
 * in this folder.  If this is not set, then the folder implementation
 * should sub-class the CamelFolderSearch and provide its own
 * body-contains function.
 **/
void
camel_folder_search_set_body_index(CamelFolderSearch *search, ibex *index)
{
	search->body_index = index;
}

/**
 * camel_folder_search_execute_expression:
 * @search: 
 * @expr: 
 * @ex: 
 * 
 * Execute the search expression @expr, returning an array of
 * all matches as a GPtrArray of uid's of matching messages.
 *
 * Note that any settings such as set_body_index(), set_folder(),
 * and so on are reset to #NULL once the search has completed.
 *
 * TODO: The interface should probably return summary items instead
 * (since they are much more useful to any client).
 * 
 * Return value: A GPtrArray of strings of all matching messages.
 * This must only be freed by camel_folder_search_free_result.
 **/
GPtrArray *
camel_folder_search_execute_expression(CamelFolderSearch *search, const char *expr, CamelException *ex)
{
	ESExpResult *r;
	GPtrArray *matches = g_ptr_array_new ();
	int i;
	GHashTable *results;

	/* only re-parse if the search has changed */
	if (search->last_search == NULL
	    || strcmp(search->last_search, expr)) {
		e_sexp_input_text(search->sexp, expr, strlen(expr));
		e_sexp_parse(search->sexp);
		g_free(search->last_search);
		search->last_search = g_strdup(expr);
	}
	r = e_sexp_eval(search->sexp);

	/* now create a folder summary to return?? */
	if (r
	    && r->type == ESEXP_RES_ARRAY_PTR) {
		d(printf("got result ...\n"));
		if (search->summary) {
			/* reorder result in summary order */
			results = g_hash_table_new(g_str_hash, g_str_equal);
			for (i=0;i<r->value.ptrarray->len;i++) {
				d(printf("adding match: %s\n", (char *)g_ptr_array_index(r->value.ptrarray, i)));
				g_hash_table_insert(results, g_ptr_array_index(r->value.ptrarray, i), (void *)1);
			}
			for (i=0;i<search->summary->len;i++) {
				CamelMessageInfo *info = g_ptr_array_index(search->summary, i);
				if (g_hash_table_lookup(results, info->uid)) {
					g_ptr_array_add(matches, g_strdup(info->uid));
				}
			}
			g_hash_table_destroy(results);
		} else {
			for (i=0;i<r->value.ptrarray->len;i++) {
				d(printf("adding match: %s\n", (char *)g_ptr_array_index(r->value.ptrarray, i)));
				g_ptr_array_add(matches, g_strdup(g_ptr_array_index(r->value.ptrarray, i)));
			}
		}
		e_sexp_result_free(r);
	} else {
		printf("no result!\n");
	}

	search->folder = NULL;
	search->summary = NULL;
	search->current = NULL;
	search->body_index = NULL;

	return matches;
}

void camel_folder_search_free_result(CamelFolderSearch *search, GPtrArray *result)
{
	int i;

	for (i=0;i<result->len;i++)
		g_free(g_ptr_array_index(result, i));
	g_ptr_array_free(result, TRUE);
}

/* dummy function, returns false always, or an empty match array */
static ESExpResult *
search_dummy(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;

	if (search->current == NULL) {
		r = e_sexp_result_new(ESEXP_RES_BOOL);
		r->value.bool = FALSE;
	} else {
		r = e_sexp_result_new(ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new();
	}

	return r;
}

static ESExpResult *
search_match_all(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *search)
{
	int i;
	ESExpResult *r, *r1;

	if (argc>1) {
		g_warning("match-all only takes a single argument, other arguments ignored");
	}
	r = e_sexp_result_new(ESEXP_RES_ARRAY_PTR);
	r->value.ptrarray = g_ptr_array_new();

	if (search->summary == NULL) {
		/* TODO: make it work - e.g. use the folder and so forth for a slower search */
		g_warning("No summary supplied, match-all doesn't work with no summary");
		return r;
	}

	/* TODO: Could make this a bit faster in the uncommon case (of match-everything) */
	for (i=0;i<search->summary->len;i++) {
		search->current = g_ptr_array_index(search->summary, i);
		if (argc>0) {
			r1 = e_sexp_term_eval(f, argv[0]);
			if (r1->type == ESEXP_RES_BOOL) {
				if (r1->value.bool)
					g_ptr_array_add(r->value.ptrarray, search->current->uid);
			} else {
				g_warning("invalid syntax, matches require a single bool result");
			}
			e_sexp_result_free(r1);
		} else {
			g_ptr_array_add(r->value.ptrarray, search->current->uid);
		}
	}
	search->current = NULL;

	return r;
}

static ESExpResult *
search_header_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;
	int truth = FALSE;

	r(printf("executing header-contains\n"));

	/* are we inside a match-all? */
	if (search->current && argc>1
	    && argv[0]->type == ESEXP_RES_STRING) {
		char *headername, *header = NULL;
		char strbuf[32];
		int i;

		/* only a subset of headers are supported .. */
		headername = argv[0]->value.string;
		if (!strcasecmp(headername, "subject")) {
			header = search->current->subject;
		} else if (!strcasecmp(headername, "date")) {
			/* FIXME: not a very useful form of the date */
			sprintf(strbuf, "%d", (int)search->current->date_sent);
			header = strbuf;
		} else if (!strcasecmp(headername, "from")) {
			header = search->current->from;
		} else if (!strcasecmp(headername, "to")) {
			header = search->current->to;
		} else if (!strcasecmp(headername, "cc")) {
			header = search->current->cc;
		} else {
			g_warning("Performing query on unknown header: %s", headername);
		}

		if (header) {
			/* performs an OR of all words */
			for (i=1;i<argc && !truth;i++) {
				if (argv[i]->type == ESEXP_RES_STRING
				    && e_strstrcase (header, argv[i]->value.string)) {
					r(printf("%s got a match with %s of %s\n", search->current->uid, header, argv[i]->value.string));
					truth = TRUE;
					break;
				}
			}
		}
	}
	/* TODO: else, find all matches */

	r = e_sexp_result_new(ESEXP_RES_BOOL);
	r->value.bool = truth;

	return r;
}

/* this is just to OR results together */
struct _glib_sux_donkeys {
	int count;
	GPtrArray *uids;
};

/* or, store all unique values */
static void
g_lib_sux_htor(char *key, int value, struct _glib_sux_donkeys *fuckup)
{
	g_ptr_array_add(fuckup->uids, key);
}

static ESExpResult *
search_body_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;
	int i, j;

	if (search->current) {
		int truth = FALSE;

		r = e_sexp_result_new(ESEXP_RES_BOOL);
		if (search->body_index) {
			for (i=0;i<argc && !truth;i++) {
				if (argv[i]->type == ESEXP_RES_STRING) {
					truth = ibex_find_name(search->body_index, search->current->uid, argv[i]->value.string);
				} else {
					g_warning("Invalid type passed to body-contains match function");
				}
			}
		} else {
			g_warning("Cannot perform indexed body query with no index");
		}
		r->value.bool = truth;
	} else {
		r = e_sexp_result_new(ESEXP_RES_ARRAY_PTR);

		if (search->body_index) {
			if (argc==1) {
				/* common case */
				r->value.ptrarray = ibex_find(search->body_index, argv[0]->value.string);
			} else {
				GHashTable *ht = g_hash_table_new(g_str_hash, g_str_equal);
				GPtrArray *pa;
				struct _glib_sux_donkeys lambdafoo;

				/* this sux, perform an or operation on the result(s) of each word */
				for (i=0;i<argc;i++) {
					if (argv[i]->type == ESEXP_RES_STRING) {
						pa = ibex_find(search->body_index, argv[i]->value.string);
						for (j=0;j<pa->len;j++) {
							g_hash_table_insert(ht, g_ptr_array_index(pa, j), (void *)1);
						}
						g_ptr_array_free(pa, FALSE);
					} else {
						g_warning("invalid type passed to body-contains");
					}
				}
				lambdafoo.uids = g_ptr_array_new();
				g_hash_table_foreach(ht, (GHFunc)g_lib_sux_htor, &lambdafoo);
				r->value.ptrarray = lambdafoo.uids;
				g_hash_table_destroy(ht);
			}
		} else {
			r->value.ptrarray = g_ptr_array_new();
		}
	}

	return r;
}

static ESExpResult *search_user_flag(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;
	int i;

	r(printf("executing user-flag\n"));

	/* are we inside a match-all? */
	if (search->current) {
		int truth = FALSE;
		/* performs an OR of all words */
		for (i=0;i<argc && !truth;i++) {
			if (argv[i]->type == ESEXP_RES_STRING
			    && camel_flag_get(&search->current->user_flags, argv[i]->value.string)) {
				truth = TRUE;
				break;
			}
		}
		r = e_sexp_result_new(ESEXP_RES_BOOL);
		r->value.bool = truth;
	} else {
		r = e_sexp_result_new(ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new();
	}

	return r;
}

static ESExpResult *search_user_tag(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;

	r(printf("executing user-tag\n"));

	/* are we inside a match-all? */
	if (search->current) {
		const char *value = NULL;
		if (argc == 1) {
			value = camel_tag_get(&search->current->user_tags, argv[0]->value.string);
		}
		r = e_sexp_result_new(ESEXP_RES_STRING);
		r->value.string = g_strdup(value?value:"");
	} else {
		r = e_sexp_result_new(ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new();
	}

	return r;
}
