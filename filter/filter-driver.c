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

#include "filter-arg-types.h"
#include "filter-xml.h"
#include "e-sexp.h"
#include "filter-format.h"

#include <camel/camel.h>

struct _FilterDriverPrivate {
	GList *rules, *options;
	GHashTable *globals;	/* global variables */

	/* run-time data */
	GHashTable *folders;	/* currently open folders */
	GList *matches;		/* all messages which match current rule */
	GHashTable *terminated;	/* messages for which processing is terminated */
	GHashTable *processed;	/* all messages that were processed in some way */

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
static ESExpResult *do_forward(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);
static ESExpResult *do_copy(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);
static ESExpResult *do_stop(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);

static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "delete", (ESExpFunc *)do_delete, 0 },
	{ "forward-to", (ESExpFunc *)do_forward, 0 },
	{ "copy-to", (ESExpFunc *)do_copy, 0 },
	{ "stop", (ESExpFunc *)do_stop, 0 },
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
 *
 * Create a new FilterDriver object.
 * 
 * Return value: A new FilterDriver widget.
 **/
FilterDriver *
filter_driver_new (void)
{
	FilterDriver *new = FILTER_DRIVER (gtk_type_new (filter_driver_get_type ()));
	return new;
}


void filter_driver_set_session(FilterDriver *d, CamelSession *s)
{
	if (d->session)
		gtk_object_unref((GtkObject *)s);
	d->session = s;
	if (s)
		gtk_object_ref((GtkObject *)s);
}

