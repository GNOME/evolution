/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-ops.c: callbacks for the mail toolbar/menus */

/* 
 * Authors: Dan Winship <danw@helixcode.com>
 *  	    Jeffrey Stedfast <fejj@helixcode.com>
 *          Peter Williams <peterw@helixcode.com>
 *          Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2000,2001 Ximian, Inc. (http://www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include <gnome.h>
#include <ctype.h>
#include <errno.h>
#include <camel/camel-mime-filter-from.h>
#include <camel/camel-operation.h>
#include "mail.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-vfolder.h"
#include "composer/e-msg-composer.h"
#include "folder-browser.h"
#include "e-util/e-html-utils.h"

#include "filter/filter-filter.h"

#include "mail-mt.h"

#define d(x) x

#define mail_tool_camel_lock_down()
#define mail_tool_camel_lock_up()

FilterContext *
mail_load_filter_context(void)
{
	char *userrules;
	char *systemrules;
	FilterContext *fc;
	
	userrules = g_strdup_printf ("%s/filters.xml", evolution_dir);
	systemrules = g_strdup_printf ("%s/evolution/filtertypes.xml", EVOLUTION_DATADIR);
	fc = filter_context_new ();
	rule_context_load ((RuleContext *)fc, systemrules, userrules);
	g_free (userrules);
	g_free (systemrules);
	
	return fc;
}

static void
setup_filter_driver(CamelFilterDriver *driver, FilterContext *fc, const char *source)
{
	GString *fsearch, *faction;
	FilterFilter *rule = NULL;

	if (TRUE /* perform_logging */) {
		char *filename = g_strdup_printf("%s/evolution-filter-log", evolution_dir);
		/* FIXME: This is a nasty little thing to stop leaking file handles.
		   Needs to be setup elsewhere. */
		static FILE *logfile = NULL;

		if (logfile == NULL)
			logfile = fopen(filename, "a+");
		g_free(filename);
		if (logfile)
			camel_filter_driver_set_logfile(driver, logfile);
	}

	fsearch = g_string_new ("");
	faction = g_string_new ("");
	
	while ((rule = (FilterFilter *)rule_context_next_rule((RuleContext *)fc, (FilterRule *)rule, source))) {
		g_string_truncate (fsearch, 0);
		g_string_truncate (faction, 0);
		
		filter_rule_build_code ((FilterRule *)rule, fsearch);
		filter_filter_build_action (rule, faction);

		camel_filter_driver_add_rule(driver, ((FilterRule *)rule)->name, fsearch->str, faction->str);
	}
	
	g_string_free (fsearch, TRUE);
	g_string_free (faction, TRUE);
}

static CamelFolder *
filter_get_folder(CamelFilterDriver *d, const char *uri, void *data, CamelException *ex)
{
	CamelFolder *folder;

	folder = mail_tool_uri_to_folder(uri, ex);

	return folder;
}

/* used for both just filtering a folder + uid's, and for filtering a whole folder */
/* used both for fetching mail, and for filtering mail */
struct _filter_mail_msg {
	struct _mail_msg msg;

	CamelFolder *source_folder; /* where they come from */
	GPtrArray *source_uids;	/* uids to copy, or NULL == copy all */
	CamelOperation *cancel;
	CamelFilterDriver *driver;
	int delete;		/* delete messages after filtering them? */
	CamelFolder *destination; /* default destination for any messages, NULL for none */
};

/* since fetching also filters, we subclass the data here */
struct _fetch_mail_msg {
	struct _filter_mail_msg fmsg;

	CamelOperation *cancel;	/* we have our own cancellation struct, the other should be empty */
	int keep;		/* keep on server? */

	char *source_uri;

	void (*done)(char *source, void *data);
	void *data;
};

/* filter a folder, or a subset thereof, uses source_folder/source_uids */
/* this is shared with fetch_mail */
static void
filter_folder_filter(struct _mail_msg *mm)
{
	struct _filter_mail_msg *m = (struct _filter_mail_msg *)mm;
	CamelFolder *folder;
	GPtrArray *uids, *folder_uids = NULL;

	if (m->cancel)
		camel_operation_register(m->cancel);

	folder = m->source_folder;

	if (folder == NULL || camel_folder_get_message_count (folder) == 0) {
		if (m->cancel)
			camel_operation_unregister(m->cancel);
		return;
	}

	if (m->destination) {
		camel_folder_freeze(m->destination);
		camel_filter_driver_set_default_folder(m->driver, m->destination);
	}

	camel_folder_freeze(folder);

	if (m->source_uids)
		uids = m->source_uids;
	else
		folder_uids = uids = camel_folder_get_uids (folder);

	camel_filter_driver_filter_folder(m->driver, folder, uids, m->delete, &mm->ex);

	if (folder_uids)
		camel_folder_free_uids(folder, folder_uids);

	/* sync and expunge */
	camel_folder_sync (folder, TRUE, &mm->ex);
	camel_folder_thaw(folder);

	if (m->destination)
		camel_folder_thaw(m->destination);

	if (m->cancel)
		camel_operation_unregister(m->cancel);
}

static void
filter_folder_filtered(struct _mail_msg *mm)
{
}

static void
filter_folder_free(struct _mail_msg *mm)
{
	struct _filter_mail_msg *m = (struct _filter_mail_msg *)mm;
	int i;

	if (m->source_folder)
		camel_object_unref((CamelObject *)m->source_folder);
	if (m->source_uids) {
		for (i=0;i<m->source_uids->len;i++)
			g_free(m->source_uids->pdata[i]);
		g_ptr_array_free(m->source_uids, TRUE);
	}
	if (m->cancel)
		camel_operation_unref(m->cancel);
	if (m->destination)
		camel_object_unref((CamelObject *)m->destination);
	if (m->driver)
		camel_object_unref((CamelObject *)m->driver);
}

static struct _mail_msg_op filter_folder_op = {
	NULL,			/* we do our own progress reporting? */
	filter_folder_filter,
	filter_folder_filtered,
	filter_folder_free,
};

void
mail_filter_folder(CamelFolder *source_folder, GPtrArray *uids,
		   FilterContext *fc, const char *type,
		   CamelOperation *cancel)
{
	struct _filter_mail_msg *m;

	m = mail_msg_new(&filter_folder_op, NULL, sizeof(*m));
	m->source_folder = source_folder;
	camel_object_ref((CamelObject *)source_folder);
	m->source_uids = uids;
	m->delete = FALSE;
	if (cancel) {
		m->cancel = cancel;
		camel_operation_ref(cancel);
	}

	m->driver = camel_filter_driver_new(filter_get_folder, NULL);
	setup_filter_driver(m->driver, fc, type);

	e_thread_put(mail_thread_new, (EMsg *)m);
}

