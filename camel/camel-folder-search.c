/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2003 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
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
 */

/* This is a helper class for folders to implement the search function.
   It implements enough to do basic searches on folders that can provide
   an in-memory summary and a body index. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>

#include <glib.h>

#include "camel-folder-search.h"
#include "camel-folder-thread.h"

#include "camel-exception.h"
#include "camel-medium.h"
#include "camel-multipart.h"
#include "camel-mime-message.h"
#include "camel-stream-mem.h"
#include "e-util/e-memory.h"
#include "camel-search-private.h"
#include "camel-i18n.h"

#define d(x) 
#define r(x) 

struct _CamelFolderSearchPrivate {
	GHashTable *mempool_hash;
	CamelException *ex;

	CamelFolderThread *threads;
	GHashTable *threads_hash;
};

#define _PRIVATE(o) (((CamelFolderSearch *)(o))->priv)

static ESExpResult *search_not(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);

static ESExpResult *search_header_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_header_matches(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_header_starts_with(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_header_ends_with(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_header_exists(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_match_all(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *search);
static ESExpResult *search_match_threads(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);
static ESExpResult *search_body_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_user_flag(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
static ESExpResult *search_user_tag(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
static ESExpResult *search_system_flag(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
static ESExpResult *search_get_sent_date(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
static ESExpResult *search_get_received_date(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
static ESExpResult *search_get_current_date(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
static ESExpResult *search_get_size(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
static ESExpResult *search_uid(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);

static ESExpResult *search_dummy(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);

static void camel_folder_search_class_init (CamelFolderSearchClass *klass);
static void camel_folder_search_init       (CamelFolderSearch *obj);
static void camel_folder_search_finalize   (CamelObject *obj);

static CamelObjectClass *camel_folder_search_parent;

static void
camel_folder_search_class_init (CamelFolderSearchClass *klass)
{
	camel_folder_search_parent = camel_type_get_global_classfuncs (camel_object_get_type ());

	klass->not = search_not;

	klass->match_all = search_match_all;
	klass->match_threads = search_match_threads;
	klass->body_contains = search_body_contains;
	klass->header_contains = search_header_contains;
	klass->header_matches = search_header_matches;
	klass->header_starts_with = search_header_starts_with;
	klass->header_ends_with = search_header_ends_with;
	klass->header_exists = search_header_exists;
	klass->user_tag = search_user_tag;
	klass->user_flag = search_user_flag;
	klass->system_flag = search_system_flag;
	klass->get_sent_date = search_get_sent_date;
	klass->get_received_date = search_get_received_date;
	klass->get_current_date = search_get_current_date;
	klass->get_size = search_get_size;
	klass->uid = search_uid;
}

static void
camel_folder_search_init (CamelFolderSearch *obj)
{
	struct _CamelFolderSearchPrivate *p;

	p = _PRIVATE(obj) = g_malloc0(sizeof(*p));

	obj->sexp = e_sexp_new();

	/* use a hash of mempools to associate the returned uid lists with
	   the backing mempool.  yes pretty weird, but i didn't want to change
	   the api just yet */

	p->mempool_hash = g_hash_table_new(0, 0);
}

static void
free_mempool(void *key, void *value, void *data)
{
	GPtrArray *uids = key;
	EMemPool *pool = value;

	g_warning("Search closed with outstanding result unfreed: %p", uids);

	g_ptr_array_free(uids, TRUE);
	e_mempool_destroy(pool);
}

static void
camel_folder_search_finalize (CamelObject *obj)
{
	CamelFolderSearch *search = (CamelFolderSearch *)obj;
	struct _CamelFolderSearchPrivate *p = _PRIVATE(obj);

	if (search->sexp)
		e_sexp_unref(search->sexp);
	if (search->summary_hash)
		g_hash_table_destroy(search->summary_hash);

	g_free(search->last_search);
	g_hash_table_foreach(p->mempool_hash, free_mempool, obj);
	g_hash_table_destroy(p->mempool_hash);
	g_free(p);
}

CamelType
camel_folder_search_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_object_get_type (), "CamelFolderSearch",
					    sizeof (CamelFolderSearch),
					    sizeof (CamelFolderSearchClass),
					    (CamelObjectClassInitFunc) camel_folder_search_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_folder_search_init,
					    (CamelObjectFinalizeFunc) camel_folder_search_finalize);
	}
	
	return type;
}

#ifdef offsetof
#define CAMEL_STRUCT_OFFSET(type, field)        ((gint) offsetof (type, field))
#else
#define CAMEL_STRUCT_OFFSET(type, field)        ((gint) ((gchar*) &((type *) 0)->field))
#endif

