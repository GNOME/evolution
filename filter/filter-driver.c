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

#include <time.h>

#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

#include <camel/camel.h>
#include "mail/mail-tools.h" /*mail_tool_camel_lock_up/down*/
#include "filter-context.h"
#include "filter-filter.h"
#include "e-util/e-sexp.h"

#define d(x)

struct _FilterDriverPrivate {
	GHashTable *globals;       /* global variables */

	CamelFolder *defaultfolder;	/* defualt folder */
	FDStatusFunc *statusfunc; 	/* status callback */
	void *statusdata;		/* status callback data */

	FilterContext *context;
	
	/* for callback */
	FilterGetFolderFunc get_folder;
	void *data;
	
	/* run-time data */
	GHashTable *folders;       /* folders that message has been copied to */
	GHashTable *forwards;      /* addresses that have been forwarded the message */
	
	gboolean terminated;       /* message processing was terminated */
	gboolean deleted;          /* message was marked for deletion */
	gboolean copied;           /* message was copied to some folder or another */
	
	CamelMimeMessage *message; /* input message */
	CamelMessageInfo *info;    /* message summary info */

	FILE *logfile;             /* log file */
	
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
static ESExpResult *do_copy (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);
static ESExpResult *do_move (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *);
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
	{ "copy-to",    (ESExpFunc *) do_copy,      0 },
	{ "move-to",    (ESExpFunc *) do_move,      0 },
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
	
	p->folders = g_hash_table_new (g_str_hash, g_str_equal);
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
	FilterDriver *driver = (FilterDriver *) obj;
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	
	/* close all folders that were opened for appending */
	close_folders (driver);
	g_hash_table_destroy (p->folders);
	
	g_hash_table_foreach (p->globals, free_hash_strings, driver);
	g_hash_table_destroy (p->globals);
	
	gtk_object_unref (GTK_OBJECT (p->eval));

	if (p->defaultfolder)
		camel_object_unref((CamelObject *)p->defaultfolder);
	
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


void
filter_driver_set_status_func (FilterDriver *d, FDStatusFunc *func, void *data)
{
	struct _FilterDriverPrivate *p = _PRIVATE (d);
	
	p->statusfunc = func;
	p->statusdata = data;
}

void
filter_driver_set_default_folder (FilterDriver *d, CamelFolder *def)
{
	struct _FilterDriverPrivate *p = _PRIVATE (d);
	
	if (p->defaultfolder)
		camel_object_unref (CAMEL_OBJECT (p->defaultfolder));
	
	p->defaultfolder = def;
	
	if (p->defaultfolder)
		camel_object_ref (CAMEL_OBJECT (p->defaultfolder));
}

static void
report_status (FilterDriver *driver, enum filter_status_t status, const char *desc, ...)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	va_list ap;
	char *str;
	
	if (p->statusfunc) {
		va_start (ap, desc);
		str = g_strdup_vprintf (desc, ap);
		p->statusfunc (driver, status, str, p->message, p->statusdata);
		g_free (str);
	}
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
	
	d(fprintf (stderr, "doing delete\n"));
	p->deleted = TRUE;
	p->info->flags = p->info->flags | CAMEL_MESSAGE_DELETED;
	report_status (driver, FILTER_STATUS_ACTION, "Delete");
	
	return NULL;
}

static ESExpResult *
mark_forward (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	/*struct _FilterDriverPrivate *p = _PRIVATE (driver);*/
	
	d(fprintf (stderr, "marking message for forwarding\n"));
	/* FIXME: do stuff here */
	report_status (driver, FILTER_STATUS_ACTION, "Forward");
	
	return NULL;
}

static ESExpResult *
do_copy (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	int i;
	
	d(fprintf (stderr, "copying message...\n"));
	p->copied = TRUE;
	for (i = 0; i < argc; i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			/* open folders we intent to copy to */
			char *folder = argv[i]->value.string;
			CamelFolder *outbox;
			
			outbox = open_folder (driver, folder);
			if (!outbox)
				continue;
			
			mail_tool_camel_lock_up ();
			camel_folder_append_message (outbox, p->message, p->info, p->ex);
			report_status (driver, FILTER_STATUS_ACTION, "Copy to folder %s", outbox->full_name);
			mail_tool_camel_lock_down ();
		}
	}
	
	return NULL;
}