/* convenience function for it */
void mail_filter_on_demand(CamelFolder *folder, GPtrArray *uids)
{
	FilterContext *fc;

	fc = mail_load_filter_context();
	mail_filter_folder(folder, uids, fc, FILTER_SOURCE_INCOMING, NULL);
	gtk_object_unref((GtkObject *)fc);
}

/* ********************************************************************** */

static void
fetch_mail_fetch(struct _mail_msg *mm)
{
	struct _fetch_mail_msg *m = (struct _fetch_mail_msg *)mm;
	struct _filter_mail_msg *fm = (struct _filter_mail_msg *)mm;
	int i;

	if (m->cancel)
		camel_operation_register(m->cancel);

	if ( (fm->destination = mail_tool_get_local_inbox(&mm->ex)) == NULL) {
		if (m->cancel)
			camel_operation_unregister(m->cancel);
		return;
	}

	/* FIXME: this should support keep_on_server too, which would then perform a spool
	   access thingy, right?  problem is matching raw messages to uid's etc. */
	if (!strncmp (m->source_uri, "mbox:", 5)) {
		char *path = mail_tool_do_movemail (m->source_uri, &mm->ex);
		
		if (path && !camel_exception_is_set (&mm->ex)) {
			camel_folder_freeze(fm->destination);
			camel_filter_driver_set_default_folder(fm->driver, fm->destination);
			camel_filter_driver_filter_mbox(fm->driver, path, &mm->ex);
			camel_folder_thaw(fm->destination);
			
			if (!camel_exception_is_set (&mm->ex))
				unlink (path);
		}
		g_free (path);
	} else {
		CamelFolder *folder = fm->source_folder = mail_tool_get_inbox(m->source_uri, &mm->ex);
		
		if (folder) {
			/* this handles 'keep on server' stuff, if we have any new uid's to copy
			   across, we need to copy them to a new array 'cause of the way fetch_mail_free works */
			CamelUIDCache *cache = NULL;
			char *cachename;
			
			cachename = mail_config_folder_to_cachename (folder, "cache-");
			cache = camel_uid_cache_new (cachename);
			g_free (cachename);
			
			if (cache) {
				GPtrArray *folder_uids, *cache_uids, *uids;
				
				folder_uids = camel_folder_get_uids (folder);
				cache_uids = camel_uid_cache_get_new_uids (cache, folder_uids);
				if (cache_uids) {
					/* need to copy this, sigh */
					fm->source_uids = uids = g_ptr_array_new ();
					g_ptr_array_set_size (uids, cache_uids->len);
					for (i = 0; i < cache_uids->len; i++)
						uids->pdata[i] = g_strdup (cache_uids->pdata[i]);
					camel_uid_cache_free_uids (cache_uids);
					
					filter_folder_filter (mm);
					
					/* if we are not to delete the messages, save the UID cache */
					if (!fm->delete && !camel_exception_is_set (&mm->ex))
						camel_uid_cache_save (cache);
					
					camel_uid_cache_destroy (cache);
				}
				camel_folder_free_uids (folder, folder_uids);
			} else {
				filter_folder_filter (mm);
			}
		}
	}

	if (m->cancel)
		camel_operation_unregister(m->cancel);

	/* we unref this here as it may have more work to do (syncing
	   folders and whatnot) before we are really done */
	/* should this be cancellable too? (i.e. above unregister above) */
	camel_object_unref((CamelObject *)m->fmsg.driver);
	m->fmsg.driver = NULL;
}

static void
fetch_mail_fetched(struct _mail_msg *mm)
{
	struct _fetch_mail_msg *m = (struct _fetch_mail_msg *)mm;

	if (m->done)
		m->done(m->source_uri, m->data);
}

static void
fetch_mail_free(struct _mail_msg *mm)
{
	struct _fetch_mail_msg *m = (struct _fetch_mail_msg *)mm;

	g_free(m->source_uri);
	if (m->cancel)
		camel_operation_unref(m->cancel);

	filter_folder_free(mm);
}

static struct _mail_msg_op fetch_mail_op = {
	NULL,			/* we do our own progress reporting */
	fetch_mail_fetch,
	fetch_mail_fetched,
	fetch_mail_free,
};

/* ouch, a 'do everything' interface ... */
void mail_fetch_mail(const char *source, int keep,
		     FilterContext *fc, const char *type,
		     CamelOperation *cancel,
		     CamelFilterGetFolderFunc get_folder, void *get_data,
		     CamelFilterStatusFunc *status, void *status_data,
		     void (*done)(char *source, void *data), void *data)
{
	struct _fetch_mail_msg *m;
	struct _filter_mail_msg *fm;

	m = mail_msg_new(&fetch_mail_op, NULL, sizeof(*m));
	fm = (struct _filter_mail_msg *)m;
	m->source_uri = g_strdup(source);
	fm->delete = !keep;
	if (cancel) {
		m->cancel = cancel;
		camel_operation_ref(cancel);
	}
	m->done = done;
	m->data = data;

	fm->driver = camel_filter_driver_new(get_folder, get_data);
	setup_filter_driver(fm->driver, fc, type);
	if (status)
		camel_filter_driver_set_status_func(fm->driver, status, status_data);

	e_thread_put(mail_thread_new, (EMsg *)m);
}


/* updating of imap folders etc */
struct _update_info {
	EvolutionStorage *storage;

	void (*done)(CamelStore *, void *data);
	void *data;
};

static void do_update_subfolders_rec(CamelStore *store, CamelFolderInfo *info, EvolutionStorage *storage, const char *prefix)
{
	char *path, *name;
	
	path = g_strdup_printf("%s/%s", prefix, info->name);
	if (info->unread_message_count > 0)
		name = g_strdup_printf("%s (%d)", info->name, info->unread_message_count);
	else
		name = g_strdup(info->name);
	
	evolution_storage_update_folder(storage, path, name, info->unread_message_count > 0);
	g_free(name);
	if (info->child)
		do_update_subfolders_rec(store, info->child, storage, path);
	if (info->sibling)
		do_update_subfolders_rec(store, info->sibling, storage, prefix);
	g_free(path);
}

static void do_update_subfolders(CamelStore *store, CamelFolderInfo *info, void *data)
{
	struct _update_info *uinfo = data;
	
	if (uinfo) {
		do_update_subfolders_rec(store, info, uinfo->storage, "");
	}

	if (uinfo->done)
		uinfo->done(store, uinfo->data);

	gtk_object_unref((GtkObject *)uinfo->storage);
	g_free(uinfo);
}