int filter_driver_set_rules(FilterDriver *d, const char *description, const char *filter)
{
	struct _FilterDriverPrivate *p = _PRIVATE(d);
	xmlDocPtr desc, filt;

	printf("Loading system '%s'\nLoading user '%s'\n", description, filter);

#warning "fix leaks, free xml docs here"
	desc = xmlParseFile(description);
	p->rules = filter_load_ruleset(desc);

	filt = xmlParseFile(filter);
	p->options = filter_load_optionset(filt, p->rules);

#warning "Zucchi: is this safe? Doesn't seem to cause problems..."
	filter_load_ruleset_free (p->rules);
	xmlFreeDoc (desc);
	
	return 0;
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

extern int filter_find_arg(FilterArg *a, char *name);

/*

  foreach rule
    find matches

  foreach action
    get all matches

 */

/*
  splices ${cc} lines into a single string
*/
static int
expand_variables(GString *out, char *source, GList *args, GHashTable *globals)
{
	GList *argl;
	FilterArg *arg;
	char *name= alloca(32);
	char *start, *end, *newstart, *tmp, *val;
	int namelen=32;
	int len=0;
	int ok = 0;

	printf("expanding %s\n", source);

	start = source;
	while ( (newstart = strstr(start, "${"))
		&& (end = strstr(newstart+2, "}")) ) {
		len = end-newstart-2;
		if (len+1>namelen) {
			namelen = (len+1)*2;
			name = alloca(namelen);
		}
		memcpy(name, newstart+2, len);
		name[len] = 0;
		printf("looking for name '%s'\n", name);
		argl = g_list_find_custom(args, name, (GCompareFunc) filter_find_arg);
		if (argl) {
			int i, count;

			tmp = g_strdup_printf("%.*s", newstart-start, start);
			printf("appending: %s\n", tmp);
			g_string_append(out, tmp);
			g_free(tmp);

			arg = argl->data;
			count = filter_arg_get_count(arg);
			for (i=0;i<count;i++) {
				printf("appending '%s'\n", filter_arg_get_value_as_string(arg, i));
				g_string_append(out, " \"");
				g_string_append(out, filter_arg_get_value_as_string(arg, i));
				g_string_append(out, "\"");
			}
		} else if ( (val = g_hash_table_lookup(globals, name)) ) {
			tmp = g_strdup_printf("%.*s", newstart-start, start);
			printf("appending: %s\n", tmp);
			g_string_append(out, tmp);
			g_free(tmp);
			g_string_append(out, " \"");
			g_string_append(out, val);
			g_string_append(out, "\"");
		} else {
			ok = 1;
			tmp = g_strdup_printf("%.*s", end-start+1, start);
			printf("appending: '%s'\n", tmp);
			g_string_append(out, tmp);
			g_free(tmp);
		}
		start = end+1;
	}
	g_string_append(out, start);

	return ok;
}

/*
  build an expression for the filter
*/
void
filter_driver_expand_option(FilterDriver *d, GString *s, GString *action, struct filter_option *op)
{
	GList *optionl;
	FilterArg *arg;
	struct _FilterDriverPrivate *p = _PRIVATE(d);

	if (s) {
		g_string_append(s, "(and ");
		optionl = op->options;
		while (optionl) {
			struct filter_optionrule *or = optionl->data;
			if (or->rule->type == FILTER_XML_MATCH
			    || or->rule->type == FILTER_XML_EXCEPT) {
				if (or->args) {
					arg = or->args->data;
					if (arg) {
						printf("arg = %s\n", arg->name);
					}
				}
				expand_variables(s, or->rule->code, or->args, p->globals);
			}
			optionl = g_list_next(optionl);
		}

		g_string_append(s, ")");
	}

	if (action) {
		g_string_append(action, "(begin ");
		optionl = op->options;
		while (optionl) {
			struct filter_optionrule *or = optionl->data;
			if (or->rule->type == FILTER_XML_ACTION) {
				expand_variables(action, or->rule->code, or->args, p->globals);
				g_string_append(action, " ");
			}
			optionl = g_list_next(optionl);
		}
		g_string_append(action, ")");
	}

	if (s)
		printf("combined rule '%s'\n", s->str);
	if (action)
		printf("combined action '%s'\n", action->str);
}

static ESExpResult *
do_delete(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *d)
{
	struct _FilterDriverPrivate *p = _PRIVATE(d);
	GList *m;

	printf("doing delete\n");
	m = p->matches;
	while (m) {
		printf(" %s\n", (char *)m->data);

		camel_folder_set_message_flags (p->source, m->data, CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED, p->ex);

		m = m->next;
	}
	return NULL;
}

static ESExpResult *
do_forward(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *d)
{
	struct _FilterDriverPrivate *p = _PRIVATE(d);
	GList *m;

	printf("doing forward on the following messages:\n");
	m = p->matches;
	while (m) {
		printf(" %s\n", (char *)m->data);
		m = m->next;
	}
	return NULL;
}

static ESExpResult *
do_copy(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *d)
{
	GList *m;
	int i;
	struct _FilterDriverPrivate *p = _PRIVATE(d);

	printf("doing copy\n");
	for (i=0;i<argc;i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			char *folder = argv[i]->value.string;
			CamelFolder *outbox;

			/* FIXME: this might have to find another store, based on
			   the folder as a url??? */
			printf("opening outpbox %s\n", folder);
			outbox = open_folder(d, folder);
			if (outbox == NULL) {
				g_warning("Cannot open folder: %s", folder);
				continue;
			}

			m = p->matches;
			while (m) {
				CamelMimeMessage *mm;

				printf("appending message %s\n", (char *)m->data);

				mm = camel_folder_get_message_by_uid(p->source, m->data, p->ex);
				camel_folder_append_message(outbox, mm, p->ex);
				gtk_object_unref((GtkObject *)mm);

				printf(" %s\n", (char *)m->data);
				m = m->next;
			}
		}
	}

	return NULL;
}

static ESExpResult *
do_stop(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *d)
{
	GList *m;
	struct _FilterDriverPrivate *p = _PRIVATE(d);

	printf("doing stop on the following messages:\n");
	m = p->matches;
	while (m) {
		printf(" %s\n", (char *)m->data);
		g_hash_table_insert(p->terminated, g_strdup(m->data), (void *)1);
		m = m->next;
	}
	return NULL;
}

