/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Helix Code Inc.
 *  Copyright (C) 2001 Ximian Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>

#include <glib.h>

#include "camel-filter-driver.h"
#include "camel-filter-search.h"

#include "camel-exception.h"
#include "camel-service.h"
#include "camel-mime-message.h"

#include "e-util/e-sexp.h"
#include "e-util/e-memory.h"
#include "e-util/e-msgport.h"	/* for edlist */

#define d(x)

/* type of status for a log report */
enum filter_log_t {
	FILTER_LOG_NONE,
	FILTER_LOG_START,       /* start of new log entry */
	FILTER_LOG_ACTION,      /* an action performed */
	FILTER_LOG_END,	        /* end of log */
};

/* list of rule nodes */
struct _filter_rule {
	struct _filter_rule *next;
	struct _filter_rule *prev;

	char *match;
	char *action;
	char *name;
};

struct _CamelFilterDriverPrivate {
	GHashTable *globals;       /* global variables */

	CamelFolder *defaultfolder;	/* defualt folder */
	
	CamelFilterStatusFunc *statusfunc; 	/* status callback */
	void *statusdata;		/* status callback data */
	
	/* for callback */
	CamelFilterGetFolderFunc get_folder;
	void *data;
	
	/* run-time data */
	GHashTable *folders;       /* folders that message has been copied to */
	int closed;		   /* close count */
	GHashTable *forwards;      /* addresses that have been forwarded the message */
	
	gboolean terminated;       /* message processing was terminated */
	gboolean deleted;          /* message was marked for deletion */
	gboolean copied;           /* message was copied to some folder or another */
	
	CamelMimeMessage *message; /* input message */
	CamelMessageInfo *info;    /* message summary info */
	const char *uid;           /* message uid */
	CamelFolder *source;       /* message source folder */
	
	FILE *logfile;             /* log file */
	
	EDList rules;		   /* list of _filter_rule structs */

	CamelException *ex;
	
	/* evaluator */
	ESExp *eval;
};

#define _PRIVATE(o) (((CamelFilterDriver *)(o))->priv)

static void camel_filter_driver_class_init (CamelFilterDriverClass *klass);
static void camel_filter_driver_init       (CamelFilterDriver *obj);
static void camel_filter_driver_finalise   (CamelObject *obj);

static void camel_filter_driver_log (CamelFilterDriver *driver, enum filter_log_t status, const char *desc, ...);

static CamelFolder *open_folder (CamelFilterDriver *d, const char *folder_url);
static int close_folders (CamelFilterDriver *d);

static ESExpResult *do_delete (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);
static ESExpResult *mark_forward (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);
static ESExpResult *do_copy (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);
static ESExpResult *do_move (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);
static ESExpResult *do_stop (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);
static ESExpResult *do_colour (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);
static ESExpResult *do_score (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);
static ESExpResult *do_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);

/* these are our filter actions - each must have a callback */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "delete",          (ESExpFunc *) do_delete,    0 },
	{ "forward-to",      (ESExpFunc *) mark_forward, 0 },
	{ "copy-to",         (ESExpFunc *) do_copy,      0 },
	{ "move-to",         (ESExpFunc *) do_move,      0 },
	{ "stop",            (ESExpFunc *) do_stop,      0 },
	{ "set-colour",      (ESExpFunc *) do_colour,    0 },
	{ "set-score",       (ESExpFunc *) do_score,     0 },
	{ "set-system-flag", (ESExpFunc *) do_flag,      0 }
};

static CamelObjectClass *camel_filter_driver_parent;

guint
camel_filter_driver_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;

	if (type == CAMEL_INVALID_TYPE)	{
		type = camel_type_register(CAMEL_OBJECT_TYPE, "CamelFilterDriver",
					   sizeof(CamelFilterDriver),
					   sizeof(CamelFilterDriverClass),
					   (CamelObjectClassInitFunc)camel_filter_driver_class_init,
					   NULL,
					   (CamelObjectInitFunc)camel_filter_driver_init,
					   (CamelObjectFinalizeFunc)camel_filter_driver_finalise);
	}
	
	return type;
}