/* this interface is a little icky */
int mail_update_subfolders(CamelStore *store, EvolutionStorage *storage,
			   void (*done)(CamelStore *, void *data), void *data)
{
	struct _update_info *info;

	/* FIXME: This wont actually work entirely right, as a failure may lose this data */
	/* however, this isn't a big problem ... */
	info = g_malloc0(sizeof(*info));
	info->storage = storage;
	gtk_object_ref((GtkObject *)storage);
	info->done = done;
	info->data = data;

	return mail_get_folderinfo(store, do_update_subfolders, info);
}

/* ********************************************************************** */
/* sending stuff */
/* ** SEND MAIL *********************************************************** */

/* send 1 message to a specific transport */
static void
mail_send_message(CamelMimeMessage *message, const char *destination, CamelFilterDriver *driver, CamelException *ex)
{
	extern CamelFolder *sent_folder; /* FIXME */
	CamelMessageInfo *info;
	CamelTransport *xport;
	const char *version;

	if (SUB_VERSION[0] == '\0')
		version = "Evolution (" VERSION " - Preview Release)";
	else
		version = "Evolution (" VERSION "/" SUB_VERSION " - Preview Release)";
	camel_medium_add_header(CAMEL_MEDIUM (message), "X-Mailer", version);
	camel_mime_message_set_date(message, CAMEL_MESSAGE_DATE_CURRENT, 0);

	xport = camel_session_get_transport(session, destination, ex);
	if (camel_exception_is_set(ex))
		return;
	
	camel_transport_send(xport, (CamelMedium *)message, ex);
	camel_object_unref((CamelObject *)xport);
	if (camel_exception_is_set(ex))
		return;
	
	/* post-process */
	info = camel_message_info_new();
	info->flags = CAMEL_MESSAGE_SEEN;

	if (driver)
		camel_filter_driver_filter_message(driver, message, info, "", ex);
	
	if (sent_folder)
		camel_folder_append_message(sent_folder, message, info, ex);
	
	camel_message_info_free(info);
}

/* ********************************************************************** */

struct _send_mail_msg {
	struct _mail_msg msg;

	CamelFilterDriver *driver;
	char *destination;
	CamelMimeMessage *message;

	void (*done)(char *uri, CamelMimeMessage *message, gboolean sent, void *data);
	void *data;
};

static char *send_mail_desc(struct _mail_msg *mm, int done)
{
	struct _send_mail_msg *m = (struct _send_mail_msg *)mm;
	const char *subject;

	subject = camel_mime_message_get_subject(m->message);
	if (subject && subject[0])
		return g_strdup_printf (_("Sending \"%s\""), subject);
	else
		return g_strdup(_("Sending message"));
}

static void send_mail_send(struct _mail_msg *mm)
{
	struct _send_mail_msg *m = (struct _send_mail_msg *)mm;

	camel_operation_register(mm->cancel);
	mail_send_message(m->message, m->destination, m->driver, &mm->ex);
	camel_operation_unregister(mm->cancel);
}

static void send_mail_sent(struct _mail_msg *mm)
{
	struct _send_mail_msg *m = (struct _send_mail_msg *)mm;

	if (m->done)
		m->done(m->destination, m->message, !camel_exception_is_set(&mm->ex), m->data);
}

static void send_mail_free(struct _mail_msg *mm)
{
	struct _send_mail_msg *m = (struct _send_mail_msg *)mm;

	camel_object_unref((CamelObject *)m->message);
	g_free(m->destination);
}

static struct _mail_msg_op send_mail_op = {
	send_mail_desc,
	send_mail_send,
	send_mail_sent,
	send_mail_free,
};

int
mail_send_mail(const char *uri, CamelMimeMessage *message, void (*done) (char *uri, CamelMimeMessage *message, gboolean sent, void *data), void *data)
{
	struct _send_mail_msg *m;
	int id;
	FilterContext *fc;

	m = mail_msg_new(&send_mail_op, NULL, sizeof(*m));
	m->destination = g_strdup(uri);
	m->message = message;
	camel_object_ref((CamelObject *)message);
	m->data = data;
	m->done = done;

	id = m->msg.seq;

	m->driver = camel_filter_driver_new(filter_get_folder, NULL);
	fc = mail_load_filter_context();
	setup_filter_driver(m->driver, fc, FILTER_SOURCE_OUTGOING);
	gtk_object_unref((GtkObject *)fc);

	e_thread_put(mail_thread_new, (EMsg *)m);
	return id;
}

/* ** SEND MAIL QUEUE ***************************************************** */

struct _send_queue_msg {
	struct _mail_msg msg;

	CamelFolder *queue;
	char *destination;

	CamelFilterDriver *driver;
	CamelOperation *cancel;

	/* we use camelfilterstatusfunc, even though its not the filter doing it */
	CamelFilterStatusFunc *status;
	void *status_data;

	void (*done)(char *destination, void *data);
	void *data;
};

static void
report_status(struct _send_queue_msg *m, enum camel_filter_status_t status, int pc, const char *desc, ...)
{
	va_list ap;
	char *str;
	
	if (m->status) {
		va_start(ap, desc);
		str = g_strdup_vprintf(desc, ap);
		m->status(m->driver, status, pc, str, m->status_data);
		g_free(str);
	}
}

static void
send_queue_send(struct _mail_msg *mm)
{
	struct _send_queue_msg *m = (struct _send_queue_msg *)mm;
	GPtrArray *uids;
	int i;
	extern CamelFolder *sent_folder; /* FIXME */

	printf("sending queue\n");

	uids = camel_folder_get_uids(m->queue);
	if (uids == NULL || uids->len == 0)
		return;

	if (m->cancel)
		camel_operation_register(m->cancel);
	
	for (i=0; i<uids->len; i++) {
		CamelMimeMessage *message;
		char *destination;
		int pc = (100 * i)/uids->len;

		report_status(m, CAMEL_FILTER_STATUS_START, pc, "Sending message %d of %d", i+1, uids->len);
		
		message = camel_folder_get_message(m->queue, uids->pdata[i], &mm->ex);
		if (camel_exception_is_set(&mm->ex))
			break;

		/* Get the preferred transport URI */
		destination = (char *)camel_medium_get_header(CAMEL_MEDIUM(message), "X-Evolution-Transport");
		if (destination) {
			destination = g_strdup(destination);
			camel_medium_remove_header(CAMEL_MEDIUM(message), "X-Evolution-Transport");
			mail_send_message(message, g_strstrip(destination), m->driver, &mm->ex);
			g_free(destination);
		} else
			mail_send_message(message, m->destination, m->driver, &mm->ex);

		if (camel_exception_is_set(&mm->ex))
			break;

		camel_folder_set_message_flags(m->queue, uids->pdata[i], CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);
	}

	if (camel_exception_is_set(&mm->ex))
		report_status(m, CAMEL_FILTER_STATUS_END, 100, "Failed on message %d of %d", i+1, uids->len);
	else
		report_status(m, CAMEL_FILTER_STATUS_END, 100, "Complete.");

	camel_folder_free_uids(m->queue, uids);

	if (!camel_exception_is_set(&mm->ex))
		camel_folder_expunge(m->queue, &mm->ex);
	
	if (sent_folder)
		camel_folder_sync(sent_folder, FALSE, &mm->ex);

	if (m->cancel)
		camel_operation_unregister(m->cancel);
}

