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
#include "filter-message-search.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <gtkhtml/gtkhtml.h>

#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

#include <camel/camel.h>
#include "mail/mail-tools.h" /*mail_tool_camel_lock_up/down*/
#include "filter-context.h"
#include "filter-filter.h"
#include "e-util/e-sexp.h"

#define d(x) x

struct _FilterDriverPrivate {
	GHashTable *globals;       /* global variables */
	
	FilterContext *context;
	
	/* for callback */
	FilterGetFolderFunc get_folder;
	void *data;
	
	/* run-time data */
	GHashTable *folders;       /* folders that message has been copied to */
	GHashTable *forwards;      /* addresses that have been forwarded the message */
	
	gboolean terminated;       /* message processing was terminated */
	gboolean deleted;          /* message was marked for deletion */
	
	CamelMimeMessage *message; /* input message */
	CamelMessageInfo *info;    /* message summary info */
	
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

static ESExpResult *do_delete (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);
static ESExpResult *mark_forward (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);
static ESExpResult *mark_copy (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);
static ESExpResult *do_stop (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);
static ESExpResult *do_colour (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);
static ESExpResult *do_score (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);

/* these are our filter actions - each must have a callback */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "delete",     (ESExpFunc *) do_delete,    0 },
	{ "forward-to", (ESExpFunc *) mark_forward, 0 },
	{ "copy-to",    (ESExpFunc *) mark_copy,    0 },
	{ "stop",       (ESExpFunc *) do_stop,      0 },
	{ "set-colour", (ESExpFunc *) do_colour,    0 },
	{ "set-score",  (ESExpFunc *) do_score,     0 }
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
	p = _PRIVATE (new);
	
	p->get_folder = get_folder;
	p->data = data;
	p->context = context;
	gtk_object_ref (GTK_OBJECT (context));
	
	return new;
}


#if 0
void
filter_driver_set_global (FilterDriver *d, const char *name, const char *value)
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
	
	if (!p->terminated) {
		d(fprintf (stderr, "doing delete\n"));
		p->deleted = TRUE;
	}
	
	return NULL;
}

static ESExpResult *
mark_forward (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	
	if (!p->terminated) {
		d(fprintf (stderr, "marking message for forwarding\n"));
		/* FIXME: do stuff here */
	}
	
	return NULL;
}

static ESExpResult *
mark_copy (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	int i;
	
	if (!p->terminated) {
		d(fprintf (stderr, "copying message...\n"));
		for (i = 0; i < argc; i++) {
			if (argv[i]->type == ESEXP_RES_STRING) {
				/* open folders we intent to copy to */
				char *folder = argv[i]->value.string;
				CamelFolder *outbox;
				
				outbox = g_hash_table_lookup (p->folders, folder);
				if (!outbox) {
					outbox = open_folder (driver, folder);
					if (!outbox)
						continue;
					
					mail_tool_camel_lock_up ();
					camel_folder_append_message (outbox, p->message, p->info, p->ex);
					mail_tool_camel_lock_down ();
				}
			}
		}
	}
	
	return NULL;
}

static ESExpResult *
do_stop (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	
	if (!p->terminated) {
		d(fprintf (stderr, "terminating message processing\n"));
		p->terminated = TRUE;
	}
	
	return NULL;
}

static ESExpResult *
do_colour (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	
	if (!p->terminated) {
		d(fprintf (stderr, "setting colour tag\n"));
		if (argc > 0 && argv[0]->type == ESEXP_RES_STRING) {
			camel_tag_set (&p->info->user_tags, "colour", argv[0]->value.string);
		}
	}
	
	return NULL;
}

static ESExpResult *
do_score (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	
	if (!p->terminated) {
		d(fprintf (stderr, "setting score tag\n"));
		if (argc > 0 && argv[0]->type == ESEXP_RES_INT) {
			char *value;
			
			value = g_strdup_printf ("%d", argv[0]->value.number);
			camel_tag_set (&p->info->user_tags, "score", value);
			g_free (value);
		}
	}
	
	return NULL;
}