static void
camel_filter_driver_class_init (CamelFilterDriverClass *klass)
{
	/*CamelObjectClass *object_class = (CamelObjectClass *) klass;*/

	camel_filter_driver_parent = camel_type_get_global_classfuncs(camel_object_get_type());
}

static void
camel_filter_driver_init (CamelFilterDriver *obj)
{
	struct _CamelFilterDriverPrivate *p;
	int i;
	
	p = _PRIVATE (obj) = g_malloc0 (sizeof (*p));

	e_dlist_init(&p->rules);

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
camel_filter_driver_finalise (CamelObject *obj)
{
	CamelFilterDriver *driver = (CamelFilterDriver *) obj;
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);	
	struct _filter_rule *node;

	/* close all folders that were opened for appending */
	close_folders (driver);
	g_hash_table_destroy (p->folders);
	
	g_hash_table_foreach (p->globals, free_hash_strings, driver);
	g_hash_table_destroy (p->globals);

	e_sexp_unref(p->eval);
	
	if (p->defaultfolder) {
		camel_folder_thaw (p->defaultfolder);
		camel_object_unref (CAMEL_OBJECT (p->defaultfolder));
	}

	while ((node = (struct _filter_rule *)e_dlist_remhead(&p->rules))) {
		g_free(node->match);
		g_free(node->action);
		g_free(node->name);
		g_free(node);
	}
	
	g_free (p);
}

/**
 * camel_filter_driver_new:
 * @system: path to system rules
 * @user: path to user rules
 * @get_folder: function to call to fetch folders
 *
 * Create a new CamelFilterDriver object.
 * 
 * Return value: A new CamelFilterDriver widget.
 **/
CamelFilterDriver *
camel_filter_driver_new (CamelFilterGetFolderFunc get_folder, void *data)
{
	CamelFilterDriver *new;
	struct _CamelFilterDriverPrivate *p;
	
	new = CAMEL_FILTER_DRIVER (camel_object_new(camel_filter_driver_get_type ()));
	p = _PRIVATE (new);
	
	p->get_folder = get_folder;
	p->data = data;
	
	return new;
}

void
camel_filter_driver_set_logfile (CamelFilterDriver *d, FILE *logfile)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (d);
	
	p->logfile = logfile;
}

void
camel_filter_driver_set_status_func (CamelFilterDriver *d, CamelFilterStatusFunc *func, void *data)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (d);
	
	p->statusfunc = func;
	p->statusdata = data;
}

void
camel_filter_driver_set_default_folder (CamelFilterDriver *d, CamelFolder *def)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (d);
	
	if (p->defaultfolder) {
		camel_folder_thaw (p->defaultfolder);
		camel_object_unref (CAMEL_OBJECT (p->defaultfolder));
	}
	
	p->defaultfolder = def;
	
	if (p->defaultfolder) {
		camel_folder_freeze (p->defaultfolder);
		camel_object_ref (CAMEL_OBJECT (p->defaultfolder));
	}
}

void
camel_filter_driver_add_rule(CamelFilterDriver *d, const char *name, const char *match, const char *action)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (d);
	struct _filter_rule *node;

	node = g_malloc(sizeof(*node));
	node->match = g_strdup(match);
	node->action = g_strdup(action);
	node->name = g_strdup(name);
	e_dlist_addtail(&p->rules, (EDListNode *)node);
}

static void
report_status (CamelFilterDriver *driver, enum camel_filter_status_t status, int pc, const char *desc, ...)
{
	/* call user-defined status report function */
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	va_list ap;
	char *str;
	
	if (p->statusfunc) {
		va_start (ap, desc);
		str = g_strdup_vprintf (desc, ap);
		p->statusfunc (driver, status, pc, str, p->statusdata);
		g_free (str);
	}
}


#if 0
void
camel_filter_driver_set_global (CamelFilterDriver *d, const char *name, const char *value)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (d);
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
do_delete (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	
	d(fprintf (stderr, "doing delete\n"));
	p->deleted = TRUE;
	camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Delete");
	
	return NULL;
}