static void
send_queue_sent(struct _mail_msg *mm)
{
	struct _send_queue_msg *m = (struct _send_queue_msg *)mm;

	if (m->done)
		m->done(m->destination, m->data);
}

static void
send_queue_free(struct _mail_msg *mm)
{
	struct _send_queue_msg *m = (struct _send_queue_msg *)mm;
	
	camel_object_unref((CamelObject *)m->queue);
	g_free(m->destination);
	if (m->cancel)
		camel_operation_unref(m->cancel);
}

static struct _mail_msg_op send_queue_op = {
	NULL,			/* do our own reporting, as with fetch mail */
	send_queue_send,
	send_queue_sent,
	send_queue_free,
};

/* same interface as fetch_mail, just 'cause i'm lazy today (and we need to run it from the same spot?) */
void
mail_send_queue(CamelFolder *queue, const char *destination,
		FilterContext *fc, const char *type,
		CamelOperation *cancel,
		CamelFilterGetFolderFunc get_folder, void *get_data,
		CamelFilterStatusFunc *status, void *status_data,
		void (*done)(char *destination, void *data), void *data)
{
	struct _send_queue_msg *m;

	m = mail_msg_new(&send_queue_op, NULL, sizeof(*m));
	m->queue = queue;
	camel_object_ref((CamelObject *)queue);
	m->destination = g_strdup(destination);
	if (cancel) {
		m->cancel = cancel;
		camel_operation_ref(cancel);
	}
	m->status = status;
	m->status_data = status_data;
	m->done = done;
	m->data = data;

	m->driver = camel_filter_driver_new(get_folder, get_data);
	setup_filter_driver(m->driver, fc, type);

	e_thread_put(mail_thread_new, (EMsg *)m);
}

/* ** APPEND MESSAGE TO FOLDER ******************************************** */

struct _append_msg {
	struct _mail_msg msg;

        CamelFolder *folder;
	CamelMimeMessage *message;
        CamelMessageInfo *info;

	void (*done)(CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info, int ok, void *data);
	void *data;
};

static char *append_mail_desc(struct _mail_msg *mm, int done)
{
	return g_strdup(_("Saving message to folder"));
}

static void append_mail_append(struct _mail_msg *mm)
{
	struct _append_msg *m = (struct _append_msg *)mm;
	
	camel_mime_message_set_date(m->message, CAMEL_MESSAGE_DATE_CURRENT, 0);
	camel_folder_append_message(m->folder, m->message, m->info, &mm->ex);
}

static void append_mail_appended(struct _mail_msg *mm)
{
	struct _append_msg *m = (struct _append_msg *)mm;

	if (m->done)
		m->done(m->folder, m->message, m->info, !camel_exception_is_set(&mm->ex), m->data);
}

static void append_mail_free(struct _mail_msg *mm)
{
	struct _append_msg *m = (struct _append_msg *)mm;

	camel_object_unref((CamelObject *)m->message);
	camel_object_unref((CamelObject *)m->folder);
}

static struct _mail_msg_op append_mail_op = {
	append_mail_desc,
	append_mail_append,
	append_mail_appended,
	append_mail_free
};

void
mail_append_mail (CamelFolder *folder,
		  CamelMimeMessage *message,
		  CamelMessageInfo *info,
		  void (*done)(CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info, int ok, void *data), void *data)
{
	struct _append_msg *m;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	m = mail_msg_new(&append_mail_op, NULL, sizeof(*m));
	m->folder = folder;
	camel_object_ref((CamelObject *)folder);
	m->message = message;
	camel_object_ref((CamelObject *)message);
	m->info = info;

	m->done = done;
	m->data = data;

	e_thread_put(mail_thread_new, (EMsg *)m);
}

/* ** TRANSFER MESSAGES **************************************************** */

struct _transfer_msg {
	struct _mail_msg msg;

	CamelFolder *source;
	GPtrArray *uids;
	gboolean delete;
	char *dest_uri;
};

static char *transfer_messages_desc(struct _mail_msg *mm, int done)
{
	struct _transfer_msg *m = (struct _transfer_msg *)mm;

	return g_strdup_printf(m->delete?_("Moving messages to %s"):_("Copying messages to %s"),
			       m->dest_uri);
			       
}

static void transfer_messages_transfer(struct _mail_msg *mm)
{
	struct _transfer_msg *m = (struct _transfer_msg *)mm;
	CamelFolder *dest;
	int i;
	char *desc;
	void (*func) (CamelFolder *, const char *, 
		      CamelFolder *, 
		      CamelException *);

	if (m->delete) {
		func = camel_folder_move_message_to;
		desc = _("Moving");
	} else {
		func = camel_folder_copy_message_to;
		desc = _("Copying");
	}

	dest = mail_tool_uri_to_folder (m->dest_uri, &mm->ex);
	if (camel_exception_is_set (&mm->ex))
		return;

	camel_folder_freeze (m->source);
	camel_folder_freeze (dest);

	for (i = 0; i < m->uids->len; i++) {
		mail_statusf(_("%s message %d of %d (uid \"%s\")"), desc,
			     i + 1, m->uids->len, (char *)m->uids->pdata[i]);

		(func) (m->source, m->uids->pdata[i], dest, &mm->ex);
		if (camel_exception_is_set (&mm->ex))
			break;
	}

	camel_folder_thaw(m->source);
	camel_folder_thaw(dest);
	camel_object_unref((CamelObject *)dest);
}

static void transfer_messages_free(struct _mail_msg *mm)
{
	struct _transfer_msg *m = (struct _transfer_msg *)mm;
	int i;

	camel_object_unref((CamelObject *)m->source);
	g_free(m->dest_uri);
	for (i=0;i<m->uids->len;i++)
		g_free(m->uids->pdata[i]);
	g_ptr_array_free(m->uids, TRUE);

}

static struct _mail_msg_op transfer_messages_op = {
	transfer_messages_desc,
	transfer_messages_transfer,
	NULL,
	transfer_messages_free,
};

void
mail_do_transfer_messages (CamelFolder *source, GPtrArray *uids,
			   gboolean delete_from_source,
			   gchar *dest_uri)
{
	struct _transfer_msg *m;

	g_return_if_fail (CAMEL_IS_FOLDER (source));
	g_return_if_fail (uids != NULL);
	g_return_if_fail (dest_uri != NULL);

	m = mail_msg_new(&transfer_messages_op, NULL, sizeof(*m));
	m->source = source;
	camel_object_ref((CamelObject *)source);
	m->uids = uids;
	m->delete = delete_from_source;
	m->dest_uri = g_strdup (dest_uri);

	e_thread_put(mail_thread_queued, (EMsg *)m);
}