struct {
	char *name;
	int offset;
	int flags;		/* 0x02 = immediate, 0x01 = always enter */
} builtins[] = {
	/* these have default implementations in e-sexp */
	{ "and", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, and), 2 },
	{ "or", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, or), 2 },
	/* we need to override this one though to implement an 'array not' */
	{ "not", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, not), 0 },
	{ "<", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, lt), 2 },
	{ ">", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, gt), 2 },
	{ "=", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, eq), 2 },

	/* these we have to use our own default if there is none */
	/* they should all be defined in the language? so it parses, or should they not?? */
	{ "match-all", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, match_all), 3 },
	{ "match-threads", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, match_threads), 3 },
	{ "body-contains", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, body_contains), 1 },
	{ "header-contains", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, header_contains), 1 },
	{ "header-matches", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, header_matches), 1 },
	{ "header-starts-with", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, header_starts_with), 1 },
	{ "header-ends-with", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, header_ends_with), 1 },
	{ "header-exists", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, header_exists), 1 },
	{ "user-tag", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, user_tag), 1 },
	{ "user-flag", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, user_flag), 1 },
	{ "system-flag", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, system_flag), 1 },
	{ "get-sent-date", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, get_sent_date), 1 },
	{ "get-received-date", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, get_received_date), 1 },
	{ "get-current-date", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, get_current_date), 1 },
	{ "get-size", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, get_size), 1 },
	{ "uid", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, uid), 1 },
};