static ESExpResult *
mark_forward (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	/*struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);*/
	
	d(fprintf (stderr, "marking message for forwarding\n"));
	/* FIXME: do stuff here */
	camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Forward");
	
	return NULL;
}

static ESExpResult *
do_copy (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	int i;
	
	d(fprintf (stderr, "copying message...\n"));
	
	for (i = 0; i < argc; i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			/* open folders we intent to copy to */
			char *folder = argv[i]->value.string;
			char *service_url;
			CamelFolder *outbox;
			
			outbox = open_folder (driver, folder);
			if (!outbox)
				break;
			
			p->copied = TRUE;
			if (p->uid && p->source && camel_folder_has_summary_capability (p->source)) {
				GPtrArray *uids;
				
				uids = g_ptr_array_new ();
				g_ptr_array_add (uids, (char *) p->uid);
				camel_folder_copy_messages_to (p->source, uids, outbox, p->ex);
				g_ptr_array_free (uids, TRUE);
			} else
				camel_folder_append_message (outbox, p->message, p->info, p->ex);
			
			service_url = camel_service_get_url (CAMEL_SERVICE (camel_folder_get_parent_store (outbox)));
			camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Copy to folder %s",
						 service_url);
			g_free (service_url);
		}
	}
	
	return NULL;
}

static ESExpResult *
do_move (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	int i;
	
	d(fprintf (stderr, "moving message...\n"));
	
	for (i = 0; i < argc; i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			/* open folders we intent to move to */
			char *folder = argv[i]->value.string;
			char *service_url;
			CamelFolder *outbox;
			
			outbox = open_folder (driver, folder);
			if (!outbox)
				break;
			
			p->copied = TRUE;
			p->deleted = TRUE;  /* a 'move' is a copy & delete */
			
			if (p->uid && p->source && camel_folder_has_summary_capability (p->source)) {
				GPtrArray *uids;
				
				uids = g_ptr_array_new ();
				g_ptr_array_add (uids, (char *) p->uid);
				camel_folder_copy_messages_to (p->source, uids, outbox, p->ex);
				g_ptr_array_free (uids, TRUE);
			} else
				camel_folder_append_message (outbox, p->message, p->info, p->ex);
			
			service_url = camel_service_get_url (CAMEL_SERVICE (camel_folder_get_parent_store (outbox)));
			camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Move to folder %s",
						 service_url);
			g_free (service_url);
		}
	}
	
	return NULL;
}

static ESExpResult *
do_stop (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	
	camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Stopped processing");
	d(fprintf (stderr, "terminating message processing\n"));
	p->terminated = TRUE;
	
	return NULL;
}

static ESExpResult *
do_colour (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	
	d(fprintf (stderr, "setting colour tag\n"));
	if (argc > 0 && argv[0]->type == ESEXP_RES_STRING) {
		camel_tag_set (&p->info->user_tags, "colour", argv[0]->value.string);
		camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Set colour to %s", argv[0]->value.string);
	}
	
	return NULL;
}

static ESExpResult *
do_score (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	
	d(fprintf (stderr, "setting score tag\n"));
	if (argc > 0 && argv[0]->type == ESEXP_RES_INT) {
		char *value;
		
		value = g_strdup_printf ("%d", argv[0]->value.number);
		camel_tag_set (&p->info->user_tags, "score", value);
		camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Set score to %d", argv[0]->value.number);
		g_free (value);
	}
	
	return NULL;
}

static ESExpResult *
do_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	
	d(fprintf (stderr, "setting flag\n"));
	if (argc == 1 && argv[0]->type == ESEXP_RES_STRING) {
		p->info->flags |= camel_system_flag (argv[0]->value.string) | CAMEL_MESSAGE_FOLDER_FLAGGED;
		camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Set %s flag", argv[0]->value.string);
	}
	
	return NULL;
}