/* ** SCAN SUBFOLDERS ***************************************************** */

struct _get_folderinfo_msg {
	struct _mail_msg msg;

	CamelStore *store;
	CamelFolderInfo *info;
	void (*done)(CamelStore *store, CamelFolderInfo *info, void *data);
	void *data;
};

static char *get_folderinfo_desc(struct _mail_msg *mm, int done)
{
	struct _get_folderinfo_msg *m = (struct _get_folderinfo_msg *)mm;
	char *ret, *name;

	name = camel_service_get_name((CamelService *)m->store, TRUE);
	ret = g_strdup_printf(_("Scanning folders in \"%s\""), name);
	g_free(name);
	return ret;
}

static void get_folderinfo_get(struct _mail_msg *mm)
{
	struct _get_folderinfo_msg *m = (struct _get_folderinfo_msg *)mm;

	m->info = camel_store_get_folder_info(m->store, NULL, FALSE, TRUE, TRUE, &mm->ex);
}

static void get_folderinfo_got(struct _mail_msg *mm)
{
	struct _get_folderinfo_msg *m = (struct _get_folderinfo_msg *)mm;

	if (m->done)
		m->done(m->store, m->info, m->data);
}

static void get_folderinfo_free(struct _mail_msg *mm)
{
	struct _get_folderinfo_msg *m = (struct _get_folderinfo_msg *)mm;

	if (m->info)
		camel_store_free_folder_info(m->store, m->info);
	camel_object_unref((CamelObject *)m->store);
}

static struct _mail_msg_op get_folderinfo_op = {
	get_folderinfo_desc,
	get_folderinfo_get,
	get_folderinfo_got,
	get_folderinfo_free,
};

int mail_get_folderinfo(CamelStore *store, void (*done)(CamelStore *store, CamelFolderInfo *info, void *data), void *data)
{
	struct _get_folderinfo_msg *m;
	int id;

	m = mail_msg_new(&get_folderinfo_op, NULL, sizeof(*m));
	m->store = store;
	camel_object_ref((CamelObject *)store);
	m->done = done;
	m->data = data;
	id = m->msg.seq;

	e_thread_put(mail_thread_new, (EMsg *)m);

	return id;
}

/* ********************************************************************** */

static void
do_scan_subfolders (CamelStore *store, CamelFolderInfo *info, void *data)
{
	EvolutionStorage *storage = data;

	if (info) {
		gtk_object_set_data((GtkObject *)storage, "connected", (void *)1);
		mail_storage_create_folder (storage, store, info);
	}
}

/* synchronous function to scan the & and add folders in a store */
void mail_scan_subfolders(CamelStore *store, EvolutionStorage *storage)
{
	int id;

	id = mail_get_folderinfo(store, do_scan_subfolders, storage);
	/*mail_msg_wait(id);*/
}

/* ** ATTACH MESSAGES ****************************************************** */

struct _build_data {
	void (*done)(CamelFolder *folder, GPtrArray *uids, CamelMimePart *part, char *subject, void *data);
	void *data;
};

static void do_build_attachment(CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, void *data)
{
	struct _build_data *d = data;
	CamelMultipart *multipart;
	CamelMimePart *part;
	char *subject;
	int i;

	if (messages->len == 0) {
		d->done(folder, messages, NULL, NULL, d->data);
		g_free(d);
		return;
	}

	if (messages->len == 1) {
		part = mail_tool_make_message_attachment(messages->pdata[0]);
	} else {
		multipart = camel_multipart_new();
		camel_data_wrapper_set_mime_type(CAMEL_DATA_WRAPPER (multipart), "multipart/digest");
		camel_multipart_set_boundary(multipart, NULL);

		for (i=0;i<messages->len;i++) {
			part = mail_tool_make_message_attachment(messages->pdata[i]);
			camel_multipart_add_part(multipart, part);
			camel_object_unref((CamelObject *)part);
		}
		part = camel_mime_part_new();
		camel_medium_set_content_object(CAMEL_MEDIUM (part), CAMEL_DATA_WRAPPER(multipart));
		camel_object_unref((CamelObject *)multipart);

		camel_mime_part_set_description(part, _("Forwarded messages"));
	}

	subject = mail_tool_generate_forward_subject(messages->pdata[0]);
	d->done(folder, messages, part, subject, d->data);
	g_free(subject);
	camel_object_unref((CamelObject *)part);

	g_free(d);
}

void
mail_build_attachment(CamelFolder *folder, GPtrArray *uids,
		      void (*done)(CamelFolder *folder, GPtrArray *messages, CamelMimePart *part, char *subject, void *data), void *data)
{
	struct _build_data *d;

	d = g_malloc(sizeof(*d));
	d->done = done;
	d->data = data;
	mail_get_messages(folder, uids, do_build_attachment, d);
}

/* ** LOAD FOLDER ********************************************************* */

/* there hsould be some way to merge this and create folder, since both can
   presumably create a folder ... */

struct _get_folder_msg {
	struct _mail_msg msg;

	char *uri;
	CamelFolder *folder;
	void (*done) (char *uri, CamelFolder *folder, void *data);
	void *data;
};

static char *get_folder_desc(struct _mail_msg *mm, int done)
{
	struct _get_folder_msg *m = (struct _get_folder_msg *)mm;
	
	return g_strdup_printf(_("Opening folder %s"), m->uri);
}

static void get_folder_get(struct _mail_msg *mm)
{
	struct _get_folder_msg *m = (struct _get_folder_msg *)mm;

	m->folder = mail_tool_uri_to_folder(m->uri, &mm->ex);
}

static void get_folder_got(struct _mail_msg *mm)
{
	struct _get_folder_msg *m = (struct _get_folder_msg *)mm;

	if (m->done)
		m->done(m->uri, m->folder, m->data);
}

static void get_folder_free(struct _mail_msg *mm)
{
	struct _get_folder_msg *m = (struct _get_folder_msg *)mm;

	g_free(m->uri);
	if (m->folder)
		camel_object_unref((CamelObject *)m->folder);
}

static struct _mail_msg_op get_folder_op = {
	get_folder_desc,
	get_folder_get,
	get_folder_got,
	get_folder_free,
};

int
mail_get_folder(const char *uri, void (*done) (char *uri, CamelFolder *folder, void *data), void *data)
{
	struct _get_folder_msg *m;
	int id;

	m = mail_msg_new(&get_folder_op, NULL, sizeof(*m));
	m->uri = g_strdup(uri);
	m->data = data;
	m->done = done;

	id = m->msg.seq;
	e_thread_put(mail_thread_new, (EMsg *)m);
	return id;
}