static CamelFolder *
open_folder(FilterDriver *d, const char *folder_url)
{
	char *folder, *store;
	CamelStore *camelstore;
	CamelFolder *camelfolder;
	struct _FilterDriverPrivate *p = _PRIVATE(d);

	/* we have a lookup table of currently open folders */
	camelfolder = g_hash_table_lookup(p->folders, folder_url);
	if (camelfolder)
		return camelfolder;

	store = g_strdup(folder_url);
	folder = strrchr(store, '/');
	if (folder == NULL || folder == store || folder[1]==0)
		goto fail;

	*folder++ = 0;
	camelstore = camel_session_get_store (d->session, store, p->ex);
	if (camel_exception_get_id (p->ex)) {
		printf ("Could not open store: %s: %s", store, camel_exception_get_description (p->ex));
		goto fail;
	}

	camelfolder = camel_store_get_folder (camelstore, folder, TRUE, p->ex);
	if (camel_exception_get_id (p->ex)) {
		printf ("Could not open folder: %s: %s", folder, camel_exception_get_description (p->ex));
		goto fail;
	}

	printf("opening folder: %s\n", folder_url);

	g_free(store);

	g_hash_table_insert(p->folders, g_strdup(folder_url), camelfolder);

	return camelfolder;

fail:
	g_free(store);
	return NULL;
}

static void
close_folder(void *key, void *value, void *data)
{
	CamelFolder *f = value;
	FilterDriver *d = data;
	struct _FilterDriverPrivate *p = _PRIVATE(d);

	printf("closing folder: %s\n", (char *) key);

	g_free(key);
	camel_folder_sync(f, FALSE, p->ex);
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

int
filter_driver_run(FilterDriver *d, CamelFolder *source, CamelFolder *inbox)
{
	struct _FilterDriverPrivate *p = _PRIVATE(d);
	ESExpResult *r;
	GList *options;
	GString *s, *a;
	GPtrArray *all;
	GList *m;
	int i;

#warning "This must be made mega-robust"
	p->source = source;

	/* setup runtime data */
	p->folders = g_hash_table_new(g_str_hash, g_str_equal);
	p->terminated = g_hash_table_new(g_str_hash, g_str_equal);
	p->processed = g_hash_table_new(g_str_hash, g_str_equal);

	camel_exception_init(p->ex);

	options = p->options;
	while (options) {
		struct filter_option *fo = options->data;

		s = g_string_new("");
		a = g_string_new("");
		filter_driver_expand_option(d, s, a, fo);

		printf("searching expression %s\n", s->str);
		p->matches = camel_folder_search_by_expression  (p->source, s->str, p->ex);

		/* remove uid's for which processing is complete ... */
		m = p->matches;
		while (m) {
			GList *n = m->next;

			printf("matched: %s\n", (char *) m->data);

			/* for all matching id's, so we can work out what to default */
			if (g_hash_table_lookup(p->processed, m->data) == NULL) {
				g_hash_table_insert(p->processed, g_strdup(m->data), (void *)1);
			}

			if (g_hash_table_lookup(p->terminated, m->data)) {
				printf("removing terminated message %s\n", (char *)m->data);
				p->matches = g_list_remove_link(p->matches, m);
			}
			m = n;
		}

		printf("applying actions ... '%s'\n", a->str);
		e_sexp_input_text(p->eval, a->str, strlen(a->str));
		e_sexp_parse(p->eval);
		r = e_sexp_eval(p->eval);
		e_sexp_result_free(r);

		g_string_free(s, TRUE);
		g_string_free(a, TRUE);

		g_list_free(p->matches);
		
		options = g_list_next(options);
	}

	/* apply the default of copying to an inbox, if we are given one, and make sure
	   we delete everything as well */
	all = camel_folder_get_uids(p->source, p->ex);
	for (i = 0; i < all->len; i++) {
		char *uid = all->pdata[i], *procuid;
		CamelMimeMessage *mm;

		procuid = g_hash_table_lookup(p->processed, uid);
		if (procuid == NULL) {
			if (inbox) {
				printf("Applying default rule to message %s\n", uid);

				mm = camel_folder_get_message_by_uid(p->source, all->pdata[i], p->ex);
				camel_folder_append_message(inbox, mm, p->ex);
				gtk_object_unref((GtkObject *)mm);
				camel_folder_set_message_flags(p->source, all->pdata[i], CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED, p->ex);
			}
		} else {
			camel_folder_delete_message_by_uid(p->source, uid, p->ex);
		}
	}
	camel_folder_free_uids(p->source, all);

	g_hash_table_destroy(p->processed);
	g_hash_table_destroy(p->terminated);
	close_folders(d);
	g_hash_table_destroy(p->folders);

	return 0;
}
