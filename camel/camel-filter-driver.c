/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include <string.h>
#include <time.h>

#include <glib.h>

#include "camel-filter-driver.h"
#include "camel-filter-search.h"

#include "camel-service.h"
#include "camel-stream-fs.h"
#include "camel-stream-mem.h"
#include "camel-mime-message.h"
#include "camel-debug.h"
#include "camel-i18n.h"

#include "libedataserver/e-sexp.h"
#include "libedataserver/e-memory.h"
#include "libedataserver/e-msgport.h"	/* for edlist */

#define d(x)

/* an invalid pointer */
#define FOLDER_INVALID ((void *)~0)

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

	CamelSession *session;

	CamelFolder *defaultfolder;        /* defualt folder */
	
	CamelFilterStatusFunc *statusfunc; /* status callback */
	void *statusdata;                  /* status callback data */
	
	CamelFilterShellFunc *shellfunc;    /* execute shell command callback */
	void *shelldata;                    /* execute shell command callback data */
	
	CamelFilterPlaySoundFunc *playfunc; /* play-sound command callback */
	void *playdata;                     /* play-sound command callback data */
	
	CamelFilterSystemBeepFunc *beep;    /* system beep callback */
	void *beepdata;                     /* system beep callback data */
	
	/* for callback */
	CamelFilterGetFolderFunc get_folder;
	void *data;
	
	/* run-time data */
	GHashTable *folders;       /* folders that message has been copied to */
	int closed;		   /* close count */
	GHashTable *forwards;      /* addresses that have been forwarded the message */
	GHashTable *only_once;     /* actions to run only-once */
	
	gboolean terminated;       /* message processing was terminated */
	gboolean deleted;          /* message was marked for deletion */
	gboolean copied;           /* message was copied to some folder or another */
	
	CamelMimeMessage *message; /* input message */
	CamelMessageInfo *info;    /* message summary info */
	const char *uid;           /* message uid */
	CamelFolder *source;       /* message source folder */
	gboolean modified;         /* has the input message been modified? */
	
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
static ESExpResult *set_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);
static ESExpResult *unset_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);
static ESExpResult *do_shell (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);
static ESExpResult *do_beep (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);
static ESExpResult *play_sound (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);
static ESExpResult *do_only_once (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);
static ESExpResult *pipe_message (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *);

/* these are our filter actions - each must have a callback */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "delete",            (ESExpFunc *) do_delete,    0 },
	{ "forward-to",        (ESExpFunc *) mark_forward, 0 },
	{ "copy-to",           (ESExpFunc *) do_copy,      0 },
	{ "move-to",           (ESExpFunc *) do_move,      0 },
	{ "stop",              (ESExpFunc *) do_stop,      0 },
	{ "set-colour",        (ESExpFunc *) do_colour,    0 },
	{ "set-score",         (ESExpFunc *) do_score,     0 },
	{ "set-system-flag",   (ESExpFunc *) set_flag,     0 },
	{ "unset-system-flag", (ESExpFunc *) unset_flag,   0 },
	{ "pipe-message",      (ESExpFunc *) pipe_message, 0 },
	{ "shell",             (ESExpFunc *) do_shell,     0 },
	{ "beep",              (ESExpFunc *) do_beep,      0 },
	{ "play-sound",        (ESExpFunc *) play_sound,   0 },
	{ "only-once",         (ESExpFunc *) do_only_once, 0 }
};

static CamelObjectClass *camel_filter_driver_parent;

CamelType
camel_filter_driver_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;

	if (type == CAMEL_INVALID_TYPE)	{
		type = camel_type_register (CAMEL_OBJECT_TYPE,
					    "CamelFilterDriver",
					    sizeof (CamelFilterDriver),
					    sizeof (CamelFilterDriverClass),
					    (CamelObjectClassInitFunc) camel_filter_driver_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_filter_driver_init,
					    (CamelObjectFinalizeFunc) camel_filter_driver_finalise);
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
	
	p->only_once = g_hash_table_new (g_str_hash, g_str_equal);
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
	
	g_hash_table_foreach (p->only_once, free_hash_strings, driver);
	g_hash_table_destroy (p->only_once);
	
	e_sexp_unref(p->eval);
	
	if (p->defaultfolder) {
		camel_folder_thaw (p->defaultfolder);
		camel_object_unref (p->defaultfolder);
	}

	while ((node = (struct _filter_rule *)e_dlist_remhead(&p->rules))) {
		g_free(node->match);
		g_free(node->action);
		g_free(node->name);
		g_free(node);
	}

	camel_object_unref(p->session);

	g_free (p);
}

