/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *           Jeffrey Stedfast <fejj@helixcode.com>
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
#include "mail/mail-tools.h" /*mail_tool_camel_lock_up*/
#include "filter-context.h"
#include "filter-filter.h"
#include "e-util/e-sexp.h"

/* mail-thread filter input data type */
typedef struct {
	FilterDriver *driver;
	CamelFolder *source;
	CamelFolder *inbox;
	gboolean self_destruct;
	gpointer unhook_func;
	gpointer unhook_data;
} filter_mail_input_t;

/* mail-thread filter functions */
static gchar *describe_filter_mail (gpointer in_data, gboolean gerund);
static void setup_filter_mail (gpointer in_data, gpointer op_data, CamelException *ex);
static void do_filter_mail (gpointer in_data, gpointer op_data, CamelException *ex);
static void cleanup_filter_mail (gpointer in_data, gpointer op_data, CamelException *ex);


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

	GList *searches;	/* search results */

	CamelException *ex;

	/* evaluator */
	ESExp *eval;
};

#define _PRIVATE(o) (((FilterDriver *)(o))->priv)

static void filter_driver_class_init (FilterDriverClass *klass);
static void filter_driver_init       (FilterDriver *obj);
static void filter_driver_finalise   (GtkObject *obj);

static CamelFolder *open_folder (FilterDriver *d, const char *folder_url);
static int close_folders (FilterDriver *d);

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

	p = _PRIVATE (obj) = g_malloc0 (sizeof (*p));

	p->eval = e_sexp_new ();
	/* Load in builtin symbols */
	for (i = 0; i < sizeof (symbols) / sizeof (symbols[0]); i++) {
		if (symbols[i].type == 1) {
			e_sexp_add_ifunction (p->eval, 0, symbols[i].name, (ESExpIFunc *)symbols[i].func, obj);
		} else {
			e_sexp_add_function (p->eval, 0, symbols[i].name, symbols[i].func, obj);
		}
	}

	p->globals = g_hash_table_new (g_str_hash, g_str_equal);

	/* Will get set in filter_driver_run */
	p->ex = NULL;
}

static void
free_hash_strings (void *key, void *value, void *data)
{
	g_free (key);
	g_free (value);
}

static void
filter_driver_finalise (GtkObject *obj)
{
	FilterDriver *d = (FilterDriver *) obj;
	struct _FilterDriverPrivate *p = _PRIVATE (d);

	g_hash_table_foreach (p->globals, free_hash_strings, d);
	g_hash_table_destroy (p->globals);

	gtk_object_unref (GTK_OBJECT (p->eval));

	/*Was set to the mail_operation_queue exception,
	 * not our responsibility to free it.*/
	/*camel_exception_free (p->ex);*/
	
	g_free (p);

	((GtkObjectClass *)(filter_driver_parent))->finalize (GTK_OBJECT (obj));
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


#if 0
void filter_driver_set_global(FilterDriver *d, const char *name, const char *value)
{
	struct _FilterDriverPrivate *p = _PRIVATE (d);
	char *oldkey, *oldvalue;

	if (g_hash_table_lookup_extended (p->globals, name, (void *)&oldkey, (void *)&oldvalue)) {
		g_free (oldvalue);
		g_hash_table_insert (p->globals, oldkey, g_strdup (value));
	} else {
		g_hash_table_insert (p->globals, g_strdup (name), g_strdup (value));
	}
}
#endif

static ESExpResult *
do_delete (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	char *uid;
	int i;

	printf ("doing delete\n");
	for (i = 0; i < p->matches->len; i++) {
		uid = p->matches->pdata[i];
		printf (" %s\n", uid);

		mail_tool_camel_lock_up ();
		camel_folder_delete_message (p->source, uid);
		mail_tool_camel_lock_down ();
	}
	return NULL;
}

static ESExpResult *
mark_forward (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	int i;

	printf ("marking the following messages for forwarding:\n");
	for (i = 0; i < p->matches->len; i++) {
		printf (" %s\n", (char *)p->matches->pdata[i]);
	}
	return NULL;
}

static ESExpResult *
mark_copy (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	int i, m;
	char *uid;

	printf ("marking for copy\n");
	for (i = 0; i < argc; i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			char *folder = argv[i]->value.string;
			CamelFolder *outbox;

			outbox = open_folder (driver, folder);
			if (outbox == NULL)
				continue;

			for (m = 0; m < p->matches->len; m++) {
				gpointer old_key, old_value;

				uid = p->matches->pdata[m];
				printf (" %s\n", uid);

				if (g_hash_table_lookup_extended (p->copies, uid, &old_key, &old_value))
					g_hash_table_insert (p->copies, old_key, g_list_prepend (old_value, outbox));
				else
					g_hash_table_insert (p->copies, uid, g_list_append (NULL, outbox));
			}
		}
	}

	return NULL;
}