static CamelFolder *
open_folder (CamelFilterDriver *driver, const char *folder_url)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	CamelFolder *camelfolder;
	
	/* we have a lookup table of currently open folders */
	camelfolder = g_hash_table_lookup (p->folders, folder_url);
	if (camelfolder)
		return camelfolder;
	
	camelfolder = p->get_folder (driver, folder_url, p->data, p->ex);
	
	if (camelfolder) {
		g_hash_table_insert (p->folders, g_strdup (folder_url), camelfolder);
		camel_folder_freeze (camelfolder);
	}
	
	return camelfolder;
}

static void
close_folder (void *key, void *value, void *data)
{	
	CamelFolder *folder = value;
	CamelFilterDriver *driver = data;
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	
	p->closed++;

	g_free (key);
	camel_folder_sync (folder, FALSE, p->ex);
	camel_folder_thaw (folder);
	camel_object_unref (CAMEL_OBJECT (folder));

	report_status(driver, CAMEL_FILTER_STATUS_PROGRESS, g_hash_table_size(p->folders)* 100 / p->closed, "Syncing folders");
}

/* flush/close all folders */
static int
close_folders (CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);

	report_status(driver, CAMEL_FILTER_STATUS_PROGRESS, 0, "Syncing folders");

	p->closed = 0;
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


static void
camel_filter_driver_log (CamelFilterDriver *driver, enum filter_log_t status, const char *desc, ...)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	
	if (p->logfile) {
		char *str = NULL;
		
		if (desc) {
			va_list ap;
			
			va_start (ap, desc);
			str = g_strdup_vprintf (desc, ap);
		}
		
		switch (status) {
		case FILTER_LOG_START: {
			/* write log header */
			const char *subject = NULL;
			char *fromstr;
			const CamelInternetAddress *from;
			char date[50];
			time_t t;

			/* FIXME: does this need locking?  Probably */

			from = camel_mime_message_get_from (p->message);
			fromstr = camel_address_format((CamelAddress *)from);
			subject = camel_mime_message_get_subject (p->message);
			
			time (&t);
			strftime (date, 49, "%a, %d %b %Y %H:%M:%S", localtime (&t));
			fprintf (p->logfile, "Applied filter \"%s\" to message from %s - \"%s\" at %s\n",
				 str, fromstr ? fromstr : "unknown", subject ? subject : "", date);
			g_free(fromstr);
			break;
		}
		case FILTER_LOG_ACTION:
			fprintf (p->logfile, "Action: %s\n", str);
			break;
		case FILTER_LOG_END:
			fprintf (p->logfile, "\n");
			break;
		default:
			/* nothing else is loggable */
			break;
		}
		
		g_free (str);
	}
}


/**
 * camel_filter_driver_filter_mbox:
 * @driver: CamelFilterDriver
 * @mbox: mbox filename to be filtered
 * @ex: exception
 *
 * Filters an mbox file based on rules defined in the FilterDriver
 * object. Is more efficient as it doesn't need to open the folder
 * through Camel directly.
 *
 * Returns -1 if errors were encountered during filtering,
 * otherwise returns 0.
 *
 **/