/**
 * camel_filter_driver_new:
 *
 * Return value: A new CamelFilterDriver object
 **/
CamelFilterDriver *
camel_filter_driver_new (CamelSession *session)
{
	CamelFilterDriver *d = (CamelFilterDriver *)camel_object_new(camel_filter_driver_get_type());

	d->priv->session = session;
	camel_object_ref((CamelObject *)session);

	return d;
}

void
camel_filter_driver_set_folder_func (CamelFilterDriver *d, CamelFilterGetFolderFunc get_folder, void *data)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (d);

	p->get_folder = get_folder;
	p->data = data;
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
camel_filter_driver_set_shell_func (CamelFilterDriver *d, CamelFilterShellFunc *func, void *data)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (d);
	
	p->shellfunc = func;
	p->shelldata = data;
}

void
camel_filter_driver_set_play_sound_func (CamelFilterDriver *d, CamelFilterPlaySoundFunc *func, void *data)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (d);
	
	p->playfunc = func;
	p->playdata = data;
}

void
camel_filter_driver_set_system_beep_func (CamelFilterDriver *d, CamelFilterSystemBeepFunc *func, void *data)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (d);
	
	p->beep = func;
	p->beepdata = data;
}

void
camel_filter_driver_set_default_folder (CamelFilterDriver *d, CamelFolder *def)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (d);
	
	if (p->defaultfolder) {
		camel_folder_thaw (p->defaultfolder);
		camel_object_unref (p->defaultfolder);
	}
	
	p->defaultfolder = def;
	
	if (p->defaultfolder) {
		camel_folder_freeze (p->defaultfolder);
		camel_object_ref (p->defaultfolder);
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

int
camel_filter_driver_remove_rule_by_name (CamelFilterDriver *d, const char *name)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (d);
	struct _filter_rule *node;
	
	node = (struct _filter_rule *) p->rules.head;
	while (node->next) {
		if (!strcmp (node->name, name)) {
			e_dlist_remove ((EDListNode *) node);
			g_free (node->match);
			g_free (node->action);
			g_free (node->name);
			g_free (node);
			
			return 0;
		}
		
		node = node->next;
	}
	
	return -1;
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
			CamelFolder *outbox;
			
			outbox = open_folder (driver, folder);
			if (!outbox)
				break;
			
			if (outbox == p->source)
				break;
			
			if (!p->modified && p->uid && p->source && camel_folder_has_summary_capability (p->source)) {
				GPtrArray *uids;
				
				uids = g_ptr_array_new ();
				g_ptr_array_add (uids, (char *) p->uid);
				camel_folder_transfer_messages_to (p->source, uids, outbox, NULL, FALSE, p->ex);
				g_ptr_array_free (uids, TRUE);
			} else {
				if (p->message == NULL)
					p->message = camel_folder_get_message (p->source, p->uid, p->ex);
				
				if (!p->message)
					continue;
				
				camel_folder_append_message (outbox, p->message, p->info, NULL, p->ex);
			}
			
			if (!camel_exception_is_set (p->ex))
				p->copied = TRUE;
			
			camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Copy to folder %s",
						 folder);
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
			CamelFolder *outbox;
			
			outbox = open_folder (driver, folder);
			if (!outbox)
				break;
			
			if (outbox == p->source)
				break;
			
			if (!p->modified && p->uid && p->source && camel_folder_has_summary_capability (p->source)) {
				GPtrArray *uids;
				
				uids = g_ptr_array_new ();
				g_ptr_array_add (uids, (char *) p->uid);
				camel_folder_transfer_messages_to (p->source, uids, outbox, NULL, FALSE, p->ex);
				g_ptr_array_free (uids, TRUE);
			} else {
				if (p->message == NULL)
					p->message = camel_folder_get_message (p->source, p->uid, p->ex);
				
				if (!p->message)
					continue;
				
				camel_folder_append_message (outbox, p->message, p->info, NULL, p->ex);
			}
			
			if (!camel_exception_is_set (p->ex)) {
				/* a 'move' is a copy & delete */
				p->copied = TRUE;
				p->deleted = TRUE;
			}
			
			camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Move to folder %s",
						 folder);
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
		if (p->source && p->uid && camel_folder_has_summary_capability (p->source))
			camel_folder_set_message_user_tag (p->source, p->uid, "colour", argv[0]->value.string);
		else
			camel_message_info_set_user_tag(p->info, "colour", argv[0]->value.string);
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
		if (p->source && p->uid && camel_folder_has_summary_capability (p->source))
			camel_folder_set_message_user_tag (p->source, p->uid, "score", value);
		else
			camel_message_info_set_user_tag(p->info, "score", value);
		camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Set score to %d", argv[0]->value.number);
		g_free (value);
	}
	
	return NULL;
}