void
camel_folder_search_construct (CamelFolderSearch *search)
{
	int i;
	CamelFolderSearchClass *klass = (CamelFolderSearchClass *)CAMEL_OBJECT_GET_CLASS(search);

	for (i=0;i<sizeof(builtins)/sizeof(builtins[0]);i++) {
		void *func;
		/* c is sure messy sometimes */
		func = *((void **)(((char *)klass)+builtins[i].offset));
		if (func == NULL && builtins[i].flags&1) {
			g_warning("Search class doesn't implement '%s' method: %s", builtins[i].name, camel_type_to_name(CAMEL_OBJECT_GET_CLASS(search)));
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
	CamelFolderSearch *new = CAMEL_FOLDER_SEARCH (camel_object_new (camel_folder_search_get_type ()));

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
	int i;

	search->summary = summary;
	if (search->summary_hash)
		g_hash_table_destroy(search->summary_hash);
	search->summary_hash = g_hash_table_new(g_str_hash, g_str_equal);
	for (i=0;i<summary->len;i++)
		g_hash_table_insert(search->summary_hash, (char *)camel_message_info_uid(summary->pdata[i]), summary->pdata[i]);
}

/**
 * camel_folder_search_set_body_index:
 * @search: 
 * @index: 
 * 
 * Set the index representing the contents of all messages
 * in this folder.  If this is not set, then the folder implementation
 * should sub-class the CamelFolderSearch and provide its own
 * body-contains function.
 **/
void
camel_folder_search_set_body_index(CamelFolderSearch *search, CamelIndex *index)
{
	if (search->body_index)
		camel_object_unref((CamelObject *)search->body_index);
	search->body_index = index;
	if (index)
		camel_object_ref((CamelObject *)index);
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
	GPtrArray *matches;
	int i;
	GHashTable *results;
	EMemPool *pool;
	struct _CamelFolderSearchPrivate *p = _PRIVATE(search);

	p->ex = ex;

	/* only re-parse if the search has changed */
	if (search->last_search == NULL
	    || strcmp(search->last_search, expr)) {
		e_sexp_input_text(search->sexp, expr, strlen(expr));
		if (e_sexp_parse(search->sexp) == -1) {
			camel_exception_setv(ex, 1, _("Cannot parse search expression: %s:\n%s"), e_sexp_error(search->sexp), expr);
			return NULL;
		}

		g_free(search->last_search);
		search->last_search = g_strdup(expr);
	}
	r = e_sexp_eval(search->sexp);
	if (r == NULL) {
		if (!camel_exception_is_set(ex))
			camel_exception_setv(ex, 1, _("Error executing search expression: %s:\n%s"), e_sexp_error(search->sexp), expr);
		return NULL;
	}

	matches = g_ptr_array_new();

	/* now create a folder summary to return?? */
	if (r->type == ESEXP_RES_ARRAY_PTR) {
		d(printf("got result ...\n"));
		/* we use a mempool to store the strings, packed in tight as possible, and freed together */
		/* because the strings are often short (like <8 bytes long), we would be wasting appx 50%
		   of memory just storing the size tag that malloc assigns us and alignment padding, so this
		   gets around that (and is faster to allocate and free as a bonus) */
		pool = e_mempool_new(512, 256, E_MEMPOOL_ALIGN_BYTE);
		if (search->summary) {
			/* reorder result in summary order */
			results = g_hash_table_new(g_str_hash, g_str_equal);
			for (i=0;i<r->value.ptrarray->len;i++) {
				d(printf("adding match: %s\n", (char *)g_ptr_array_index(r->value.ptrarray, i)));
				g_hash_table_insert(results, g_ptr_array_index(r->value.ptrarray, i), GINT_TO_POINTER (1));
			}
			for (i=0;i<search->summary->len;i++) {
				CamelMessageInfo *info = g_ptr_array_index(search->summary, i);
				char *uid = (char *)camel_message_info_uid(info);
				if (g_hash_table_lookup(results, uid)) {
					g_ptr_array_add(matches, e_mempool_strdup(pool, uid));
				}
			}
			g_hash_table_destroy(results);
		} else {
			for (i=0;i<r->value.ptrarray->len;i++) {
				d(printf("adding match: %s\n", (char *)g_ptr_array_index(r->value.ptrarray, i)));
				g_ptr_array_add(matches, e_mempool_strdup(pool, g_ptr_array_index(r->value.ptrarray, i)));
			}
		}
		/* instead of putting the mempool_hash in the structure, we keep the api clean by
		   putting a reference to it in a hashtable.  Lets us do some debugging and catch
		   unfree'd results as well. */
		g_hash_table_insert(p->mempool_hash, matches, pool);
	} else {
		g_warning("Search returned an invalid result type");
	}

	e_sexp_result_free(search->sexp, r);

	if (p->threads)
		camel_folder_thread_messages_unref(p->threads);
	if (p->threads_hash)
		g_hash_table_destroy(p->threads_hash);

	p->threads = NULL;
	p->threads_hash = NULL;
	search->folder = NULL;
	search->summary = NULL;
	search->current = NULL;
	search->body_index = NULL;

	return matches;
}

/**
 * camel_folder_search_search:
 * @search: 
 * @expr: 
 * @uids: to search against, NULL for all uid's.
 * @ex: 
 * 
 * Run a search.  Search must have had Folder already set on it, and
 * it must implement summaries.
 * 
 * Return value: 
 **/
GPtrArray *
camel_folder_search_search(CamelFolderSearch *search, const char *expr, GPtrArray *uids, CamelException *ex)
{
	ESExpResult *r;
	GPtrArray *matches = NULL, *summary_set;
	int i;
	GHashTable *results;
	EMemPool *pool;
	struct _CamelFolderSearchPrivate *p = _PRIVATE(search);

	g_assert(search->folder);

	p->ex = ex;

	/* setup our search list, summary_hash only contains those we're interested in */
	search->summary = camel_folder_get_summary(search->folder);
	search->summary_hash = g_hash_table_new(g_str_hash, g_str_equal);

	if (uids) {
		GHashTable *uids_hash = g_hash_table_new(g_str_hash, g_str_equal);

		summary_set = search->summary_set = g_ptr_array_new();
		for (i=0;i<uids->len;i++)
			g_hash_table_insert(uids_hash, uids->pdata[i], uids->pdata[i]);
		for (i=0;i<search->summary->len;i++)
			if (g_hash_table_lookup(uids_hash, camel_message_info_uid(search->summary->pdata[i])))
				g_ptr_array_add(search->summary_set, search->summary->pdata[i]);
	} else {
		summary_set = search->summary;
	}

	for (i=0;i<summary_set->len;i++)
		g_hash_table_insert(search->summary_hash, (char *)camel_message_info_uid(summary_set->pdata[i]), summary_set->pdata[i]);

	/* only re-parse if the search has changed */
	if (search->last_search == NULL
	    || strcmp(search->last_search, expr)) {
		e_sexp_input_text(search->sexp, expr, strlen(expr));
		if (e_sexp_parse(search->sexp) == -1) {
			camel_exception_setv(ex, 1, _("Cannot parse search expression: %s:\n%s"), e_sexp_error(search->sexp), expr);
			goto fail;
		}

		g_free(search->last_search);
		search->last_search = g_strdup(expr);
	}
	r = e_sexp_eval(search->sexp);
	if (r == NULL) {
		if (!camel_exception_is_set(ex))
			camel_exception_setv(ex, 1, _("Error executing search expression: %s:\n%s"), e_sexp_error(search->sexp), expr);
		goto fail;
	}

	matches = g_ptr_array_new();

	/* now create a folder summary to return?? */
	if (r->type == ESEXP_RES_ARRAY_PTR) {
		d(printf("got result ...\n"));

		/* we use a mempool to store the strings, packed in tight as possible, and freed together */
		/* because the strings are often short (like <8 bytes long), we would be wasting appx 50%
		   of memory just storing the size tag that malloc assigns us and alignment padding, so this
		   gets around that (and is faster to allocate and free as a bonus) */
		pool = e_mempool_new(512, 256, E_MEMPOOL_ALIGN_BYTE);
		/* reorder result in summary order */
		results = g_hash_table_new(g_str_hash, g_str_equal);
		for (i=0;i<r->value.ptrarray->len;i++) {
			d(printf("adding match: %s\n", (char *)g_ptr_array_index(r->value.ptrarray, i)));
			g_hash_table_insert(results, g_ptr_array_index(r->value.ptrarray, i), GINT_TO_POINTER (1));
		}

		for (i=0;i<summary_set->len;i++) {
			CamelMessageInfo *info = g_ptr_array_index(summary_set, i);
			char *uid = (char *)camel_message_info_uid(info);
			if (g_hash_table_lookup(results, uid))
				g_ptr_array_add(matches, e_mempool_strdup(pool, uid));
		}
		g_hash_table_destroy(results);

		/* instead of putting the mempool_hash in the structure, we keep the api clean by
		   putting a reference to it in a hashtable.  Lets us do some debugging and catch
		   unfree'd results as well. */
		g_hash_table_insert(p->mempool_hash, matches, pool);
	} else {
		g_warning("Search returned an invalid result type");
	}

	e_sexp_result_free(search->sexp, r);
fail:
	/* these might be allocated by match-threads */
	if (p->threads)
		camel_folder_thread_messages_unref(p->threads);
	if (p->threads_hash)
		g_hash_table_destroy(p->threads_hash);
	if (search->summary_set)
		g_ptr_array_free(search->summary_set, TRUE);
	g_hash_table_destroy(search->summary_hash);
	camel_folder_free_summary(search->folder, search->summary);

	p->threads = NULL;
	p->threads_hash = NULL;
	search->folder = NULL;
	search->summary = NULL;
	search->summary_hash = NULL;
	search->summary_set = NULL;
	search->current = NULL;
	search->body_index = NULL;

	return matches;
}

void camel_folder_search_free_result(CamelFolderSearch *search, GPtrArray *result)
{
	int i;
	struct _CamelFolderSearchPrivate *p = _PRIVATE(search);
	EMemPool *pool;

	pool = g_hash_table_lookup(p->mempool_hash, result);
	if (pool) {
		e_mempool_destroy(pool);
		g_hash_table_remove(p->mempool_hash, result);
	} else {
		for (i=0;i<result->len;i++)
			g_free(g_ptr_array_index(result, i));
	}
	g_ptr_array_free(result, TRUE);
}

/* dummy function, returns false always, or an empty match array */
static ESExpResult *
search_dummy(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;

	if (search->current == NULL) {
		r = e_sexp_result_new(f, ESEXP_RES_BOOL);
		r->value.bool = FALSE;
	} else {
		r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new();
	}

	return r;
}

/* impelemnt an 'array not', i.e. everything in the summary, not in the supplied array */
static ESExpResult *
search_not(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;
	int i;

	if (argc>0) {
		if (argv[0]->type == ESEXP_RES_ARRAY_PTR) {
			GPtrArray *v = argv[0]->value.ptrarray;
			const char *uid;

			r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
			r->value.ptrarray = g_ptr_array_new();

			/* not against a single message?*/
			if (search->current) {
				int found = FALSE;

				uid = camel_message_info_uid(search->current);
				for (i=0;!found && i<v->len;i++) {
					if (strcmp(uid, v->pdata[i]) == 0)
						found = TRUE;
				}

				if (!found)
					g_ptr_array_add(r->value.ptrarray, (char *)uid);
			} else if (search->summary == NULL) {
				g_warning("No summary set, 'not' against an array requires a summary");
			} else {
				/* 'not' against the whole summary */
				GHashTable *have = g_hash_table_new(g_str_hash, g_str_equal);
				char **s;
				CamelMessageInfo **m;

				s = (char **)v->pdata;
				for (i=0;i<v->len;i++)
					g_hash_table_insert(have, s[i], s[i]);

				v = search->summary_set?search->summary_set:search->summary;
				m = (CamelMessageInfo **)v->pdata;
				for (i=0;i<v->len;i++) {
					char *uid = (char *)camel_message_info_uid(m[i]);

					if (g_hash_table_lookup(have, uid) == NULL)
						g_ptr_array_add(r->value.ptrarray, uid);
				}
				g_hash_table_destroy(have);
			}
		} else {
			int res = TRUE;

			if (argv[0]->type == ESEXP_RES_BOOL)
				res = ! argv[0]->value.bool;

			r = e_sexp_result_new(f, ESEXP_RES_BOOL);
			r->value.bool = res;
		}
	} else {
		r = e_sexp_result_new(f, ESEXP_RES_BOOL);
		r->value.bool = TRUE;
	}

	return r;
}

static ESExpResult *
search_match_all(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *search)
{
	int i;
	ESExpResult *r, *r1;
	GPtrArray *v;

	if (argc>1) {
		g_warning("match-all only takes a single argument, other arguments ignored");
	}

	/* we are only matching a single message?  or already inside a match-all? */
	if (search->current) {
		d(printf("matching against 1 message: %s\n", camel_message_info_subject(search->current)));

		r = e_sexp_result_new(f, ESEXP_RES_BOOL);
		r->value.bool = FALSE;

		if (argc>0) {
			r1 = e_sexp_term_eval(f, argv[0]);
			if (r1->type == ESEXP_RES_BOOL) {
				r->value.bool = r1->value.bool;
			} else {
				g_warning("invalid syntax, matches require a single bool result");
				e_sexp_fatal_error(f, _("(match-all) requires a single bool result"));
			}
			e_sexp_result_free(f, r1);
		} else {
			r->value.bool = TRUE;
		}
		return r;
	}

	r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
	r->value.ptrarray = g_ptr_array_new();

	if (search->summary == NULL) {
		/* TODO: make it work - e.g. use the folder and so forth for a slower search */
		g_warning("No summary supplied, match-all doesn't work with no summary");
		g_assert(0);
		return r;
	}

	v = search->summary_set?search->summary_set:search->summary;
	for (i=0;i<v->len;i++) {
		const char *uid;

		search->current = g_ptr_array_index(v, i);
		uid = camel_message_info_uid(search->current);

		if (argc>0) {
			r1 = e_sexp_term_eval(f, argv[0]);
			if (r1->type == ESEXP_RES_BOOL) {
				if (r1->value.bool)
					g_ptr_array_add(r->value.ptrarray, (char *)uid);
			} else {
				g_warning("invalid syntax, matches require a single bool result");
				e_sexp_fatal_error(f, _("(match-all) requires a single bool result"));
			}
			e_sexp_result_free(f, r1);
		} else {
			g_ptr_array_add(r->value.ptrarray, (char *)uid);
		}
	}
	search->current = NULL;

	return r;
}

static void
fill_thread_table(struct _CamelFolderThreadNode *root, GHashTable *id_hash)
{
	while (root) {
		g_hash_table_insert(id_hash, (char *)camel_message_info_uid(root->message), root);
		if (root->child)
			fill_thread_table(root->child, id_hash);
		root = root->next;
	}
}

static void
add_thread_results(struct _CamelFolderThreadNode *root, GHashTable *result_hash)
{
	while (root) {
		g_hash_table_insert(result_hash, (char *)camel_message_info_uid(root->message), GINT_TO_POINTER (1));
		if (root->child)
			add_thread_results(root->child, result_hash);
		root = root->next;
	}
}

static void
add_results(char *uid, void *dummy, GPtrArray *result)
{
	g_ptr_array_add(result, uid);
}

static ESExpResult *
search_match_threads(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *search)
{
	ESExpResult *r;
	struct _CamelFolderSearchPrivate *p = search->priv;
	int i, type;
	GHashTable *results;

	/* not supported in match-all */
	if (search->current)
		e_sexp_fatal_error(f, _("(match-threads) not allowed inside match-all"));

	if (argc == 0)
		e_sexp_fatal_error(f, _("(match-threads) requires a match type string"));

	r = e_sexp_term_eval(f, argv[0]);
	if (r->type != ESEXP_RES_STRING)
		e_sexp_fatal_error(f, _("(match-threads) requires a match type string"));

	type = 0;
	if (!strcmp(r->value.string, "none"))
		type = 0;
	else if (!strcmp(r->value.string, "all"))
		type = 1;
	else if (!strcmp(r->value.string, "replies"))
		type = 2;
	else if (!strcmp(r->value.string, "replies_parents"))
		type = 3;
	e_sexp_result_free(f, r);

	/* behave as (begin does */
	r = NULL;
	for (i=1;i<argc;i++) {
		if (r)
			e_sexp_result_free(f, r);
		r = e_sexp_term_eval(f, argv[i]);
	}

	if (r == NULL || r->type != ESEXP_RES_ARRAY_PTR)
		e_sexp_fatal_error(f, _("(match-threads) expects an array result"));

	if (type == 0)
		return r;

	if (search->folder == NULL)
		e_sexp_fatal_error(f, _("(match-threads) requires the folder set"));

	/* cache this, so we only have to re-calculate once per search at most */
	if (p->threads == NULL) {
		p->threads = camel_folder_thread_messages_new(search->folder, NULL, TRUE);
		p->threads_hash = g_hash_table_new(g_str_hash, g_str_equal);

		fill_thread_table(p->threads->tree, p->threads_hash);
	}

	results = g_hash_table_new(g_str_hash, g_str_equal);
	for (i=0;i<r->value.ptrarray->len;i++) {
		d(printf("adding match: %s\n", (char *)g_ptr_array_index(r->value.ptrarray, i)));
		g_hash_table_insert(results, g_ptr_array_index(r->value.ptrarray, i), GINT_TO_POINTER (1));
	}

	for (i=0;i<r->value.ptrarray->len;i++) {
		struct _CamelFolderThreadNode *node, *scan;

		node = g_hash_table_lookup(p->threads_hash, (char *)g_ptr_array_index(r->value.ptrarray, i));
		if (node == NULL) /* this shouldn't happen but why cry over spilt milk */
			continue;

		/* select messages in thread according to search criteria */
		if (type == 3) {
			scan = node;
			while (scan && scan->parent) {
				scan = scan->parent;
				g_hash_table_insert(results, (char *)camel_message_info_uid(scan->message), GINT_TO_POINTER(1));
			}
		} else if (type == 1) {
			while (node && node->parent)
				node = node->parent;
		}
		g_hash_table_insert(results, (char *)camel_message_info_uid(node->message), GINT_TO_POINTER(1));
		if (node->child)
			add_thread_results(node->child, results);
	}
	e_sexp_result_free(f, r);

	r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
	r->value.ptrarray = g_ptr_array_new();

	g_hash_table_foreach(results, (GHFunc)add_results, r->value.ptrarray);
	g_hash_table_destroy(results);

	return r;
}

static ESExpResult *
check_header(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search, camel_search_match_t how)
{
	ESExpResult *r;
	int truth = FALSE;

	r(printf("executing check-header %d\n", how));

	/* are we inside a match-all? */
	if (search->current && argc>1
	    && argv[0]->type == ESEXP_RES_STRING) {
		char *headername;
		const char *header = NULL;
		char strbuf[32];
		int i, j;
		camel_search_t type = CAMEL_SEARCH_TYPE_ASIS;
		struct _camel_search_words *words;

		/* only a subset of headers are supported .. */
		headername = argv[0]->value.string;
		if (!strcasecmp(headername, "subject")) {
			header = camel_message_info_subject(search->current);
		} else if (!strcasecmp(headername, "date")) {
			/* FIXME: not a very useful form of the date */
			sprintf(strbuf, "%d", (int)search->current->date_sent);
			header = strbuf;
		} else if (!strcasecmp(headername, "from")) {
			header = camel_message_info_from(search->current);
			type = CAMEL_SEARCH_TYPE_ADDRESS;
		} else if (!strcasecmp(headername, "to")) {
			header = camel_message_info_to(search->current);
			type = CAMEL_SEARCH_TYPE_ADDRESS;
		} else if (!strcasecmp(headername, "cc")) {
			header = camel_message_info_cc(search->current);
			type = CAMEL_SEARCH_TYPE_ADDRESS;
		} else if (!strcasecmp(headername, "x-camel-mlist")) {
			header = camel_message_info_mlist(search->current);
			type = CAMEL_SEARCH_TYPE_MLIST;
		} else {
			e_sexp_resultv_free(f, argc, argv);
			e_sexp_fatal_error(f, _("Performing query on unknown header: %s"), headername);
		}

		if (header) {
			/* performs an OR of all words */
			for (i=1;i<argc && !truth;i++) {
				if (argv[i]->type == ESEXP_RES_STRING) {
					if (argv[i]->value.string[0] == 0) {
						truth = TRUE;
					} else if (how == CAMEL_SEARCH_MATCH_CONTAINS) {
						/* doesn't make sense to split words on anything but contains i.e. we can't have an ending match different words */
						words = camel_search_words_split(argv[i]->value.string);
						truth = TRUE;
						for (j=0;j<words->len && truth;j++) {
							truth = camel_search_header_match(header, words->words[j]->word, how, type, NULL);
						}
						camel_search_words_free(words);
					} else {
						truth = camel_search_header_match(header, argv[i]->value.string, how, type, NULL);
					}
				}
			}
		}
	}
	/* TODO: else, find all matches */

	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = truth;

	return r;
}

static ESExpResult *
search_header_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	return check_header(f, argc, argv, search, CAMEL_SEARCH_MATCH_CONTAINS);
}

static ESExpResult *
search_header_matches(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	return check_header(f, argc, argv, search, CAMEL_SEARCH_MATCH_EXACT);
}

static ESExpResult *
search_header_starts_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	return check_header(f, argc, argv, search, CAMEL_SEARCH_MATCH_STARTS);
}