int
camel_filter_driver_filter_mbox (CamelFilterDriver *driver, const char *mbox, CamelException *ex)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	CamelMimeParser *mp = NULL;
	char *source_url = NULL;
	int fd = -1;
	int i = 0;
	struct stat st;
	int status;
	
	fd = open (mbox, O_RDONLY);
	if (fd == -1) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Unable to open spool folder"));
		goto fail;
	}
	/* to get the filesize */
	fstat (fd, &st);
	
	mp = camel_mime_parser_new ();
	camel_mime_parser_scan_from (mp, TRUE);
	if (camel_mime_parser_init_with_fd (mp, fd) == -1) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Unable to process spool folder"));
		goto fail;
	}
	fd = -1;
	
	source_url = g_strdup_printf ("file://%s", mbox);
	
	while (camel_mime_parser_step (mp, 0, 0) == HSCAN_FROM) {
		CamelMimeMessage *msg;
		int pc = 0;
		
		if (st.st_size > 0)
			pc = (int)(100.0 * ((double)camel_mime_parser_tell (mp) / (double)st.st_size));
		
		report_status (driver, CAMEL_FILTER_STATUS_START, pc, _("Getting message %d (%d%%)"), i, pc);
		
		msg = camel_mime_message_new ();
		if (camel_mime_part_construct_from_parser (CAMEL_MIME_PART (msg), mp) == -1) {
			report_status (driver, CAMEL_FILTER_STATUS_END, 100, _("Failed message %d"), i);
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot open message"));
			camel_object_unref (CAMEL_OBJECT (msg));
			goto fail;
		}
		
		status = camel_filter_driver_filter_message (driver, msg, NULL, NULL, NULL, source_url, ex);
		camel_object_unref (CAMEL_OBJECT (msg));
		if (camel_exception_is_set (ex) || status == -1) {
			report_status (driver, CAMEL_FILTER_STATUS_END, 100, _("Failed message %d"), i);
			goto fail;
		}
		
		i++;
		
		/* skip over the FROM_END state */
		camel_mime_parser_step (mp, 0, 0);
	}

	if (p->defaultfolder) {
		report_status(driver, CAMEL_FILTER_STATUS_PROGRESS, 100, _("Syncing folder"));
		camel_folder_sync(p->defaultfolder, FALSE, ex);
	}

	report_status (driver, CAMEL_FILTER_STATUS_END, 100, _("Complete"));
	
	return 0;
	
fail:
	g_free (source_url);
	if (fd != -1)
		close (fd);
	if (mp)
		camel_object_unref (CAMEL_OBJECT (mp));
	
	return -1;
}


/**
 * camel_filter_driver_filter_folder:
 * @driver: CamelFilterDriver
 * @folder: CamelFolder to be filtered
 * @uids: message uids to be filtered or NULL (as a shortcut to filter all messages)
 * @remove: TRUE to mark filtered messages as deleted
 * @ex: exception
 *
 * Filters a folder based on rules defined in the FilterDriver
 * object.
 *
 * Returns -1 if errors were encountered during filtering,
 * otherwise returns 0.
 *
 **/
int
camel_filter_driver_filter_folder (CamelFilterDriver *driver, CamelFolder *folder,
				   GPtrArray *uids, gboolean remove, CamelException *ex)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	int i;
	int freeuids = FALSE;
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	char *source_url, *service_url;
	int status = 0;
	
	service_url = camel_service_get_url (CAMEL_SERVICE (camel_folder_get_parent_store (folder)));
	source_url = g_strdup_printf ("%s%s", service_url, camel_folder_get_full_name (folder));
	g_free (service_url);
	
	if (uids == NULL) {
		uids = camel_folder_get_uids (folder);
		freeuids = TRUE;
	}
	
	for (i = 0; i < uids->len; i++) {
		int pc = (100 * i)/uids->len;

		report_status (driver, CAMEL_FILTER_STATUS_START, pc, "Getting message %d of %d", i+1,
			       uids->len);
		
		message = camel_folder_get_message (folder, uids->pdata[i], ex);
		if (!message || camel_exception_is_set (ex)) {
			report_status (driver, CAMEL_FILTER_STATUS_END, 100, "Failed at message %d of %d",
				       i+1, uids->len);
			status = -1;
			break;
		}
		
		if (camel_folder_has_summary_capability (folder))
			info = camel_folder_get_message_info (folder, uids->pdata[i]);
		else
			info = NULL;
		
		status = camel_filter_driver_filter_message (driver, message, info, uids->pdata[i],
							     folder, source_url, ex);
		
		if (camel_folder_has_summary_capability (folder))
			camel_folder_free_message_info (folder, info);

		if (camel_exception_is_set (ex) || status == -1) {
			report_status (driver, CAMEL_FILTER_STATUS_END, 100, "Failed at message %d of %d",
				       i+1, uids->len);
			status = -1;
			break;
		}
		
		if (remove)
			camel_folder_set_message_flags (folder, uids->pdata[i],
							CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);
		
		camel_object_unref (CAMEL_OBJECT (message));
	}

	if (freeuids)
		camel_folder_free_uids (folder, uids);
	
	if (p->defaultfolder) {
		report_status(driver, CAMEL_FILTER_STATUS_PROGRESS, 100, "Syncing folder");
		camel_folder_sync (p->defaultfolder, FALSE, ex);
	}

	if (i == uids->len)
		report_status (driver, CAMEL_FILTER_STATUS_END, 100, "Complete");
	
	g_free (source_url);
	
	return status;
}