static ESExpResult *
set_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	guint32 flags;
	
	d(fprintf (stderr, "setting flag\n"));
	if (argc == 1 && argv[0]->type == ESEXP_RES_STRING) {
		flags = camel_system_flag (argv[0]->value.string);
		if (p->source && p->uid && camel_folder_has_summary_capability (p->source))
			camel_folder_set_message_flags (p->source, p->uid, flags, ~0);
		else
			camel_message_info_set_flags(p->info, flags | CAMEL_MESSAGE_FOLDER_FLAGGED, ~0);
		camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Set %s flag", argv[0]->value.string);
	}
	
	return NULL;
}

static ESExpResult *
unset_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	guint32 flags;
	
	d(fprintf (stderr, "unsetting flag\n"));
	if (argc == 1 && argv[0]->type == ESEXP_RES_STRING) {
		flags = camel_system_flag (argv[0]->value.string);
		if (p->source && p->uid && camel_folder_has_summary_capability (p->source))
			camel_folder_set_message_flags (p->source, p->uid, flags, 0);
		else
			camel_message_info_set_flags(p->info, flags | CAMEL_MESSAGE_FOLDER_FLAGGED, 0);
		camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Unset %s flag", argv[0]->value.string);
	}
	
	return NULL;
}

static int
pipe_to_system (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	int result, status, fds[4], i;
	CamelMimeMessage *message = NULL;
	CamelMimeParser *parser;
	CamelStream *stream, *mem;
	pid_t pid;
	
	if (argc < 1 || argv[0]->value.string[0] == '\0')
		return 0;
	
	/* make sure we have the message... */
	if (p->message == NULL) {
		if (!(p->message = camel_folder_get_message (p->source, p->uid, p->ex)))
			return -1;
	}
	
	for (i = 0; i < 4; i++)
		fds[i] = -1;
	
	for (i = 0; i < 4; i += 2) {
		if (pipe (fds + i) == -1) {
			camel_exception_setv (p->ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Failed to create pipe to '%s': %s"),
					      argv[0]->value.string, g_strerror (errno));
			
			for (i = 0; i < 4; i++) {
				if (fds[i] == -1)
					break;
				close (fds[i]);
			}
			
			return -1;
		}
	}
	
	if (!(pid = fork ())) {
		/* child process */
		GPtrArray *args;
		int maxfd, fd;
		
		fd = open ("/dev/null", O_WRONLY);
		
		if (dup2 (fds[0], STDIN_FILENO) < 0 ||
		    dup2 (fds[3], STDOUT_FILENO) < 0 ||
		    dup2 (fd, STDERR_FILENO) < 0)
			_exit (255);
		
		setsid ();
		
		maxfd = sysconf (_SC_OPEN_MAX);
		for (fd = 3; fd < maxfd; fd++)
			fcntl (fd, F_SETFD, FD_CLOEXEC);
		
		args = g_ptr_array_new ();
		for (i = 0; i < argc; i++)
			g_ptr_array_add (args, argv[i]->value.string);
		g_ptr_array_add (args, NULL);
		
		execvp (argv[0]->value.string, (char **) args->pdata);
		
		g_ptr_array_free (args, TRUE);
		
		d(printf ("Could not execute %s: %s\n", argv[0]->value.string, g_strerror (errno)));
		_exit (255);
	} else if (pid < 0) {
		camel_exception_setv (p->ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to create child process '%s': %s"),
				      argv[0]->value.string, g_strerror (errno));
		return -1;
	}
	
	/* parent process */
	close (fds[0]);
	close (fds[3]);
	
	stream = camel_stream_fs_new_with_fd (fds[1]);
	if (camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (p->message), stream) == -1) {
		camel_object_unref (stream);
		close (fds[2]);
		goto wait;
	}
	
	if (camel_stream_flush (stream) == -1) {
		camel_object_unref (stream);
		close (fds[2]);
		goto wait;
	}
	
	camel_object_unref (stream);
	
	stream = camel_stream_fs_new_with_fd (fds[2]);
	mem = camel_stream_mem_new ();
	if (camel_stream_write_to_stream (stream, mem) == -1) {
		camel_object_unref (stream);
		camel_object_unref (mem);
		goto wait;
	}
	
	camel_object_unref (stream);
	camel_stream_reset (mem);
	
	parser = camel_mime_parser_new ();
	camel_mime_parser_init_with_stream (parser, mem);
	camel_mime_parser_scan_from (parser, FALSE);
	camel_object_unref (mem);
	
	message = camel_mime_message_new ();
	if (camel_mime_part_construct_from_parser ((CamelMimePart *) message, parser) == -1) {
		camel_exception_setv (p->ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Invalid message stream received from %s: %s"),
				      argv[0]->value.string,
				      g_strerror (camel_mime_parser_errno (parser)));
		camel_object_unref (message);
		message = NULL;
	} else {
		camel_object_unref (p->message);
		p->message = message;
		p->modified = TRUE;
	}
	
	camel_object_unref (parser);
	
 wait:
	
	result = waitpid (pid, &status, 0);
	
	if (result == -1 && errno == EINTR) {
		/* child process is hanging... */
		kill (pid, SIGTERM);
		sleep (1);
		result = waitpid (pid, &status, WNOHANG);
		if (result == 0) {
			/* ...still hanging, set phasers to KILL */
			kill (pid, SIGKILL);
			sleep (1);
			result = waitpid (pid, &status, WNOHANG);
		}
	}
	
	if (message && result != -1 && WIFEXITED (status))
		return WEXITSTATUS (status);
	else
		return -1;
}

