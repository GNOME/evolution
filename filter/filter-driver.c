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

#include "filter-driver.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <gtkhtml/gtkhtml.h>

#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

#include <camel/camel.h>

#include "filter-context.h"
#include "filter-filter.h"

#include "e-util/e-sexp.h"

struct _FilterDriverPrivate {
	GHashTable *globals;	/* global variables */

	FilterContext *context;

	/* for callback */
	FilterGetFolderFunc get_folder;
	void *data;

	/* run-time data */
	GHashTable *folders;	/* currently open folders */
	GPtrArray *matches;	/* all messages which match current rule */
	GHashTable *terminated;	/* messages for which processing is terminated */
	GHashTable *processed;	/* all messages that were processed in some way */
	GHashTable *copies;	/* lists of folders to copy messages to */

	CamelFolder *source;	/* temporary input folder */

	CamelException *ex;

	/* evaluator */
	ESExp *eval;
};

#define _PRIVATE(o) (((FilterDriver *)(o))->priv)

static void filter_driver_class_init (FilterDriverClass *klass);
static void filter_driver_init       (FilterDriver *obj);
static void filter_driver_finalise   (GtkObject *obj);

static CamelFolder *open_folder(FilterDriver *d, const char *folder_url);
static int close_folders(FilterDriver *d);

static ESExpResult *do_delete(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);
static ESExpResult *mark_forward(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);
static ESExpResult *mark_copy(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);
static ESExpResult *do_stop(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);
static ESExpResult *do_colour(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);

static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "delete", (ESExpFunc *)do_delete, 0 },
	{ "forward-to", (ESExpFunc *)mark_forward, 0 },
	{ "copy-to", (ESExpFunc *)mark_copy, 0 },
	{ "stop", (ESExpFunc *)do_stop, 0 },
	{ "set-colour", (ESExpFunc *)do_colour, 0 },
};

static GtkObjectClass *filter_driver_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_driver_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterDriver",
			sizeof (FilterDriver),
			sizeof (FilterDriverClass),
			(GtkClassInitFunc) filter_driver_class_init,
			(GtkObjectInitFunc) filter_driver_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gtk_object_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_driver_class_init (FilterDriverClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	
	filter_driver_parent = gtk_type_class (gtk_object_get_type ());

	object_class->finalize = filter_driver_finalise;

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
filter_driver_init (FilterDriver *obj)
{
	struct _FilterDriverPrivate *p;
	int i;

	p = _PRIVATE(obj) = g_malloc0(sizeof(*p));

	p->eval = e_sexp_new();
	/* Load in builtin symbols */
	for(i=0;i<sizeof(symbols)/sizeof(symbols[0]);i++) {
		if (symbols[i].type == 1) {
			e_sexp_add_ifunction(p->eval, 0, symbols[i].name, (ESExpIFunc *)symbols[i].func, obj);
		} else {
			e_sexp_add_function(p->eval, 0, symbols[i].name, symbols[i].func, obj);
		}
	}

	p->globals = g_hash_table_new(g_str_hash, g_str_equal);

	p->ex = camel_exception_new ();
}

static void
free_hash_strings(void *key, void *value, void *data)
{
	g_free(key);
	g_free(value);
}

static void
filter_driver_finalise (GtkObject *obj)
{
	FilterDriver *d = (FilterDriver *)obj;
	struct _FilterDriverPrivate *p = _PRIVATE(d);

	g_hash_table_foreach(p->globals, free_hash_strings, d);
	g_hash_table_destroy(p->globals);

	gtk_object_unref((GtkObject *)p->eval);

	camel_exception_free(p->ex);
	
	g_free(p);

	((GtkObjectClass *)(filter_driver_parent))->finalize((GtkObject *)obj);
}

/**
 * filter_driver_new:
 * @system: path to system rules
 * @user: path to user rules
 * @get_folder: function to call to fetch folders
 *
 * Create a new FilterDriver object.
 * 
 * Return value: A new FilterDriver widget.
 **/
FilterDriver *
filter_driver_new (FilterContext *context, FilterGetFolderFunc get_folder, void *data)
{
	FilterDriver *new;
	struct _FilterDriverPrivate *p;

	new = FILTER_DRIVER (gtk_type_new (filter_driver_get_type ()));
	p = _PRIVATE(new);

	p->get_folder = get_folder;
	p->data = data;
	p->context = context;
	gtk_object_ref((GtkObject *)context);

	return new;
}


void filter_driver_set_global(FilterDriver *d, const char *name, const char *value)
{
	struct _FilterDriverPrivate *p = _PRIVATE(d);
	char *oldkey, *oldvalue;

	if (g_hash_table_lookup_extended(p->globals, name, (void *)&oldkey, (void *)&oldvalue)) {
		g_free(oldvalue);
		g_hash_table_insert(p->globals, oldkey, g_strdup(value));
	} else {
		g_hash_table_insert(p->globals, g_strdup(name), g_strdup(value));
	}
}

static ESExpResult *
do_delete(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *d)
{
	struct _FilterDriverPrivate *p = _PRIVATE(d);
	char *uid;
	int i;

	printf("doing delete\n");
	for (i = 0; i < p->matches->len; i++) {
		uid = p->matches->pdata[i];
		printf(" %s\n", uid);

		camel_folder_delete_message (p->source, uid);
	}
	return NULL;
}

static ESExpResult *
mark_forward(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *d)
{
	struct _FilterDriverPrivate *p = _PRIVATE(d);
	int i;

	printf("marking the following messages for forwarding:\n");
	for (i = 0; i < p->matches->len; i++) {
		printf(" %s\n", (char *)p->matches->pdata[i]);
	}
	return NULL;
}

static ESExpResult *
mark_copy(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *d)
{
	int i, m;
	char *uid;
	struct _FilterDriverPrivate *p = _PRIVATE(d);

	printf("marking for copy\n");
	for (i = 0; i < argc; i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			char *folder = argv[i]->value.string;
			CamelFolder *outbox;

			outbox = open_folder(d, folder);
			if (outbox == NULL)
				continue;

			for (m = 0; m < p->matches->len; m++) {
				gpointer old_key, old_value;

				uid = p->matches->pdata[m];
				printf(" %s\n", uid);

				if (g_hash_table_lookup_extended(p->copies, uid, &old_key, &old_value))
					g_hash_table_insert(p->copies, old_key, g_list_prepend(old_value, outbox));
				else
					g_hash_table_insert(p->copies, g_strdup(uid), g_list_append(NULL, outbox));
			}
		}
	}

	return NULL;
}