static ESExpResult *
search_header_ends_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	return check_header(f, argc, argv, search, CAMEL_SEARCH_MATCH_ENDS);
}

static ESExpResult *
search_header_exists (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;
	
	r(printf ("executing header-exists\n"));
	
	if (search->current) {
		r = e_sexp_result_new(f, ESEXP_RES_BOOL);
		if (argc == 1 && argv[0]->type == ESEXP_RES_STRING)
			r->value.bool = camel_medium_get_header(CAMEL_MEDIUM(search->current), argv[0]->value.string) != NULL;
		
	} else {
		r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new();
	}
	
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

/* and, only store duplicates */
static void
g_lib_sux_htand(char *key, int value, struct _glib_sux_donkeys *fuckup)
{
	if (value == fuckup->count)
		g_ptr_array_add(fuckup->uids, key);
}

static int
match_message_index(CamelIndex *idx, const char *uid, const char *match, CamelException *ex)
{
	CamelIndexCursor *wc, *nc;
	const char *word, *name;
	int truth = FALSE;

	wc = camel_index_words(idx);
	if (wc) {
		while (!truth && (word = camel_index_cursor_next(wc))) {
			if (camel_ustrstrcase(word,match) != NULL) {
				/* perf: could have the wc cursor return the name cursor */
				nc = camel_index_find(idx, word);
				if (nc) {
					while (!truth && (name = camel_index_cursor_next(nc)))
						truth = strcmp(name, uid) == 0;
					camel_object_unref((CamelObject *)nc);
				}
			}
		}
		camel_object_unref((CamelObject *)wc);
	}

	return truth;
}

/*
 "one two" "three" "four five"

  one and two
or
  three
or
  four and five
*/

/* returns messages which contain all words listed in words */
static GPtrArray *
match_words_index(CamelFolderSearch *search, struct _camel_search_words *words, CamelException *ex)
{
	GPtrArray *result = g_ptr_array_new();
	GHashTable *ht = g_hash_table_new(g_str_hash, g_str_equal);
	struct _glib_sux_donkeys lambdafoo;
	CamelIndexCursor *wc, *nc;
	const char *word, *name;
	CamelMessageInfo *mi;
	int i;

	/* we can have a maximum of 32 words, as we use it as the AND mask */
			
	wc = camel_index_words(search->body_index);
	if (wc) {
		while ((word = camel_index_cursor_next(wc))) {
			for (i=0;i<words->len;i++) {
				if (camel_ustrstrcase(word, words->words[i]->word) != NULL) {
					/* perf: could have the wc cursor return the name cursor */
					nc = camel_index_find(search->body_index, word);
					if (nc) {
						while ((name = camel_index_cursor_next(nc))) {
							mi = g_hash_table_lookup(search->summary_hash, name);
							if (mi) {
								int mask;
								const char *uid = camel_message_info_uid(mi);

								mask = (GPOINTER_TO_INT(g_hash_table_lookup(ht, uid))) | (1<<i);
								g_hash_table_insert(ht, (char *)uid, GINT_TO_POINTER(mask));
							}
						}
						camel_object_unref((CamelObject *)nc);
					}
				}
			}
		}
		camel_object_unref((CamelObject *)wc);

		lambdafoo.uids = result;
		lambdafoo.count = (1<<words->len) - 1;
		g_hash_table_foreach(ht, (GHFunc)g_lib_sux_htand, &lambdafoo);
		g_hash_table_destroy(ht);
	}

	return result;
}

static gboolean
match_words_1message (CamelDataWrapper *object, struct _camel_search_words *words, guint32 *mask)
{
	CamelDataWrapper *containee;
	int truth = FALSE;
	int parts, i;
	
	containee = camel_medium_get_content_object (CAMEL_MEDIUM (object));
	
	if (containee == NULL)
		return FALSE;
	
	/* using the object types is more accurate than using the mime/types */
	if (CAMEL_IS_MULTIPART (containee)) {
		parts = camel_multipart_get_number (CAMEL_MULTIPART (containee));
		for (i = 0; i < parts && truth == FALSE; i++) {
			CamelDataWrapper *part = (CamelDataWrapper *)camel_multipart_get_part (CAMEL_MULTIPART (containee), i);
			if (part)
				truth = match_words_1message(part, words, mask);
		}
	} else if (CAMEL_IS_MIME_MESSAGE (containee)) {
		/* for messages we only look at its contents */
		truth = match_words_1message((CamelDataWrapper *)containee, words, mask);
	} else if (camel_content_type_is(CAMEL_DATA_WRAPPER (containee)->mime_type, "text", "*")) {
		/* for all other text parts, we look inside, otherwise we dont care */
		CamelStreamMem *mem = (CamelStreamMem *)camel_stream_mem_new ();

		/* FIXME: The match should be part of a stream op */
		camel_data_wrapper_decode_to_stream (containee, CAMEL_STREAM (mem));
		camel_stream_write (CAMEL_STREAM (mem), "", 1);
		for (i=0;i<words->len;i++) {
			/* FIXME: This is horridly slow, and should use a real search algorithm */
			if (camel_ustrstrcase(mem->buffer->data, words->words[i]->word) != NULL) {
				*mask |= (1<<i);
				/* shortcut a match */
				if (*mask == (1<<(words->len))-1)
					return TRUE;
			}
		}
		
		camel_object_unref (mem);
	}
	
	return truth;
}

static gboolean
match_words_message(CamelFolder *folder, const char *uid, struct _camel_search_words *words, CamelException *ex)
{
	guint32 mask;
	CamelMimeMessage *msg;
	int truth;

	msg = camel_folder_get_message(folder, uid, ex);
	if (msg) {
		mask = 0;
		truth = match_words_1message((CamelDataWrapper *)msg, words, &mask);
		camel_object_unref((CamelObject *)msg);
	} else {
		camel_exception_clear(ex);
		truth = FALSE;
	}

	return truth;
}

static GPtrArray *
match_words_messages(CamelFolderSearch *search, struct _camel_search_words *words, CamelException *ex)
{
	int i;
	GPtrArray *matches = g_ptr_array_new();

	if (search->body_index) {
		GPtrArray *indexed;
		struct _camel_search_words *simple;

		simple = camel_search_words_simple(words);
		indexed = match_words_index(search, simple, ex);
		camel_search_words_free(simple);

		for (i=0;i<indexed->len;i++) {
			const char *uid = g_ptr_array_index(indexed, i);
			
			if (match_words_message(search->folder, uid, words, ex))
				g_ptr_array_add(matches, (char *)uid);
		}
		
		g_ptr_array_free(indexed, TRUE);
	} else {
		GPtrArray *v = search->summary_set?search->summary_set:search->summary;

		for (i=0;i<v->len;i++) {
			CamelMessageInfo *info = g_ptr_array_index(v, i);
			const char *uid = camel_message_info_uid(info);
			
			if (match_words_message(search->folder, uid, words, ex))
				g_ptr_array_add(matches, (char *)uid);
		}
	}

	return matches;
}

static ESExpResult *
search_body_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	int i, j;
	CamelException *ex = search->priv->ex;
	struct _camel_search_words *words;
	ESExpResult *r;
	struct _glib_sux_donkeys lambdafoo;

	if (search->current) {	
		int truth = FALSE;

		if (argc == 1 && argv[0]->value.string[0] == 0) {
			truth = TRUE;
		} else {
			for (i=0;i<argc && !truth;i++) {
				if (argv[i]->type == ESEXP_RES_STRING) {
					words = camel_search_words_split(argv[i]->value.string);
					truth = TRUE;
					if ((words->type & CAMEL_SEARCH_WORD_COMPLEX) == 0 && search->body_index) {
						for (j=0;j<words->len && truth;j++)
							truth = match_message_index(search->body_index, camel_message_info_uid(search->current), words->words[j]->word, ex);
					} else {
						/* TODO: cache current message incase of multiple body search terms */
						truth = match_words_message(search->folder, camel_message_info_uid(search->current), words, ex);
					}
					camel_search_words_free(words);
				}
			}
		}
		r = e_sexp_result_new(f, ESEXP_RES_BOOL);
		r->value.bool = truth;
	} else {
		r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new();

		if (argc == 1 && argv[0]->value.string[0] == 0) {
			GPtrArray *v = search->summary_set?search->summary_set:search->summary;

			for (i=0;i<v->len;i++) {
				CamelMessageInfo *info = g_ptr_array_index(v, i);

				g_ptr_array_add(r->value.ptrarray, (char *)camel_message_info_uid(info));
			}
		} else {
			GHashTable *ht = g_hash_table_new(g_str_hash, g_str_equal);
			GPtrArray *matches;

			for (i=0;i<argc;i++) {
				if (argv[i]->type == ESEXP_RES_STRING) {
					words = camel_search_words_split(argv[i]->value.string);
					if ((words->type & CAMEL_SEARCH_WORD_COMPLEX) == 0 && search->body_index) {
						matches = match_words_index(search, words, ex);
					} else {
						matches = match_words_messages(search, words, ex);
					}
					for (j=0;j<matches->len;j++)
						g_hash_table_insert(ht, matches->pdata[j], matches->pdata[j]);
					g_ptr_array_free(matches, TRUE);
					camel_search_words_free(words);
				}
			}
			lambdafoo.uids = r->value.ptrarray;
			g_hash_table_foreach(ht, (GHFunc)g_lib_sux_htor, &lambdafoo);
			g_hash_table_destroy(ht);
		}
	}

	return r;
}