static ESExpResult *
pipe_message (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	int i;
	
	/* make sure all args are strings */
	for (i = 0; i < argc; i++) {
		if (argv[i]->type != ESEXP_RES_STRING)
			return NULL;
	}
	
	camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Piping message to %s", argv[0]->value.string);
	pipe_to_system (f, argc, argv, driver);
	
	return NULL;
}

static ESExpResult *
do_shell (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	GString *command;
	GPtrArray *args;
	int i;
	
	d(fprintf (stderr, "executing shell command\n"));
	
	command = g_string_new ("");
	
	args = g_ptr_array_new ();
	
	/* make sure all args are strings */
	for (i = 0; i < argc; i++) {
		if (argv[i]->type != ESEXP_RES_STRING)
			goto done;
		
		g_ptr_array_add (args, argv[i]->value.string);
		
		g_string_append (command, argv[i]->value.string);
		g_string_append_c (command, ' ');
	}
	
	g_string_truncate (command, command->len - 1);
	
	if (p->shellfunc && argc >= 1) {
		p->shellfunc (driver, argc, (char **) args->pdata, p->shelldata);
		camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Executing shell command: [%s]",
					 command->str);
	}
	
 done:
	
	g_ptr_array_free (args, TRUE);
	g_string_free (command, TRUE);
	
	return NULL;
}