/**
 * camel_filter_driver_filter_message:
 * @driver: CamelFilterDriver
 * @message: message to filter
 * @info: message info or NULL
 * @uid: message uid or NULL
 * @source: source folder or NULL
 * @source_url: url of source folder or NULL
 * @ex: exception
 *
 * Filters a message based on rules defined in the FilterDriver
 * object. If the source folder (@source) and the uid (@uid) are
 * provided, the filter will operate on the CamelFolder (which in
 * certain cases is more efficient than using the default
 * camel_folder_append_message() function).
 *
 * Returns -1 if errors were encountered during filtering,
 * otherwise returns 0.
 *
 **/
int
camel_filter_driver_filter_message (CamelFilterDriver *driver, CamelMimeMessage *message,
				    CamelMessageInfo *info, const char *uid,
				    CamelFolder *source, const char *source_url,
				    CamelException *ex)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	ESExpResult *r;
	struct _filter_rule *node;
	gboolean freeinfo = FALSE;
	gboolean filtered = FALSE;

	if (info == NULL) {
		struct _header_raw *h = CAMEL_MIME_PART (message)->headers;
		
		info = camel_message_info_new_from_header (h);
		freeinfo = TRUE;
	} else {
		if (info->flags & CAMEL_MESSAGE_DELETED)
			return 0;
	}
	
	p->ex = ex;
	p->terminated = FALSE;
	p->deleted = FALSE;
	p->copied = FALSE;
	p->message = message;
	p->info = info;
	p->uid = uid;
	p->source = source;
	
	if (camel_mime_message_get_source (message) == NULL)
		camel_mime_message_set_source (message, source_url);
	
	node = (struct _filter_rule *)p->rules.head;
	while (node->next) {
		d(fprintf (stderr, "applying rule %s\n action %s\n", node->match, node->action));
		
		if (camel_filter_search_match(p->message, p->info, source_url, node->match, p->ex)) {
			filtered = TRUE;
			camel_filter_driver_log (driver, FILTER_LOG_START, node->name);
			
			/* perform necessary filtering actions */
			e_sexp_input_text (p->eval, node->action, strlen (node->action));
			if (e_sexp_parse (p->eval) == -1) {
				camel_exception_setv(ex, 1, _("Error parsing filter: %s: %s"), e_sexp_error(p->eval), node->action);
				goto error;
			}
			r = e_sexp_eval (p->eval);
			if (r == NULL) {
				camel_exception_setv(ex, 1, _("Error executing filter: %s: %s"), e_sexp_error(p->eval), node->action);
				goto error;
			}
			e_sexp_result_free (p->eval, r);
			if (p->terminated)
				break;
		}
		node = node->next;
	}
	
	/* *Now* we can set the DELETED flag... */
	if (p->deleted)
		info->flags = info->flags | CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_FOLDER_FLAGGED;
	
	/* Logic: if !Moved and there exists a default folder... */
	if (!(p->copied && p->deleted) && p->defaultfolder) {
		/* copy it to the default inbox */
		filtered = TRUE;
		camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Copy to default folder");
		if (p->uid && p->source && camel_folder_has_summary_capability (p->source)) {
			GPtrArray *uids;
				
			uids = g_ptr_array_new ();
			g_ptr_array_add (uids, (char *) p->uid);
			camel_folder_copy_messages_to (p->source, uids, p->defaultfolder, p->ex);
			g_ptr_array_free (uids, TRUE);
		} else
			camel_folder_append_message (p->defaultfolder, p->message, p->info, p->ex);
	}
	
	return 0;
	
error:	
	if (filtered)
		camel_filter_driver_log (driver, FILTER_LOG_END, NULL);
	
	if (freeinfo)
		camel_message_info_free (info);
	
	return -1;
}