/* ** GET STORE ******************************************************* */

struct _get_store_msg {
	struct _mail_msg msg;

	char *uri;
	CamelStore *store;
	void (*done) (char *uri, CamelStore *store, void *data);
	void *data;
};

static char *get_store_desc(struct _mail_msg *mm, int done)
{
	struct _get_store_msg *m = (struct _get_store_msg *)mm;
	
	return g_strdup_printf(_("Opening store %s"), m->uri);
}

static void get_store_get(struct _mail_msg *mm)
{
	struct _get_store_msg *m = (struct _get_store_msg *)mm;

	m->store = camel_session_get_store(session, m->uri, &mm->ex);
}

static void get_store_got(struct _mail_msg *mm)
{
	struct _get_store_msg *m = (struct _get_store_msg *)mm;

	if (m->done)
		m->done(m->uri, m->store, m->data);
}

static void get_store_free(struct _mail_msg *mm)
{
	struct _get_store_msg *m = (struct _get_store_msg *)mm;

	g_free(m->uri);
	if (m->store)
		camel_object_unref((CamelObject *)m->store);
}

static struct _mail_msg_op get_store_op = {
	get_store_desc,
	get_store_get,
	get_store_got,
	get_store_free,
};

int
mail_get_store(const char *uri, void (*done) (char *uri, CamelStore *store, void *data), void *data)
{
	struct _get_store_msg *m;
	int id;
	
	m = mail_msg_new(&get_store_op, NULL, sizeof(*m));
	m->uri = g_strdup(uri);
	m->data = data;
	m->done = done;

	id = m->msg.seq;
	e_thread_put(mail_thread_new, (EMsg *)m);
	return id;
}

/* ** CREATE FOLDER ******************************************************* */

/* trying to find a way to remove this entirely and just use get_folder()
   to do the same thing.  But i dont think it can be done, because one works on
   shell uri's (get folder), and the other only works for mail uri's ? */

struct _create_folder_msg {
	struct _mail_msg msg;

	char *uri;
	CamelFolder *folder;
	void (*done) (char *uri, CamelFolder *folder, void *data);
	void *data;
};

static char *create_folder_desc(struct _mail_msg *mm, int done)
{
	struct _create_folder_msg *m = (struct _create_folder_msg *)mm;
	
	return g_strdup_printf(_("Opening folder %s"), m->uri);
}

static void create_folder_get(struct _mail_msg *mm)
{
	struct _create_folder_msg *m = (struct _create_folder_msg *)mm;

	/* FIXME: supply a way to make indexes optional */
	m->folder = mail_tool_get_folder_from_urlname(m->uri, "mbox",
						      CAMEL_STORE_FOLDER_CREATE|CAMEL_STORE_FOLDER_BODY_INDEX,
						      &mm->ex);
}

static void create_folder_got(struct _mail_msg *mm)
{
	struct _create_folder_msg *m = (struct _create_folder_msg *)mm;

	if (m->done)
		m->done(m->uri, m->folder, m->data);
}

static void create_folder_free(struct _mail_msg *mm)
{
	struct _create_folder_msg *m = (struct _create_folder_msg *)mm;

	g_free(m->uri);
	if (m->folder)
		camel_object_unref((CamelObject *)m->folder);
}

static struct _mail_msg_op create_folder_op = {
	create_folder_desc,
	create_folder_get,
	create_folder_got,
	create_folder_free,
};

void
mail_create_folder(const char *uri, void (*done) (char *uri, CamelFolder *folder, void *data), void *data)
{
	struct _create_folder_msg *m;

	m = mail_msg_new(&create_folder_op, NULL, sizeof(*m));
	m->uri = g_strdup(uri);
	m->data = data;
	m->done = done;

	e_thread_put(mail_thread_new, (EMsg *)m);
}

/* ** SYNC FOLDER ********************************************************* */

struct _sync_folder_msg {
	struct _mail_msg msg;

	CamelFolder *folder;
	void (*done) (CamelFolder *folder, void *data);
	void *data;
};

static char *sync_folder_desc(struct _mail_msg *mm, int done)
{
	return g_strdup(_("Synchronising folder"));
}

static void sync_folder_sync(struct _mail_msg *mm)
{
	struct _sync_folder_msg *m = (struct _sync_folder_msg *)mm;

	camel_folder_sync(m->folder, FALSE, &mm->ex);
}

static void sync_folder_synced(struct _mail_msg *mm)
{
	struct _sync_folder_msg *m = (struct _sync_folder_msg *)mm;

	if (m->done)
		m->done(m->folder, m->data);
}

static void sync_folder_free(struct _mail_msg *mm)
{
	struct _sync_folder_msg *m = (struct _sync_folder_msg *)mm;

	camel_object_unref((CamelObject *)m->folder);
}

static struct _mail_msg_op sync_folder_op = {
	sync_folder_desc,
	sync_folder_sync,
	sync_folder_synced,
	sync_folder_free,
};

void
mail_sync_folder(CamelFolder *folder, void (*done) (CamelFolder *folder, void *data), void *data)
{
	struct _sync_folder_msg *m;

	m = mail_msg_new(&sync_folder_op, NULL, sizeof(*m));
	m->folder = folder;
	camel_object_ref((CamelObject *)folder);
	m->data = data;
	m->done = done;

	e_thread_put(mail_thread_new, (EMsg *)m);
}

/* ******************************************************************************** */

static char *expunge_folder_desc(struct _mail_msg *mm, int done)
{
	return g_strdup(_("Expunging folder"));
}

static void expunge_folder_expunge(struct _mail_msg *mm)
{
	struct _sync_folder_msg *m = (struct _sync_folder_msg *)mm;

	camel_folder_expunge(m->folder, &mm->ex);
}

/* we just use the sync stuff where we can, since it would be the same */
static struct _mail_msg_op expunge_folder_op = {
	expunge_folder_desc,
	expunge_folder_expunge,
	sync_folder_synced,
	sync_folder_free,
};

void
mail_expunge_folder(CamelFolder *folder, void (*done) (CamelFolder *folder, void *data), void *data)
{
	struct _sync_folder_msg *m;

	m = mail_msg_new(&expunge_folder_op, NULL, sizeof(*m));
	m->folder = folder;
	camel_object_ref((CamelObject *)folder);
	m->data = data;
	m->done = done;

	e_thread_put(mail_thread_new, (EMsg *)m);
}

/* ** GET MESSAGE(s) ***************************************************** */

struct _get_message_msg {
	struct _mail_msg msg;

	CamelFolder *folder;
	char *uid;
	void (*done) (CamelFolder *folder, char *uid, CamelMimeMessage *msg, void *data);
	void *data;
	CamelMimeMessage *message;
	CamelOperation *cancel;
};