static ESExpResult *
do_stop(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *d)
{
	int i;
	char *uid;
	struct _FilterDriverPrivate *p = _PRIVATE(d);

	printf("doing stop on the following messages:\n");
	for (i = 0; i < p->matches->len; i++) {
		uid = p->matches->pdata[i];
		printf(" %s\n", uid);
		g_hash_table_insert(p->terminated, g_strdup(uid), (void *)1);
	}
	return NULL;
}

static ESExpResult *
do_colour(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *d)
{
	/* FIXME: implement */
	return NULL;
}

static CamelFolder *
open_folder(FilterDriver *d, const char *folder_url)
{
	CamelFolder *camelfolder;
	struct _FilterDriverPrivate *p = _PRIVATE(d);

	/* we have a lookup table of currently open folders */
	camelfolder = g_hash_table_lookup(p->folders, folder_url);
	if (camelfolder)
		return camelfolder;

	camelfolder = p->get_folder(d, folder_url, p->data);
	if (camelfolder) {
		g_hash_table_insert(p->folders, g_strdup(folder_url), camelfolder);
		camel_folder_freeze(camelfolder);
	}

	return camelfolder;
}

static void
close_folder(void *key, void *value, void *data)
{
	CamelFolder *f = value;
	FilterDriver *d = data;
	struct _FilterDriverPrivate *p = _PRIVATE(d);

	g_free(key);
	camel_folder_sync(f, FALSE, p->ex);
	camel_folder_thaw(f);
	gtk_object_unref((GtkObject *)f);
}

/* flush/close all folders */
static int
close_folders(FilterDriver *d)
{
	struct _FilterDriverPrivate *p = _PRIVATE(d);

	g_hash_table_foreach(p->folders, close_folder, d);
	g_hash_table_destroy(p->folders);
	p->folders = g_hash_table_new(g_str_hash, g_str_equal);

	/* FIXME: status from d */
	return 0;
}

#if 0
int
filter_driver_rule_count(FilterDriver *d)
{
	struct _FilterDriverPrivate *p = _PRIVATE(d);
	return g_list_length(p->options);
}