static ESExpResult *
do_stop (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	char *uid;
	int i;

	printf ("doing stop on the following messages:\n");
	for (i = 0; i < p->matches->len; i++) {
		uid = p->matches->pdata[i];
		printf (" %s\n", uid);
		g_hash_table_insert (p->terminated, uid, GINT_TO_POINTER (1));
	}
	return NULL;
}

static ESExpResult *
do_colour(struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *d)
{
	int i;
	char *uid;
	struct _FilterDriverPrivate *p = _PRIVATE(d);

	if (argc>0 && argv[0]->type == ESEXP_RES_STRING) {
		for (i=0 ; i<p->matches->len; i++) {
			uid = p->matches->pdata[i];
			camel_folder_set_message_user_tag(p->source, uid, "colour", argv[0]->value.string);
		}
	}

	return NULL;
}

static CamelFolder *
open_folder (FilterDriver *driver, const char *folder_url)
{
	CamelFolder *camelfolder;
	struct _FilterDriverPrivate *p = _PRIVATE (driver);

	/* we have a lookup table of currently open folders */
	camelfolder = g_hash_table_lookup (p->folders, folder_url);
	if (camelfolder)
		return camelfolder;

	camelfolder = p->get_folder(driver, folder_url, p->data);

	if (camelfolder) {
		g_hash_table_insert (p->folders, g_strdup (folder_url), camelfolder);
		mail_tool_camel_lock_up ();
		camel_folder_freeze (camelfolder);
		mail_tool_camel_lock_down ();
	}

	return camelfolder;
}

static void
close_folder (void *key, void *value, void *data)
{
	CamelFolder *folder = value;
	FilterDriver *driver = data;
	struct _FilterDriverPrivate *p = _PRIVATE (driver);

	g_free (key);
	mail_tool_camel_lock_up ();
	camel_folder_sync (folder, FALSE, p->ex);
	camel_folder_thaw (folder);
	mail_tool_camel_lock_down ();
	camel_object_unref (CAMEL_OBJECT (folder));
}

/* flush/close all folders */
static int
close_folders (FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);

	g_hash_table_foreach (p->folders, close_folder, driver);
	g_hash_table_destroy (p->folders);
	p->folders = g_hash_table_new (g_str_hash, g_str_equal);

	/* FIXME: status from driver */
	return 0;
}

#if 0
static void
free_key (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
}
#endif

static const mail_operation_spec op_filter_mail =
{
	describe_filter_mail,
	0,
	setup_filter_mail,
	do_filter_mail,
	cleanup_filter_mail
};

void
filter_driver_run (FilterDriver *d, CamelFolder *source, CamelFolder *inbox,
		   gboolean self_destruct, gpointer unhook_func, gpointer unhook_data)
{
	filter_mail_input_t *input;

	input = g_new (filter_mail_input_t, 1);
	input->driver = d;
	input->source = source;
	input->inbox = inbox;
	input->self_destruct = self_destruct;
	input->unhook_func = unhook_func;
	input->unhook_data = unhook_data;

	mail_operation_queue (&op_filter_mail, input, TRUE);
}

static gchar *describe_filter_mail (gpointer in_data, gboolean gerund)
{
	filter_mail_input_t *input = (filter_mail_input_t *) in_data;

	if (gerund)
		return g_strdup_printf ("Filtering messages into \"%s\"",
					mail_tool_get_folder_name (input->inbox));
	else
		return g_strdup_printf ("Filter messages into \"%s\"",
					mail_tool_get_folder_name (input->inbox));
}

static void
setup_filter_mail (gpointer in_data, gpointer op_data, CamelException *ex)
{
	filter_mail_input_t *input = (filter_mail_input_t *) in_data;

	if (!input->driver) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "Bad filter driver passed to filter_mail");
		return;
	}

	if (!CAMEL_IS_FOLDER (input->source)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "Bad source folder passed to filter_mail");
		return;
	}

	if (!CAMEL_IS_FOLDER (input->inbox)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "Bad Inbox passed to filter_mail");
		return;
	}

	camel_object_ref (CAMEL_OBJECT (input->source));
	camel_object_ref (CAMEL_OBJECT (input->inbox));
}