static ESExpResult *
do_move (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	int i;
	
	d(fprintf (stderr, "moving message...\n"));
	p->copied = TRUE;
	p->deleted = TRUE;  /* a 'move' is a copy & delete */
	p->info->flags = p->info->flags | CAMEL_MESSAGE_DELETED;
	for (i = 0; i < argc; i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			/* open folders we intent to move to */
			char *folder = argv[i]->value.string;
			CamelFolder *outbox;
			
			outbox = open_folder (driver, folder);
			if (!outbox)
				continue;
			
			mail_tool_camel_lock_up ();
			camel_folder_append_message (outbox, p->message, p->info, p->ex);
			report_status (driver, FILTER_STATUS_ACTION, "Move to folder %s", outbox->full_name);
			mail_tool_camel_lock_down ();
		}
	}
	
	return NULL;
}

static ESExpResult *
do_stop (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	
	report_status (driver, FILTER_STATUS_ACTION, "Stopped processing");
	d(fprintf (stderr, "terminating message processing\n"));
	p->terminated = TRUE;
	
	return NULL;
}

static ESExpResult *
do_colour (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	
	d(fprintf (stderr, "setting colour tag\n"));
	if (argc > 0 && argv[0]->type == ESEXP_RES_STRING) {
		camel_tag_set (&p->info->user_tags, "colour", argv[0]->value.string);
		report_status (driver, FILTER_STATUS_ACTION, "Set colour to %s", argv[0]->value.string);
	}
	
	return NULL;
}

static ESExpResult *
do_score (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterDriver *driver)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	
	d(fprintf (stderr, "setting score tag\n"));
	if (argc > 0 && argv[0]->type == ESEXP_RES_INT) {
		char *value;
		
		value = g_strdup_printf ("%d", argv[0]->value.number);
		camel_tag_set (&p->info->user_tags, "score", value);
		report_status (driver, FILTER_STATUS_ACTION, "Set score to %d", argv[0]->value.number);
		g_free (value);
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
	camel_object_unref (CAMEL_OBJECT (folder));
	mail_tool_camel_lock_down ();
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


void
filter_driver_status_log (FilterDriver *driver, enum filter_status_t status,
			  const char *desc, CamelMimeMessage *msg, void *data)
{
	FILE *out = data;
	
	switch(status) {
	case FILTER_STATUS_END: {
		/* write log header */
		time_t t;
		char date[50];
		
		time (&t);
		strftime (date, 49, "%a, %d %b %Y %H:%M:%S", localtime (&t));
		fprintf (out, " - Applied filter \"%s\" to message from %s - \"%s\" at %s\n",
			 desc, msg ? camel_mime_message_get_from (msg) : "unknown",
			 msg ? camel_mime_message_get_subject (msg) : "", date);
		break;
	}
	case FILTER_STATUS_START:
		fprintf (out, "\n");
		break;
	case FILTER_STATUS_ACTION:
		fprintf (out, "Action: %s\n", desc);
		break;
	default:
		/* nothing else is loggable */
		break;
	}
}


/* will filter only an mbox - is more efficient as it doesn't need to open the folder through camel directly */
void
filter_driver_filter_mbox (FilterDriver *driver, const char *mbox, const char *source, CamelException *ex)
{
	CamelMimeParser *mp = NULL;
	int fd = -1;
	int i = 0;
	struct stat st;
	
	fd = open (mbox, O_RDONLY);
	if (fd == -1) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, "Unable to open spool folder");
		goto fail;
	}
	/* to get the filesize */
	fstat (fd, &st);
	
	mp = camel_mime_parser_new ();
	camel_mime_parser_scan_from (mp, TRUE);
	if (camel_mime_parser_init_with_fd (mp, fd) == -1) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, "Unable to process spool folder");
		goto fail;
	}
	fd = -1;
	while (camel_mime_parser_step (mp, 0, 0) == HSCAN_FROM) {
		CamelMimeMessage *msg;
		int pc;
		
		pc = camel_mime_parser_tell (mp) * 100 / st.st_size;
		report_status (driver, FILTER_STATUS_START, "Getting message %d (%d%% of file)", i, pc);
		
		msg = camel_mime_message_new ();
		if (camel_mime_part_construct_from_parser (CAMEL_MIME_PART (msg), mp) == -1) {
			report_status (driver, FILTER_STATUS_END, "Failed message %d", i);
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, "Cannot open message");
			camel_object_unref (CAMEL_OBJECT (msg));
			goto fail;
		}
		
		filter_driver_filter_message (driver, msg, NULL, source, ex);
		camel_object_unref (CAMEL_OBJECT (msg));
		if (camel_exception_is_set (ex)) {
			report_status (driver, FILTER_STATUS_END, "Failed message %d", i);
			goto fail;
		}
		
		report_status (driver, FILTER_STATUS_END, "Finished message %d", i);
		i++;
		
		/* skip over the FROM_END state */
		camel_mime_parser_step (mp, 0, 0);
	}