static ESExpResult *
search_user_flag(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
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
		r = e_sexp_result_new(f, ESEXP_RES_BOOL);
		r->value.bool = truth;
	} else {
		r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new();
	}

	return r;
}

static ESExpResult *
search_system_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;
	
	r(printf ("executing system-flag\n"));
	
	if (search->current) {
		gboolean truth = FALSE;
		
		if (argc == 1)
			truth = camel_system_flag_get (search->current->flags, argv[0]->value.string);
		
		r = e_sexp_result_new(f, ESEXP_RES_BOOL);
		r->value.bool = truth;
	} else {
		r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new ();
	}
	
	return r;
}

static ESExpResult *
search_user_tag(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	const char *value = NULL;
	ESExpResult *r;
	
	r(printf("executing user-tag\n"));
	
	if (argc == 1)
		value = camel_tag_get (&search->current->user_tags, argv[0]->value.string);
	
	r = e_sexp_result_new(f, ESEXP_RES_STRING);
	r->value.string = g_strdup (value ? value : "");
	
	return r;
}

static ESExpResult *
search_get_sent_date(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s)
{
	ESExpResult *r;

	r(printf("executing get-sent-date\n"));

	/* are we inside a match-all? */
	if (s->current) {
		r = e_sexp_result_new(f, ESEXP_RES_INT);

		r->value.number = s->current->date_sent;
	} else {
		r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new ();
	}

	return r;
}