struct filter_option *
filter_driver_rule_get(FilterDriver *d, int n)
{
	struct _FilterDriverPrivate *p = _PRIVATE(d);
	return g_list_nth_data(p->options, n);
}
#endif

static void
free_key (gpointer key, gpointer value, gpointer user_data)
{
	g_free(key);
}

int
filter_driver_run(FilterDriver *d, CamelFolder *source, CamelFolder *inbox)
{
	struct _FilterDriverPrivate *p = _PRIVATE(d);
	ESExpResult *r;
	GString *s, *a;
	GPtrArray *all;
	char *uid;
	int i;
	FilterFilter *rule;

	/* FIXME: needs to check all failure cases */
	p->source = source;

	/* setup runtime data */
	p->folders = g_hash_table_new(g_str_hash, g_str_equal);
	p->terminated = g_hash_table_new(g_str_hash, g_str_equal);
	p->processed = g_hash_table_new(g_str_hash, g_str_equal);
	p->copies = g_hash_table_new(g_str_hash, g_str_equal);

	camel_exception_init(p->ex);
	camel_folder_freeze(inbox);

	s = g_string_new("");
	a = g_string_new("");

	rule = NULL;
	while ( (rule = (FilterFilter *)rule_context_next_rule((RuleContext *)p->context, (FilterRule *)rule)) ) {
		g_string_truncate(s, 0);
		g_string_truncate(a, 0);

		filter_rule_build_code((FilterRule *)rule, s);
		filter_filter_build_action(rule, a);

		printf("applying rule %s\n action %s\n", s->str, a->str);

		p->matches = camel_folder_search_by_expression (p->source, s->str, p->ex);

		/* remove uid's for which processing is complete ... */
		for (i = 0; i < p->matches->len; i++) {
			uid = p->matches->pdata[i];

			/* for all matching id's, so we can work out what to default */
			if (g_hash_table_lookup(p->processed, uid) == NULL) {
				g_hash_table_insert(p->processed, g_strdup(uid), (void *)1);
			}

			if (g_hash_table_lookup(p->terminated, uid)) {
				g_ptr_array_remove_index_fast(p->matches, i);
				i--;
			}
		}

#warning "Must check expression parsed and executed properly?"
		e_sexp_input_text(p->eval, a->str, strlen(a->str));
		e_sexp_parse(p->eval);
		r = e_sexp_eval(p->eval);
		e_sexp_result_free(r);

		g_strfreev((char **)p->matches->pdata);
		g_ptr_array_free(p->matches, FALSE);
	}

	g_string_free(s, TRUE);
	g_string_free(a, TRUE);

	/* Do any queued copies, and make sure everything is deleted from
	 * the source. If we have an inbox, anything that didn't get
	 * processed otherwise goes there.
	 */
	all = camel_folder_get_uids(p->source);
	for (i = 0; i < all->len; i++) {
		char *uid = all->pdata[i], *procuid;
		GList *copies, *tmp;
		CamelMimeMessage *mm;
		const CamelMessageInfo *info;

		copies = g_hash_table_lookup(p->copies, uid);
		procuid = g_hash_table_lookup(p->processed, uid);

		info = camel_folder_get_message_info (p->source, uid);
		
		if (copies || !procuid) {
			mm = camel_folder_get_message(p->source, uid, p->ex);

			while (copies) {
				camel_folder_append_message(copies->data, mm, info ? info->flags : 0, p->ex);
				tmp = copies->next;
				g_list_free_1(copies);
				copies = tmp;
			}

			if (!procuid) {
				printf("Applying default rule to message %s\n", uid);
				camel_folder_append_message(inbox, mm, info ? info->flags : 0, p->ex);
			}

			gtk_object_unref((GtkObject *)mm);
		}
		camel_folder_delete_message(p->source, uid);
	}
	camel_folder_free_uids(p->source, all);

	g_hash_table_foreach(p->copies, free_key, NULL);
	g_hash_table_destroy(p->copies);
	g_hash_table_foreach(p->processed, free_key, NULL);
	g_hash_table_destroy(p->processed);
	g_hash_table_foreach(p->terminated, free_key, NULL);
	g_hash_table_destroy(p->terminated);
	close_folders(d);
	g_hash_table_destroy(p->folders);
	camel_folder_thaw(inbox);

	return 0;
}