static void
do_filter_mail (gpointer in_data, gpointer op_data, CamelException *ex)
{
	filter_mail_input_t *input = (filter_mail_input_t *) in_data;
	FilterDriver *d = input->driver;
	CamelFolder *source = input->source;
	CamelFolder *inbox = input->inbox;
	struct _FilterDriverPrivate *p = _PRIVATE (d);
	ESExpResult *r;
	GString *s, *a;
	GPtrArray *all;
	char *uid;
	int i;
	FilterFilter *rule;
	GList *l;

	/* FIXME: needs to check all failure cases */
	p->source = source;

	/* setup runtime data */
	p->folders = g_hash_table_new(g_str_hash, g_str_equal);
	p->terminated = g_hash_table_new(g_str_hash, g_str_equal);
	p->processed = g_hash_table_new(g_str_hash, g_str_equal);
	p->copies = g_hash_table_new(g_str_hash, g_str_equal);

	mail_tool_camel_lock_up ();
	camel_folder_freeze(inbox);
	mail_tool_camel_lock_down ();

	s = g_string_new("");
	a = g_string_new("");

	rule = NULL;
	while ( (rule = (FilterFilter *)rule_context_next_rule((RuleContext *)p->context, (FilterRule *)rule)) ) {
		g_string_truncate(s, 0);
		g_string_truncate(a, 0);

		filter_rule_build_code((FilterRule *)rule, s);
		filter_filter_build_action(rule, a);

#if 0
		printf("applying rule %s\n action %s\n", s->str, a->str);
#endif

		mail_tool_camel_lock_up ();
		p->matches = camel_folder_search_by_expression (p->source, s->str, p->ex);
		mail_tool_camel_lock_down ();

		/* remove uid's for which processing is complete ... */
		for (i = 0; i < p->matches->len; i++) {
			uid = p->matches->pdata[i];

			/* for all matching id's, so we can work out what to default */
			if (g_hash_table_lookup (p->processed, uid) == NULL) {
				g_hash_table_insert (p->processed, uid, GINT_TO_POINTER (1));
			}

			if (g_hash_table_lookup (p->terminated, uid)) {
				g_ptr_array_remove_index_fast (p->matches, i);
				i--;
			}
		}

#ifndef NO_WARNINGS
#warning "Must check expression parsed and executed properly?"
#endif
		e_sexp_input_text(p->eval, a->str, strlen(a->str));
		e_sexp_parse(p->eval);
		r = e_sexp_eval(p->eval);
		e_sexp_result_free(r);

		p->searches = g_list_append(p->searches, p->matches);
	}

	g_string_free(s, TRUE);
	g_string_free(a, TRUE);

	/* Do any queued copies, and make sure everything is deleted from
	 * the source. If we have an inbox, anything that didn't get
	 * processed otherwise goes there.
	 */
	mail_tool_camel_lock_up ();
	all = camel_folder_get_uids (p->source);
	mail_tool_camel_lock_down ();
	for (i = 0; i < all->len; i++) {
		char *uid = all->pdata[i], *procuid;
		GList *copies, *tmp;
		CamelMimeMessage *mm;
		const CamelMessageInfo *info;

		copies = g_hash_table_lookup (p->copies, uid);
		procuid = g_hash_table_lookup (p->processed, uid);

		mail_tool_camel_lock_up ();
		info = camel_folder_get_message_info (p->source, uid);

		if (copies || !procuid) {
			mm = camel_folder_get_message (p->source, uid, p->ex);

			while (copies) {
				camel_folder_append_message(copies->data, mm, info, p->ex);
				tmp = copies->next;
				g_list_free_1 (copies);
				copies = tmp;
			}

			if (!procuid) {
				printf("Applying default rule to message %s\n", uid);
				camel_folder_append_message(inbox, mm, info, p->ex);
			}

			camel_object_unref (CAMEL_OBJECT (mm));
		}
		camel_folder_delete_message (p->source, uid);
		mail_tool_camel_lock_down ();

	}
	mail_tool_camel_lock_up ();
	camel_folder_free_uids (p->source, all);
	if (input->unhook_func)
		camel_object_unhook_event (CAMEL_OBJECT (input->inbox), "folder_changed", 
					   input->unhook_func, input->unhook_data);
	mail_tool_camel_lock_down ();

	/* now we no longer need our keys */
	l = p->searches;
	while (l) {
		camel_folder_search_free (p->source, l->data);
		l = l->next;
	}
	g_list_free (p->searches);
	
	g_hash_table_destroy (p->copies);
	g_hash_table_destroy (p->processed);
	g_hash_table_destroy (p->terminated);
	close_folders (d);
	g_hash_table_destroy (p->folders);
	mail_tool_camel_lock_up ();
	camel_folder_sync (p->source, TRUE, ex);
	camel_folder_thaw (inbox);
	mail_tool_camel_lock_down ();
}

static void
cleanup_filter_mail (gpointer in_data, gpointer op_data, CamelException *ex)
{
	filter_mail_input_t *input = (filter_mail_input_t *) in_data;

	camel_object_unref (CAMEL_OBJECT (input->source));
	camel_object_unref (CAMEL_OBJECT (input->inbox));

	if (input->self_destruct)
		gtk_object_unref (GTK_OBJECT (input->driver));
}