static ESExpResult *
do_beep (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	
	d(fprintf (stderr, "beep\n"));
	
	if (p->beep) {
		p->beep (driver, p->beepdata);
		camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Beep");
	}
	
	return NULL;
}

static ESExpResult *
play_sound (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	
	d(fprintf (stderr, "play sound\n"));
	
	if (p->playfunc && argc == 1 && argv[0]->type == ESEXP_RES_STRING) {
		p->playfunc (driver, argv[0]->value.string, p->playdata);
		camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Play sound");
	}
	
	return NULL;
}

static ESExpResult *
do_only_once (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	
	d(fprintf (stderr, "only once\n"));
	
	if (argc == 2 && !g_hash_table_lookup (p->only_once, argv[0]->value.string))
		g_hash_table_insert (p->only_once, g_strdup (argv[0]->value.string),
				     g_strdup (argv[1]->value.string));
	
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
		return camelfolder == FOLDER_INVALID?NULL:camelfolder;

	/* if we have a default folder, ignore exceptions.  This is so
	   a bad filter rule on pop or local delivery doesn't result
	   in duplicate mails, just mail going to inbox.  Otherwise,
	   we want to know about exceptions and abort processing */
	if (p->defaultfolder) {
		CamelException ex;

		camel_exception_init (&ex);
		camelfolder = p->get_folder (driver, folder_url, p->data, &ex);
		camel_exception_clear (&ex);
	} else {
		camelfolder = p->get_folder (driver, folder_url, p->data, p->ex);
	}
	
	if (camelfolder) {
		g_hash_table_insert (p->folders, g_strdup (folder_url), camelfolder);
		camel_folder_freeze (camelfolder);
	} else {
		g_hash_table_insert (p->folders, g_strdup (folder_url), FOLDER_INVALID);
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

	if (folder != FOLDER_INVALID) {
		camel_folder_sync (folder, FALSE, camel_exception_is_set(p->ex)?NULL : p->ex);
		camel_folder_thaw (folder);
		camel_object_unref (folder);
	}

	report_status(driver, CAMEL_FILTER_STATUS_PROGRESS, g_hash_table_size(p->folders)* 100 / p->closed, _("Syncing folders"));
}

/* flush/close all folders */
static int
close_folders (CamelFilterDriver *driver)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);

	report_status(driver, CAMEL_FILTER_STATUS_PROGRESS, 0, _("Syncing folders"));

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
			const char *from = NULL;
			char date[50];
			time_t t;
			
			/* FIXME: does this need locking?  Probably */
			
			from = camel_message_info_from (p->info);
			subject = camel_message_info_subject (p->info);
			
			time (&t);
			strftime (date, 49, "%a, %d %b %Y %H:%M:%S", localtime (&t));
			fprintf (p->logfile, "Applied filter \"%s\" to message from %s - \"%s\" at %s\n",
				 str, from ? from : "unknown", subject ? subject : "", date);
			
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


struct _run_only_once {
	CamelFilterDriver *driver;
	CamelException *ex;
};

static gboolean
run_only_once (gpointer key, char *action, struct _run_only_once *data)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (data->driver);
	CamelException *ex = data->ex;
	ESExpResult *r;
	
	d(printf ("evaluating: %s\n\n", action));
	
	e_sexp_input_text (p->eval, action, strlen (action));
	if (e_sexp_parse (p->eval) == -1) {
		if (!camel_exception_is_set (ex))
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Error parsing filter: %s: %s"),
					      e_sexp_error (p->eval), action);
		goto done;
	}
	
	r = e_sexp_eval (p->eval);
	if (r == NULL) {
		if (!camel_exception_is_set (ex))
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Error executing filter: %s: %s"),
					      e_sexp_error (p->eval), action);
		goto done;
	}
	
	e_sexp_result_free (p->eval, r);
	
 done:
	
	g_free (key);
	g_free (action);
	
	return TRUE;
}


