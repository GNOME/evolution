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
#include <ctype.h>
#include <glib.h>
#include <sys/types.h>
#include <regex.h>

#warning "Fixme: remove gal/widgets/e-unicode dependency"
#include <gal/widgets/e-unicode.h>
#include "camel-folder-search.h"
#include "string-utils.h"

#include "camel-exception.h"
#include "camel-medium.h"
#include "camel-multipart.h"
#include "camel-mime-message.h"
#include "camel-stream-mem.h"
#include "e-util/e-memory.h"

#define d(x)
#define r(x)

struct _CamelFolderSearchPrivate {
	GHashTable *mempool_hash;
	CamelException *ex;
};

#define _PRIVATE(o) (((CamelFolderSearch *)(o))->priv)

static ESExpResult *search_header_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_header_matches(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_header_starts_with(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_header_ends_with(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_header_exists(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_match_all(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *search);
static ESExpResult *search_body_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);
static ESExpResult *search_user_flag(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
static ESExpResult *search_user_tag(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
static ESExpResult *search_system_flag(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
static ESExpResult *search_get_sent_date(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
static ESExpResult *search_get_received_date(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
static ESExpResult *search_get_current_date(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);

static ESExpResult *search_dummy(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search);

static void camel_folder_search_class_init (CamelFolderSearchClass *klass);
static void camel_folder_search_init       (CamelFolderSearch *obj);
static void camel_folder_search_finalize   (CamelObject *obj);

static CamelObjectClass *camel_folder_search_parent;

static void
camel_folder_search_class_init (CamelFolderSearchClass *klass)
{
	camel_folder_search_parent = camel_type_get_global_classfuncs (camel_object_get_type ());

	klass->match_all = search_match_all;
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
	{ "not", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, not), 2 },
	{ "<", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, lt), 2 },
	{ ">", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, gt), 2 },
	{ "=", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, eq), 2 },

	/* these we have to use our own default if there is none */
	/* they should all be defined in the language? so it poarses, or should they not?? */
	{ "match-all", CAMEL_STRUCT_OFFSET(CamelFolderSearchClass, match_all), 3 },
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
			g_warning("Search class doesn't implement '%s' method: %s", builtins[i].name, camel_type_to_name(CAMEL_OBJECT_GET_CLASS(search)->s.type));
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
	CamelFolderSearch *new = CAMEL_FOLDER_SEARCH ( camel_object_new (camel_folder_search_get_type ()));

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
	if (r
	    && r->type == ESEXP_RES_ARRAY_PTR) {
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
				g_hash_table_insert(results, g_ptr_array_index(r->value.ptrarray, i), (void *)1);
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
		e_sexp_result_free(r);
		/* instead of putting the mempool_hash in the structure, we keep the api clean by
		   putting a reference to it in a hashtable.  Lets us do some debugging and catch
		   unfree'd results as well. */
		g_hash_table_insert(p->mempool_hash, matches, pool);
	} else {
		printf("no result!\n");
	}

	search->folder = NULL;
	search->summary = NULL;
	search->current = NULL;
	search->body_index = NULL;

	return matches;
}

/**
 * camel_folder_search_match_expression:
 * @search: 
 * @expr: 
 * @info: 
 * @ex: 
 * 
 * Returns #TRUE if the expression matches the specific message info @info.
 * Note that the folder and index may need to be set for body searches to
 * operate as well.
 * 
 * Return value: 
 **/
gboolean
camel_folder_search_match_expression(CamelFolderSearch *search, const char *expr, const CamelMessageInfo *info, CamelException *ex)
{
	GPtrArray *uids;
	int ret = FALSE;

	search->match1 = (CamelMessageInfo *)info;

	uids = camel_folder_search_execute_expression(search, expr, ex);
	if (uids) {
		if (uids->len == 1)
			ret = TRUE;
		camel_folder_search_free_result(search, uids);
	}
	search->match1 = NULL;

	return ret;
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

	/* we are only matching a single message? */
	if (search->match1) {
		search->current = search->match1;

		if (argc>0) {
			r1 = e_sexp_term_eval(f, argv[0]);
			if (r1->type == ESEXP_RES_BOOL) {
				if (r1->value.bool)
					g_ptr_array_add(r->value.ptrarray, (char *)camel_message_info_uid(search->current));
			} else {
				g_warning("invalid syntax, matches require a single bool result");
				e_sexp_fatal_error(f, _("(match-all) requires a single bool result"));
			}
			e_sexp_result_free(r1);
		} else {
			g_ptr_array_add(r->value.ptrarray, (char *)camel_message_info_uid(search->current));
		}
		search->current = NULL;

		return r;
	}

	if (search->summary == NULL) {
		/* TODO: make it work - e.g. use the folder and so forth for a slower search */
		g_warning("No summary supplied, match-all doesn't work with no summary");
		g_assert(0);
		return r;
	}

	/* TODO: Could make this a bit faster in the uncommon case (of match-everything) */
	for (i=0;i<search->summary->len;i++) {
		search->current = g_ptr_array_index(search->summary, i);
		if (argc>0) {
			r1 = e_sexp_term_eval(f, argv[0]);
			if (r1->type == ESEXP_RES_BOOL) {
				if (r1->value.bool)
					g_ptr_array_add(r->value.ptrarray, (char *)camel_message_info_uid(search->current));
			} else {
				g_warning("invalid syntax, matches require a single bool result");
				e_sexp_fatal_error(f, _("(match-all) requires a single bool result"));
			}
			e_sexp_result_free(r1);
		} else {
			g_ptr_array_add(r->value.ptrarray, (char *)camel_message_info_uid(search->current));
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
		char *headername;
		const char *header = NULL;
		char strbuf[32];
		int i;

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
		} else if (!strcasecmp(headername, "to")) {
			header = camel_message_info_to(search->current);
		} else if (!strcasecmp(headername, "cc")) {
			header = camel_message_info_cc(search->current);
		} else {
			g_warning("Performing query on unknown header: %s", headername);
		}

		if (header) {
			/* performs an OR of all words */
			for (i=1;i<argc && !truth;i++) {
				if (argv[i]->type == ESEXP_RES_STRING
				    && e_utf8_strstrcase (header, argv[i]->value.string)) {
					r(printf("%s got a match with %s of %s\n",
						 camel_message_info_uid(search->current),
						 header, argv[i]->value.string));
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

static ESExpResult *
search_header_matches(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;
	
	r(printf ("executing header-matches\n"));
	
	if (search->current && argc == 2) {
		char *headername;
		const char *header = NULL;
		char strbuf[32];
		gboolean truth = FALSE;
		
		/* only a subset of headers are supported .. */
		headername = argv[0]->value.string;
		if (!strcasecmp (headername, "subject")) {
			header = camel_message_info_subject (search->current);
		} else if (!strcasecmp (headername, "date")) {
			/* FIXME: not a very useful form of the date */
			sprintf (strbuf, "%d", (int)search->current->date_sent);
			header = strbuf;
		} else if (!strcasecmp (headername, "from")) {
			header = camel_message_info_from (search->current);
		} else if (!strcasecmp (headername, "to")) {
			header = camel_message_info_to (search->current);
		} else if (!strcasecmp (headername, "cc")) {
			header = camel_message_info_cc (search->current);
		} else {
			g_warning ("Performing query on unknown header: %s", headername);
		}
		
		if (header && argv[1]->type == ESEXP_RES_STRING) {
			/* danw says to use search-engine style matching...
			 * This means that if the search match string is
			 * lowercase then compare case-insensitive else
			 * compare case-sensitive. */
			gboolean is_lowercase = TRUE;
			char *match = argv[1]->value.string;
			char *c;
			
			/* remove any leading white space... */
			for ( ; *header && isspace (*header); header++);
			
			for (c = match; *c; c++) {
				if (isalpha (*c) && isupper (*c)) {
					is_lowercase = FALSE;
					break;
				}
			}
			
			if (is_lowercase) {
				if (!g_strcasecmp (header, match))
					truth = TRUE;
			} else {
				if (!strcmp (header, match))
					truth = TRUE;
			}
		}
		
		r = e_sexp_result_new (ESEXP_RES_BOOL);
		r->value.bool = truth;
	} else {
		r = e_sexp_result_new (ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new ();
	}
	
	return r;
}

static ESExpResult *
search_header_starts_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;
	
	r(printf ("executing header-starts-with\n"));
	
	if (search->current && argc == 2) {
		char *headername, *match;
		const char *header = NULL;
		char strbuf[32];
		gboolean truth = FALSE;
		
		/* only a subset of headers are supported .. */
		headername = argv[0]->value.string;
		if (!strcasecmp (headername, "subject")) {
			header = camel_message_info_subject (search->current);
		} else if (!strcasecmp (headername, "date")) {
			/* FIXME: not a very useful form of the date */
			sprintf (strbuf, "%d", (int)search->current->date_sent);
			header = strbuf;
		} else if (!strcasecmp (headername, "from")) {
			header = camel_message_info_from (search->current);
		} else if (!strcasecmp (headername, "to")) {
			header = camel_message_info_to (search->current);
		} else if (!strcasecmp (headername, "cc")) {
			header = camel_message_info_cc (search->current);
		} else {
			g_warning ("Performing query on unknown header: %s", headername);
		}
		
		match = argv[1]->value.string;
		
		if (header && strlen (header) >= strlen (match)) {
			/* danw says to use search-engine style matching...
			 * This means that if the search match string is
			 * lowercase then compare case-insensitive else
			 * compare case-sensitive. */
			gboolean is_lowercase = TRUE;
			char *c;
			
			/* remove any leading white space... */
			for ( ; *header && isspace (*header); header++);
			
			for (c = match; *c; c++) {
				if (isalpha (*c) && isupper (*c)) {
					is_lowercase = FALSE;
					break;
				}
			}
			
			if (is_lowercase) {
				if (!g_strncasecmp (header, match, strlen (match)))
					truth = TRUE;
			} else {
				if (!strncmp (header, match, strlen (match)))
					truth = TRUE;
			}
		}
		
		r = e_sexp_result_new (ESEXP_RES_BOOL);
		r->value.bool = truth;
	} else {
		r = e_sexp_result_new (ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new ();
	}
	
	return r;
}

static ESExpResult *
search_header_ends_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;
	
	r(printf ("executing header-ends-with\n"));
	
	if (search->current && argc == 2) {
		char *headername, *match;
		const char *header = NULL;
		char strbuf[32];
		gboolean truth = FALSE;
		
		/* only a subset of headers are supported .. */
		headername = argv[0]->value.string;
		if (!strcasecmp (headername, "subject")) {
			header = camel_message_info_subject (search->current);
		} else if (!strcasecmp (headername, "date")) {
			/* FIXME: not a very useful form of the date */
			sprintf (strbuf, "%d", (int)search->current->date_sent);
			header = strbuf;
		} else if (!strcasecmp (headername, "from")) {
			header = camel_message_info_from (search->current);
		} else if (!strcasecmp (headername, "to")) {
			header = camel_message_info_to (search->current);
		} else if (!strcasecmp (headername, "cc")) {
			header = camel_message_info_cc (search->current);
		} else {
			g_warning ("Performing query on unknown header: %s", headername);
		}
		
		match = argv[1]->value.string;
		
		if (header && strlen (header) >= strlen (match)) {
			/* danw says to use search-engine style matching...
			 * This means that if the search match string is
			 * lowercase then compare case-insensitive else
			 * compare case-sensitive. */
			gboolean is_lowercase = TRUE;
			char *c, *end;
			
			/* remove any leading white space... */
			for ( ; *header && isspace (*header); header++);
			
			for (c = match; *c; c++) {
				if (isalpha (*c) && isupper (*c)) {
					is_lowercase = FALSE;
					break;
				}
			}
			
			end = (char *) header + strlen (header) - strlen (match);
			
			if (is_lowercase) {
				if (!g_strncasecmp (header, match, strlen (match)))
					truth = TRUE;
			} else {
				if (!strncmp (header, match, strlen (match)))
					truth = TRUE;
			}
		}
		
		r = e_sexp_result_new (ESEXP_RES_BOOL);
		r->value.bool = truth;
	} else {
		r = e_sexp_result_new (ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new ();
	}
	
	return r;
}

static ESExpResult *
search_header_exists (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;
	
	r(printf ("executing header-exists\n"));
	
	if (search->current) {
		const gchar *value = NULL;
		
		if (argc == 1 && argv[0]->type == ESEXP_RES_STRING)
			value = camel_medium_get_header (CAMEL_MEDIUM (search->current),
							 argv[0]->value.string);
		
		r = e_sexp_result_new (ESEXP_RES_BOOL);
		r->value.bool = value ? TRUE : FALSE;
	} else {
		r = e_sexp_result_new (ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new ();
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

/* performs a 'slow' content-based match */
/* there is also an identical copy of this in camel-filter-search.c */
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

/* builds the regex into pattern */
static int
build_match_regex(regex_t *pattern, int argc, struct _ESExpResult **argv)
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
			while ((c = *word++)) {
				if (strchr("*\\.()[]^$+", c) != NULL) {
					g_string_append_c(match, '\\');
				}
				g_string_append_c(match, c);
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
		char buffer[1024]; /* dont really care if its longer than this ... */
		
		regerror(err, pattern, buffer, 1023);
		g_warning("Internal error with search pattern: %s: %s", match->str, buffer);
		regfree(pattern);
	}
	d(printf("Built regex: '%s'\n", match->str));
	g_string_free(match, TRUE);
	return err;
}

static int
match_message(CamelFolder *folder, const char *uid, regex_t *pattern)
{
	CamelMimeMessage *msg;
	int truth = FALSE;
	CamelException *ex;

	ex = camel_exception_new();
	msg = camel_folder_get_message(folder, uid, ex);
	if (!camel_exception_is_set(ex) && msg!=NULL) {
		truth = message_body_contains((CamelDataWrapper *)msg, pattern);
		camel_object_unref((CamelObject *)msg);
	}
	camel_exception_free(ex);
	return truth;
}

static ESExpResult *
search_body_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *search)
{
	ESExpResult *r;
	int i, j;
	regex_t pattern;

	if (search->current) {
		int truth = FALSE;

		r = e_sexp_result_new(ESEXP_RES_BOOL);
		if (search->body_index) {
			for (i=0;i<argc && !truth;i++) {
				if (argv[i]->type == ESEXP_RES_STRING) {
					truth = ibex_find_name(search->body_index, (char *)camel_message_info_uid(search->current),
							       argv[i]->value.string);
				} else {
					g_warning("Invalid type passed to body-contains match function");
				}
			}
		} else if (search->folder) {
			/* we do a 'slow' direct search */
			if (build_match_regex(&pattern, argc, argv) == 0) {
				truth = match_message(search->folder, camel_message_info_uid(search->current), &pattern);
				regfree(&pattern);
			}
		} else {
			g_warning("Cannot perform indexed body query with no index or folder set");
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
		} else if (search->folder) {
			/* do a slow search */
			r->value.ptrarray = g_ptr_array_new();
			if (build_match_regex(&pattern, argc, argv) == 0) {
				if (search->summary) {
					for (i=0;i<search->summary->len;i++) {
						CamelMessageInfo *info = g_ptr_array_index(search->summary, i);

						if (match_message(search->folder, camel_message_info_uid(info), &pattern))
							g_ptr_array_add(r->value.ptrarray, (char *)camel_message_info_uid(info));
					}
				} /* else?  we could always get the summary from the folder, but then
				     we need to free it later somehow */
				regfree(&pattern);
			}
		} else {
			g_warning("Cannot perform indexed body query with no index or folder set");
			r->value.ptrarray = g_ptr_array_new();
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
		r = e_sexp_result_new(ESEXP_RES_BOOL);
		r->value.bool = truth;
	} else {
		r = e_sexp_result_new(ESEXP_RES_ARRAY_PTR);
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
		
		r = e_sexp_result_new (ESEXP_RES_BOOL);
		r->value.bool = truth;
	} else {
		r = e_sexp_result_new (ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new ();
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

static ESExpResult *
search_get_sent_date(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s)
{
	ESExpResult *r;

	r(printf("executing get-sent-date\n"));

	/* are we inside a match-all? */
	if (s->current) {
		r = e_sexp_result_new (ESEXP_RES_INT);

		r->value.number = s->current->date_sent;
	} else {
		r = e_sexp_result_new (ESEXP_RES_ARRAY_PTR);
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
		r = e_sexp_result_new (ESEXP_RES_INT);

		r->value.number = s->current->date_received;
	} else {
		r = e_sexp_result_new (ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new ();
	}

	return r;
}

static ESExpResult *
search_get_current_date(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s)
{
	ESExpResult *r;

	r(printf("executing get-current-date\n"));

	r = e_sexp_result_new (ESEXP_RES_INT);
	r->value.number = time (NULL);
	return r;
}