static char *get_message_desc(struct _mail_msg *mm, int done)
{
	struct _get_message_msg *m = (struct _get_message_msg *)mm;

	return g_strdup_printf(_("Retrieving message %s"), m->uid);
}

static void get_message_get(struct _mail_msg *mm)
{
	struct _get_message_msg *m = (struct _get_message_msg *)mm;

	camel_operation_register(m->cancel);
	m->message = camel_folder_get_message(m->folder, m->uid, &mm->ex);
	camel_operation_unregister(m->cancel);
}

static void get_message_got(struct _mail_msg *mm)
{
	struct _get_message_msg *m = (struct _get_message_msg *)mm;

	if (m->done)
		m->done(m->folder, m->uid, m->message, m->data);
}

static void get_message_free(struct _mail_msg *mm)
{
	struct _get_message_msg *m = (struct _get_message_msg *)mm;

	g_free(m->uid);
	camel_object_unref((CamelObject *)m->folder);
	camel_operation_unref(m->cancel);
}

static struct _mail_msg_op get_message_op = {
	get_message_desc,
	get_message_get,
	get_message_got,
	get_message_free,
};

void
mail_get_message(CamelFolder *folder, const char *uid, void (*done) (CamelFolder *folder, char *uid, CamelMimeMessage *msg, void *data), void *data, EThread *thread)
{
	struct _get_message_msg *m;

	m = mail_msg_new(&get_message_op, NULL, sizeof(*m));
	m->folder = folder;
	camel_object_ref((CamelObject *)folder);
	m->uid = g_strdup(uid);
	m->data = data;
	m->done = done;
	m->cancel = camel_operation_new(NULL, NULL);

	e_thread_put(thread, (EMsg *)m);
}

/* ********************************************************************** */

struct _get_messages_msg {
	struct _mail_msg msg;

	CamelFolder *folder;
	GPtrArray *uids;
	GPtrArray *messages;

	void (*done) (CamelFolder *folder, GPtrArray *uids, GPtrArray *msgs, void *data);
	void *data;
};

static char * get_messages_desc(struct _mail_msg *mm, int done)
{
	return g_strdup_printf(_("Retrieving messages"));
}

static void get_messages_get(struct _mail_msg *mm)
{
	struct _get_messages_msg *m = (struct _get_messages_msg *)mm;
	int i;
	CamelMimeMessage *message;

	for (i=0; i<m->uids->len; i++) {
		mail_statusf(_("Retrieving message number %d of %d (uid \"%s\")"),
			     i+1, m->uids->len, (char *) m->uids->pdata[i]);

		message = camel_folder_get_message(m->folder, m->uids->pdata[i], &mm->ex);
		if (message == NULL)
			break;

		g_ptr_array_add(m->messages, message);
	}
}

static void get_messages_got(struct _mail_msg *mm)
{
	struct _get_messages_msg *m = (struct _get_messages_msg *)mm;

	if (m->done)
		m->done(m->folder, m->uids, m->messages, m->data);
}

static void get_messages_free(struct _mail_msg *mm)
{
	struct _get_messages_msg *m = (struct _get_messages_msg *)mm;
	int i;

	for (i=0;i<m->uids->len;i++)
		g_free(m->uids->pdata[i]);
	g_ptr_array_free(m->uids, TRUE);
	for (i=0;i<m->messages->len;i++) {
		if (m->messages->pdata[i])
			camel_object_unref((CamelObject *)m->messages->pdata[i]);
	}
	g_ptr_array_free(m->messages, TRUE);
	camel_object_unref((CamelObject *)m->folder);
}

static struct _mail_msg_op get_messages_op = {
	get_messages_desc,
	get_messages_get,
	get_messages_got,
	get_messages_free,
};

void
mail_get_messages(CamelFolder *folder, GPtrArray *uids,
		  void (*done) (CamelFolder *folder, GPtrArray *uids, GPtrArray *msgs, void *data),
		  void *data)
{
	struct _get_messages_msg *m;

	m = mail_msg_new(&get_messages_op, NULL, sizeof(*m));
	m->folder = folder;
	camel_object_ref((CamelObject *)folder);
	m->uids = uids;
	m->messages = g_ptr_array_new();
	m->data = data;
	m->done = done;

	e_thread_put(mail_thread_new, (EMsg *)m);
}

/* ** SAVE MESSAGES ******************************************************* */

struct _save_messages_msg {
	struct _mail_msg msg;

	CamelFolder *folder;
	GPtrArray *uids;
	char *path;
	void (*done)(CamelFolder *folder, GPtrArray *uids, char *path, void *data);
	void *data;
};

static char *save_messages_desc(struct _mail_msg *mm, int done)
{
	return g_strdup(_("Saving messages"));
}

/* tries to build a From line, based on message headers */
/* this is a copy directly from camel-mbox-summary.c */