/**
 * camel_filter_driver_flush:
 * @driver:
 * @ex:
 *
 * Flush all of the only-once filter actions.
 **/
void
camel_filter_driver_flush (CamelFilterDriver *driver, CamelException *ex)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	struct _run_only_once data;
	
	if (!p->only_once)
		return;
	
	data.driver = driver;
	data.ex = ex;
	
	g_hash_table_foreach_remove (p->only_once, (GHRFunc) run_only_once, &data);
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
camel_filter_driver_filter_mbox (CamelFilterDriver *driver, const char *mbox, const char *original_source_url, CamelException *ex)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	CamelMimeParser *mp = NULL;
	char *source_url = NULL;
	int fd = -1;
	int i = 0;
	struct stat st;
	int status;
	off_t last = 0;
	int ret = -1;
	
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
	
	while (camel_mime_parser_step (mp, 0, 0) == CAMEL_MIME_PARSER_STATE_FROM) {
		CamelMessageInfo *info;
		CamelMimeMessage *msg;
		int pc = 0;
		
		if (st.st_size > 0)
			pc = (int)(100.0 * ((double)camel_mime_parser_tell (mp) / (double)st.st_size));
		
		report_status (driver, CAMEL_FILTER_STATUS_START, pc, _("Getting message %d (%d%%)"), i, pc);
		
		msg = camel_mime_message_new ();
		if (camel_mime_part_construct_from_parser (CAMEL_MIME_PART (msg), mp) == -1) {
			camel_exception_set (ex, (errno==EINTR)?CAMEL_EXCEPTION_USER_CANCEL:CAMEL_EXCEPTION_SYSTEM, _("Cannot open message"));
			report_status (driver, CAMEL_FILTER_STATUS_END, 100, _("Failed on message %d"), i);
			camel_object_unref (msg);
			goto fail;
		}
		
		info = camel_message_info_new_from_header(NULL, ((CamelMimePart *)msg)->headers);
		((CamelMessageInfoBase *)info)->size = camel_mime_parser_tell(mp) - last;
		last = camel_mime_parser_tell(mp);
		status = camel_filter_driver_filter_message (driver, msg, info, NULL, NULL, source_url, 
							     original_source_url ? original_source_url : source_url, ex);
		camel_object_unref (msg);
		if (camel_exception_is_set (ex) || status == -1) {
			report_status (driver, CAMEL_FILTER_STATUS_END, 100, _("Failed on message %d"), i);
			camel_message_info_free (info);
			goto fail;
		}
		
		i++;
		
		/* skip over the FROM_END state */
		camel_mime_parser_step (mp, 0, 0);

		camel_message_info_free (info);
	}
	
	if (p->defaultfolder) {
		report_status(driver, CAMEL_FILTER_STATUS_PROGRESS, 100, _("Syncing folder"));
		camel_folder_sync(p->defaultfolder, FALSE, camel_exception_is_set (ex) ? NULL : ex);
	}
	
	report_status (driver, CAMEL_FILTER_STATUS_END, 100, _("Complete"));
	
	ret = 0;
fail:
	g_free (source_url);
	if (fd != -1)
		close (fd);
	if (mp)
		camel_object_unref (mp);
	
	return -1;
}