static ESExpResult *
search_get_received_date(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s)
{
	ESExpResult *r;

	r(printf("executing get-received-date\n"));

	/* are we inside a match-all? */
	if (s->current) {
		r = e_sexp_result_new(f, ESEXP_RES_INT);

		r->value.number = s->current->date_received;
	} else {
		r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new ();
	}

	return r;
}

static ESExpResult *
search_get_current_date(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s)
{
	ESExpResult *r;

	r(printf("executing get-current-date\n"));

	r = e_sexp_result_new(f, ESEXP_RES_INT);
	r->value.number = time (NULL);
	return r;
}

static ESExpResult *
search_get_size (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s)
{
	ESExpResult *r;
	
	r(printf("executing get-size\n"));
	
	/* are we inside a match-all? */
	if (s->current) {
		r = e_sexp_result_new (f, ESEXP_RES_INT);
		r->value.number = s->current->size / 1024;
	} else {
		r = e_sexp_result_new (f, ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new ();
	}
	
	return r;
}

static ESExpResult *
search_uid(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;
	int i;

	r(printf("executing uid\n"));

	/* are we inside a match-all? */
	if (search->current) {
		int truth = FALSE;
		const char *uid = camel_message_info_uid(search->current);

		/* performs an OR of all words */
		for (i=0;i<argc && !truth;i++) {
			if (argv[i]->type == ESEXP_RES_STRING
			    && !strcmp(uid, argv[i]->value.string)) {
				truth = TRUE;
				break;
			}
		}
		r = e_sexp_result_new(f, ESEXP_RES_BOOL);
		r->value.bool = truth;
	} else {
		r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new();
		for (i=0;i<argc;i++) {
			if (argv[i]->type == ESEXP_RES_STRING)
				g_ptr_array_add(r->value.ptrarray, argv[i]->value.string);
		}
	}

	return r;
}