fail:
	if (fd != -1)
		close (fd);
	if (mp)
		camel_object_unref (CAMEL_OBJECT (mp));
}

/* will filter a folder */
void
filter_driver_filter_folder (FilterDriver *driver, CamelFolder *folder, const char *source,
			     GPtrArray *uids, gboolean remove, CamelException *ex)
{
	int i;
	int freeuids = FALSE;
	CamelMimeMessage *message;
	const CamelMessageInfo *info;
	
	if (uids == NULL) {
		uids = camel_folder_get_uids (folder);
		freeuids = TRUE;
	}
	
	for (i = 0; i < uids->len; i++) {
		report_status (driver, FILTER_STATUS_START, "Getting message %d of %d", i+1, uids->len);
		
		message = camel_folder_get_message (folder, uids->pdata[i], ex);
		if (camel_exception_is_set (ex)) {
			report_status (driver, FILTER_STATUS_END, "Failed at message %d of %d", i+1, uids->len);
			break;
		}
		
		if (camel_folder_has_summary_capability (folder))
			info = camel_folder_get_message_info (folder, uids->pdata[i]);
		else
			info = NULL;
		
		filter_driver_filter_message (driver, message, (CamelMessageInfo *)info, source, ex);
		if (camel_exception_is_set (ex)) {
			report_status (driver, FILTER_STATUS_END, "Failed at message %d of %d", i+1, uids->len);
			break;
		}
		
		if (remove)
			camel_folder_set_message_flags (folder, uids->pdata[i],
							CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);
		
		camel_object_unref (CAMEL_OBJECT (message));
	}
	
	if (freeuids)
		camel_folder_free_uids (folder, uids);
}

void
filter_driver_filter_message (FilterDriver *driver, CamelMimeMessage *message, CamelMessageInfo *info,
			      const char *source, CamelException *ex)
{
	struct _FilterDriverPrivate *p = _PRIVATE (driver);
	ESExpResult *r;
	GString *fsearch, *faction;
	FilterFilter *rule;
	int freeinfo = FALSE;
	
	if (info == NULL) {
		struct _header_raw *h = CAMEL_MIME_PART (message)->headers;
		
		info = g_new0 (CamelMessageInfo, 1);
		freeinfo = TRUE;
		info->subject = camel_folder_summary_format_string (h, "subject");
		info->from = camel_folder_summary_format_address (h, "from");
		info->to = camel_folder_summary_format_address (h, "to");
		info->cc = camel_folder_summary_format_address (h, "cc");
	} else {
		if (info->flags & CAMEL_MESSAGE_DELETED)
			return;
	}
	
	p->ex = ex;
	p->terminated = FALSE;
	p->deleted = FALSE;
	p->copied = FALSE;
	p->message = message;
	p->info = info;
	
	fsearch = g_string_new ("");
	faction = g_string_new ("");
	
	rule = NULL;
	while ((rule = (FilterFilter *)rule_context_next_rule ((RuleContext *)p->context, (FilterRule *)rule, source))) {
		gboolean matched;
		
		g_string_truncate (fsearch, 0);
		g_string_truncate (faction, 0);
		
		filter_rule_build_code (FILTER_RULE (rule), fsearch);
		filter_filter_build_action (rule, faction);
		
		d(fprintf (stderr, "applying rule %s\n action %s\n", fsearch->str, faction->str));
		
		mail_tool_camel_lock_up ();
		matched = filter_message_search (p->message, p->info, source, fsearch->str, p->ex);
		mail_tool_camel_lock_down ();
		
		if (matched) {
#ifndef NO_WARNINGS
#warning "Must check expression parsed and executed properly?"
#endif
			/* perform necessary filtering actions */
			e_sexp_input_text (p->eval, faction->str, strlen (faction->str));
			e_sexp_parse (p->eval);
			r = e_sexp_eval (p->eval);
			e_sexp_result_free (r);
			if (p->terminated)
				break;
		}
	}
	
	g_string_free (fsearch, TRUE);
	g_string_free (faction, TRUE);
	
	if (!p->deleted && !p->copied && p->defaultfolder) {
		/* copy it to the default inbox */
		report_status (driver, FILTER_STATUS_ACTION, "Copy to default folder");
		mail_tool_camel_lock_up ();
		camel_folder_append_message (p->defaultfolder, p->message, p->info, p->ex);
		mail_tool_camel_lock_down ();
	}
	
	if (freeinfo) {
		camel_flag_list_free (&info->user_flags);
		camel_tag_list_free (&info->user_tags);
		g_free (info->subject);
		g_free (info->from);
		g_free (info->to);
		g_free (info->cc);
		g_free (info);
	}
}