/**
 * camel_filter_driver_filter_folder:
 * @driver: CamelFilterDriver
 * @folder: CamelFolder to be filtered
 * @cache: UID cache (needed for POP folders)
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
camel_filter_driver_filter_folder (CamelFilterDriver *driver, CamelFolder *folder, CamelUIDCache *cache,
				   GPtrArray *uids, gboolean remove, CamelException *ex)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	gboolean freeuids = FALSE;
	CamelMessageInfo *info;
	char *source_url, *service_url;
	int status = 0;
	CamelURL *url;
	int i;
	
	service_url = camel_service_get_url (CAMEL_SERVICE (camel_folder_get_parent_store (folder)));
	url = camel_url_new (service_url, NULL);
	g_free (service_url);
	
	source_url = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
	camel_url_free (url);
	
	if (uids == NULL) {
		uids = camel_folder_get_uids (folder);
		freeuids = TRUE;
	}
	
	for (i = 0; i < uids->len; i++) {
		int pc = (100 * i)/uids->len;
		
		report_status (driver, CAMEL_FILTER_STATUS_START, pc, _("Getting message %d of %d"), i+1,
			       uids->len);
		
		if (camel_folder_has_summary_capability (folder))
			info = camel_folder_get_message_info (folder, uids->pdata[i]);
		else
			info = NULL;
		
		status = camel_filter_driver_filter_message (driver, NULL, info, uids->pdata[i],
							     folder, source_url, source_url, ex);
		
		if (camel_folder_has_summary_capability (folder))
			camel_folder_free_message_info (folder, info);
		
		if (camel_exception_is_set (ex) || status == -1) {
			report_status (driver, CAMEL_FILTER_STATUS_END, 100, _("Failed at message %d of %d"),
				       i+1, uids->len);
			status = -1;
			break;
		}
		
		if (remove)
			camel_folder_set_message_flags (folder, uids->pdata[i],
							CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN, ~0);
		
		if (cache)
			camel_uid_cache_save_uid (cache, uids->pdata[i]);
	}
	
	if (p->defaultfolder) {
		report_status (driver, CAMEL_FILTER_STATUS_PROGRESS, 100, _("Syncing folder"));
		camel_folder_sync (p->defaultfolder, FALSE, camel_exception_is_set (ex) ? NULL : ex);
	}
	
	if (i == uids->len)
		report_status (driver, CAMEL_FILTER_STATUS_END, 100, _("Complete"));
	
	if (freeuids)
		camel_folder_free_uids (folder, uids);
	
	g_free (source_url);
	
	return status;
}


struct _get_message {
	struct _CamelFilterDriverPrivate *p;
	const char *source_url;
};


static CamelMimeMessage *
get_message_cb (void *data, CamelException *ex)
{
	struct _get_message *msgdata = data;
	struct _CamelFilterDriverPrivate *p = msgdata->p;
	const char *source_url = msgdata->source_url;
	CamelMimeMessage *message;
	
	if (p->message) {
		message = p->message;
		camel_object_ref (message);
	} else {
		const char *uid;
		
		if (p->uid)
			uid = p->uid;
		else
			uid = camel_message_info_uid (p->info);
		
		message = camel_folder_get_message (p->source, uid, ex);
	}
	
	if (source_url && message && camel_mime_message_get_source (message) == NULL)
		camel_mime_message_set_source (message, source_url);
	
	return message;
}

/**
 * camel_filter_driver_filter_message:
 * @driver: CamelFilterDriver
 * @message: message to filter or NULL
 * @info: message info or NULL
 * @uid: message uid or NULL
 * @source: source folder or NULL
 * @source_url: url of source folder or NULL
 * @original_source_url: url of original source folder (pre-movemail) or NULL
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
				    const char *original_source_url,
				    CamelException *ex)
{
	struct _CamelFilterDriverPrivate *p = _PRIVATE (driver);
	struct _filter_rule *node;
	gboolean freeinfo = FALSE;
	gboolean filtered = FALSE;
	ESExpResult *r;
	int result;
	
	/* FIXME: make me into a g_return_if_fail/g_assert or whatever... */
	if (message == NULL && (source == NULL || uid == NULL)) {
		g_warning ("there is no way to fetch the message using the information provided...");
		return -1;
	}
	
	if (info == NULL) {
		struct _camel_header_raw *h;
		
		if (message) {
			camel_object_ref (message);
		} else {
			message = camel_folder_get_message (source, uid, ex);
			if (!message)
				return -1;
		}
		
		h = CAMEL_MIME_PART (message)->headers;
		info = camel_message_info_new_from_header (NULL, h);
		freeinfo = TRUE;
	} else {
		if (camel_message_info_flags(info) & CAMEL_MESSAGE_DELETED)
			return 0;
		
		uid = camel_message_info_uid (info);
		
		if (message)
			camel_object_ref (message);
	}
	
	p->ex = ex;
	p->terminated = FALSE;
	p->deleted = FALSE;
	p->copied = FALSE;
	p->message = message;
	p->info = info;
	p->uid = uid;
	p->source = source;
	
	if (message && original_source_url && camel_mime_message_get_source (message) == NULL)
		camel_mime_message_set_source (message, original_source_url);
	
	node = (struct _filter_rule *) p->rules.head;
	result = CAMEL_SEARCH_NOMATCH;
	while (node->next && !p->terminated) {
		struct _get_message data;
		
		d(printf("applying rule %s\naction %s\n", node->match, node->action));
		
		data.p = p;
		data.source_url = original_source_url;
		
		result = camel_filter_search_match (p->session, get_message_cb, &data, p->info, 
						    original_source_url ? original_source_url : source_url,
						    node->match, p->ex);
		
		switch (result) {
		case CAMEL_SEARCH_ERROR:
			goto error;
		case CAMEL_SEARCH_MATCHED:
			filtered = TRUE;
			camel_filter_driver_log (driver, FILTER_LOG_START, node->name);

			if (camel_debug(":filter"))
				printf("filtering '%s' applying rule %s\n",
				       camel_message_info_subject(info)?camel_message_info_subject(info):"?no subject?", node->name);

			/* perform necessary filtering actions */
			e_sexp_input_text (p->eval, node->action, strlen (node->action));
			if (e_sexp_parse (p->eval) == -1) {
				camel_exception_setv (ex, 1, _("Error parsing filter: %s: %s"),
						      e_sexp_error (p->eval), node->action);
				goto error;
			}
			r = e_sexp_eval (p->eval);
			if (camel_exception_is_set(p->ex))
				goto error;

			if (r == NULL) {
				camel_exception_setv (ex, 1, _("Error executing filter: %s: %s"),
						      e_sexp_error (p->eval), node->action);
				goto error;
			}
			e_sexp_result_free (p->eval, r);
		default:
			break;
		}
		
		node = node->next;
	}
	
	/* *Now* we can set the DELETED flag... */
	if (p->deleted) {
		if (p->source && p->uid && camel_folder_has_summary_capability (p->source))
			camel_folder_set_message_flags(p->source, p->uid, CAMEL_MESSAGE_DELETED|CAMEL_MESSAGE_SEEN, ~0);
		else
			camel_message_info_set_flags(info, CAMEL_MESSAGE_DELETED|CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_FOLDER_FLAGGED, ~0);
	}
	
	/* Logic: if !Moved and there exists a default folder... */
	if (!(p->copied && p->deleted) && p->defaultfolder) {
		/* copy it to the default inbox */
		filtered = TRUE;
		camel_filter_driver_log (driver, FILTER_LOG_ACTION, "Copy to default folder");

		if (camel_debug(":filter"))
			printf("filtering '%s' copy %s to default folder\n",
			       camel_message_info_subject(info)?camel_message_info_subject(info):"?no subject?",
			       p->modified?"modified message":"");

		if (!p->modified && p->uid && p->source && camel_folder_has_summary_capability (p->source)) {
			GPtrArray *uids;
			
			uids = g_ptr_array_new ();
			g_ptr_array_add (uids, (char *) p->uid);
			camel_folder_transfer_messages_to (p->source, uids, p->defaultfolder, NULL, FALSE, p->ex);
			g_ptr_array_free (uids, TRUE);
		} else {
			if (p->message == NULL) {
				p->message = camel_folder_get_message (source, uid, ex);
				if (!p->message)
					goto error;
			}

			camel_folder_append_message (p->defaultfolder, p->message, p->info, NULL, p->ex);
		}
	}
	
	if (p->message)
		camel_object_unref (p->message);
	
	if (freeinfo)
		camel_message_info_free (info);
	
	return 0;
	
 error:
	if (filtered)
		camel_filter_driver_log (driver, FILTER_LOG_END, NULL);
	
	if (p->message)
		camel_object_unref (p->message);
	
	if (freeinfo)
		camel_message_info_free (info);
	
	return -1;
}