static CamelFolder *
open_folder (FilterDriver *driver, const char *folder_url)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	CamelFolder *camelfolder;
	
	/* we have a lookup table of currently open folders */
	camelfolder = g_hash_table_lookup (p->folders, folder_url);
	if (camelfolder)
		return camelfolder;
	
	camelfolder = p->get_folder (driver, folder_url, p->data);
	
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

gboolean
filter_driver_run (FilterDriver *driver, CamelMimeMessage *message, CamelMessageInfo *info,
		   CamelFolder *inbox, enum _filter_source_t sourcetype,
		   gpointer unhook_func, gpointer unhook_data,
		   gboolean self_destruct, CamelException *ex)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	ESExpResult *r;
	GString *fsearch, *faction;
	FilterFilter *rule;
	gboolean filtered = FALSE;
	
	gtk_object_ref (GTK_OBJECT (driver));
	camel_object_ref (CAMEL_OBJECT (message));
	if (inbox)
		camel_object_ref (CAMEL_OBJECT (inbox));
	
	p->ex = camel_exception_new ();
	p->terminated = FALSE;
	p->deleted = FALSE;
	p->message = message;
	p->info = info;
	
	/* setup runtime data */
	p->folders = g_hash_table_new (g_str_hash, g_str_equal);
	
	mail_tool_camel_lock_up ();
	camel_folder_freeze (inbox);
	mail_tool_camel_lock_down ();
	
	fsearch = g_string_new ("");
	faction = g_string_new ("");
	
	rule = NULL;
	while ((rule = (FilterFilter *)rule_context_next_rule ((RuleContext *)p->context, (FilterRule *)rule))) {
		gboolean matched;
		
		if (((FilterRule *)rule)->source != sourcetype) {
			d(fprintf (stderr, "skipping rule %s - wrong source type (%d %d)\n", ((FilterRule *)rule)->name,
				   ((FilterRule *)rule)->source, sourcetype));
			continue;
		}
		
		g_string_truncate (fsearch, 0);
		g_string_truncate (faction, 0);
		
		filter_rule_build_code (FILTER_RULE (rule), fsearch);
		filter_filter_build_action (rule, faction);
		
		d(fprintf (stderr, "applying rule %s\n action %s\n", fsearch->str, faction->str));
		
		mail_tool_camel_lock_up ();
		matched = filter_message_search (p->message, p->info, fsearch->str, p->ex);
		mail_tool_camel_lock_down ();
		
		if (!matched)
			continue;
		
#ifndef NO_WARNINGS
#warning "Must check expression parsed and executed properly?"
#endif
		e_sexp_input_text (p->eval, faction->str, strlen (faction->str));
		e_sexp_parse (p->eval);
		r = e_sexp_eval (p->eval);
		e_sexp_result_free (r);
		
		if (p->terminated)
			break;
	}
	
	g_string_free (fsearch, TRUE);
	g_string_free (faction, TRUE);
	
	if (!p->deleted && g_hash_table_size (p->folders) == 0 && inbox) {
		/* copy it to the default inbox */
		mail_tool_camel_lock_up ();
		camel_folder_append_message (inbox, p->message, p->info, p->ex);
		
		/* warn that inbox was changed */
		if (unhook_func)
			camel_object_unhook_event (CAMEL_OBJECT (inbox), "folder_changed", 
						   unhook_func, unhook_data);
		mail_tool_camel_lock_down ();
	} else {
		filtered = TRUE;
	}
	
	/* close all folders that were opened for appending */
	close_folders (driver);
	g_hash_table_destroy (p->folders);
	
	/* thaw the inbox folder */
	mail_tool_camel_lock_up ();
	camel_folder_thaw (inbox);
	mail_tool_camel_lock_down ();
	
	/* transfer the exception over to the parents exception */
	if (camel_exception_is_set (p->ex))
		camel_exception_xfer (ex, p->ex);
	camel_exception_free (p->ex);
	
	camel_object_unref (CAMEL_OBJECT (message));
	if (inbox)
		camel_object_unref (CAMEL_OBJECT (inbox));
	if (self_destruct)
		gtk_object_unref (GTK_OBJECT (driver));
	
	return filtered;
}