static char *tz_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static char *tz_days[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static char *
build_from(struct _header_raw *header)
{
	GString *out = g_string_new("From ");
	char *ret;
	const char *tmp;
	time_t thetime;
	int offset;
	struct tm tm;

	tmp = header_raw_find(&header, "Sender", NULL);
	if (tmp == NULL)
		tmp = header_raw_find(&header, "From", NULL);
	if (tmp != NULL) {
		struct _header_address *addr = header_address_decode(tmp);

		tmp = NULL;
		if (addr) {
			if (addr->type == HEADER_ADDRESS_NAME) {
				g_string_append(out, addr->v.addr);
				tmp = "";
			}
			header_address_unref(addr);
		}
	}
	if (tmp == NULL)
		g_string_append(out, "unknown@nodomain.now.au");

	/* try use the received header to get the date */
	tmp = header_raw_find(&header, "Received", NULL);
	if (tmp) {
		tmp = strrchr(tmp, ';');
		if (tmp)
			tmp++;
	}

	/* if there isn't one, try the Date field */
	if (tmp == NULL)
		tmp = header_raw_find(&header, "Date", NULL);

	thetime = header_decode_date(tmp, &offset);
	thetime += ((offset / 100) * (60 * 60)) + (offset % 100) * 60;
	gmtime_r(&thetime, &tm);
	g_string_sprintfa(out, " %s %s %d %02d:%02d:%02d %4d\n",
			  tz_days[tm.tm_wday],
			  tz_months[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year + 1900);

	ret = out->str;
	g_string_free(out, FALSE);
	return ret;
}

static void save_messages_save(struct _mail_msg *mm)
{
	struct _save_messages_msg *m = (struct _save_messages_msg *)mm;
	CamelStreamFilter *filtered_stream;
	CamelMimeFilterFrom *from_filter;
	CamelStream *stream;
	int fd, i;
	char *from;

	fd = open(m->path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd == -1) {
		camel_exception_setv(&mm->ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Unable to create output file: %s\n %s"), m->path, strerror(errno));
		return;
	}
	
	stream = camel_stream_fs_new_with_fd(fd);
	from_filter = camel_mime_filter_from_new();
	filtered_stream = camel_stream_filter_new_with_stream(stream);
	camel_stream_filter_add(filtered_stream, (CamelMimeFilter *)from_filter);
	camel_object_unref((CamelObject *)from_filter);
	
	for (i=0; i<m->uids->len; i++) {
		CamelMimeMessage *message;

		mail_statusf(_("Saving message %d of %d (uid \"%s\")"),
			     i+1, m->uids->len, (char *)m->uids->pdata[i]);
		
		message = camel_folder_get_message(m->folder, m->uids->pdata[i], &mm->ex);
		if (!message)
			break;

		/* we need to flush after each stream write since we are writing to the same fd */
		from = build_from(((CamelMimePart *)message)->headers);
		if (camel_stream_write_string(stream, from) == -1
		    || camel_stream_flush(stream) == -1
		    || camel_data_wrapper_write_to_stream((CamelDataWrapper *)message, (CamelStream *)filtered_stream) == -1
		    || camel_stream_flush((CamelStream *)filtered_stream) == -1) {
			camel_exception_setv(&mm->ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Error saving messages to: %s:\n %s"), m->path, strerror(errno));
			g_free(from);
			camel_object_unref((CamelObject *)message);
			break;
		}
		g_free(from);
		camel_object_unref((CamelObject *)message);
	}

	camel_object_unref((CamelObject *)filtered_stream);
	camel_object_unref((CamelObject *)stream);
}

static void save_messages_saved(struct _mail_msg *mm)
{
	struct _save_messages_msg *m = (struct _save_messages_msg *)mm;

	if (m->done)
		m->done(m->folder, m->uids, m->path, m->data);
}

static void save_messages_free(struct _mail_msg *mm)
{
	struct _save_messages_msg *m = (struct _save_messages_msg *)mm;
	int i;

	for (i=0;i<m->uids->len;i++)
		g_free(m->uids->pdata[i]);
	g_ptr_array_free(m->uids, TRUE);
	camel_object_unref((CamelObject *)m->folder);
	g_free(m->path);
}

static struct _mail_msg_op save_messages_op = {
	save_messages_desc,
	save_messages_save,
	save_messages_saved,
	save_messages_free,
};

int
mail_save_messages(CamelFolder *folder, GPtrArray *uids, const char *path,
		   void (*done) (CamelFolder *folder, GPtrArray *uids, char *path, void *data), void *data)
{
	struct _save_messages_msg *m;
	int id;

	m = mail_msg_new(&save_messages_op, NULL, sizeof(*m));
	m->folder = folder;
	camel_object_ref((CamelObject *)folder);
	m->uids = uids;
	m->path = g_strdup(path);
	m->data = data;
	m->done = done;

	id = m->msg.seq;
	e_thread_put(mail_thread_new, (EMsg *)m);

	return id;
}

/* ** SAVE PART ******************************************************* */

struct _save_part_msg {
	struct _mail_msg msg;

	CamelMimePart *part;
	char *path;
	void (*done)(CamelMimePart *part, char *path, int saved, void *data);
	void *data;
};

static char *save_part_desc(struct _mail_msg *mm, int done)
{
	return g_strdup(_("Saving attachment"));
}

static void save_part_save(struct _mail_msg *mm)
{
	struct _save_part_msg *m = (struct _save_part_msg *)mm;
	CamelMimeFilterCharset *charsetfilter;
	CamelContentType *content_type;
	CamelStreamFilter *filtered_stream;
	CamelStream *stream_fs;
	CamelDataWrapper *data;
	const char *charset;

	stream_fs = camel_stream_fs_new_with_name(m->path, O_WRONLY|O_CREAT, 0600);
	if (stream_fs == NULL) {
		camel_exception_setv(&mm->ex, 1, _("Cannot create output file: %s:\n %s"), m->path, strerror(errno));
		return;
	}

	/* we only convert text/ parts, and we only convert if we have to
	   null charset param == us-ascii == utf8 always, and utf8 == utf8 obviously */
	/* this will also let "us-ascii that isn't really" parts pass out in
	   proper format, without us trying to treat it as what it isn't, which is
	   the same algorithm camel uses */
	
	data = camel_medium_get_content_object((CamelMedium *)m->part);
	content_type = camel_mime_part_get_content_type(m->part);
	if (header_content_type_is(content_type, "text", "*")
	    && (charset = header_content_type_param(content_type, "charset"))
	    && strcasecmp(charset, "utf-8") != 0) {
		charsetfilter = camel_mime_filter_charset_new_convert("utf-8", charset);
		filtered_stream = camel_stream_filter_new_with_stream(stream_fs);
		camel_stream_filter_add(filtered_stream, CAMEL_MIME_FILTER(charsetfilter));
		camel_object_unref(CAMEL_OBJECT(charsetfilter));
	} else {
		/* no we can't use a CAMEL_BLAH() cast here, since its not true, HOWEVER
		   we only treat it as a normal stream from here on, so it is OK */
		filtered_stream = (CamelStreamFilter *)stream_fs;
		camel_object_ref(CAMEL_OBJECT(stream_fs));
	}
	
	if (camel_data_wrapper_write_to_stream(data, CAMEL_STREAM(filtered_stream)) == -1
	    || camel_stream_flush (CAMEL_STREAM(filtered_stream)) == -1)
		camel_exception_setv(&mm->ex, 1, _("Could not write data: %s"), strerror(errno));

	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (stream_fs));
}

static void save_part_saved(struct _mail_msg *mm)
{
	struct _save_part_msg *m = (struct _save_part_msg *)mm;

	if (m->done)
		m->done(m->part, m->path, !camel_exception_is_set(&mm->ex), m->data);
}

static void save_part_free(struct _mail_msg *mm)
{
	struct _save_part_msg *m = (struct _save_part_msg *)mm;

	camel_object_unref((CamelObject *)m->part);
	g_free(m->path);
}

static struct _mail_msg_op save_part_op = {
	save_part_desc,
	save_part_save,
	save_part_saved,
	save_part_free,
};

int
mail_save_part(CamelMimePart *part, const char *path,
	       void (*done)(CamelMimePart *part, char *path, int saved, void *data), void *data)
{
	struct _save_part_msg *m;
	int id;

	m = mail_msg_new(&save_part_op, NULL, sizeof(*m));
	m->part = part;
	camel_object_ref((CamelObject *)part);
	m->path = g_strdup(path);
	m->data = data;
	m->done = done;

	id = m->msg.seq;
	e_thread_put(mail_thread_queued, (EMsg *)m);

	return id;
}
