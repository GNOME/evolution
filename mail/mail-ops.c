/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-ops.c: callbacks for the mail toolbar/menus */

/* 
 * Authors: Dan Winship <danw@ximian.com>
 *  	    Jeffrey Stedfast <fejj@ximian.com>
 *          Peter Williams <peterw@ximian.com>
 *          Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2000,2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* #include <ctype.h> */
#include <errno.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-exec.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>
#include <gal/util/e-unicode-i18n.h>
#include <camel/camel-mime-filter-from.h>
#include <camel/camel-operation.h>
#include <camel/camel-vtrash-folder.h>
#include <camel/camel-vee-store.h>
#include "mail.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-vfolder.h"
#include "composer/e-msg-composer.h"
#include "folder-browser.h"
#include "e-util/e-html-utils.h"

#include "filter/filter-filter.h"

#include "mail-mt.h"
#include "mail-folder-cache.h"

#define w(x)
#define d(x) 

/* used for both just filtering a folder + uid's, and for filtering a whole folder */
/* used both for fetching mail, and for filtering mail */
struct _filter_mail_msg {
	struct _mail_msg msg;
	
	CamelFolder *source_folder; /* where they come from */
	GPtrArray *source_uids;	/* uids to copy, or NULL == copy all */
	CamelUIDCache *cache;  /* UID cache if we are to cache the uids, NULL otherwise */
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

static char *
filter_folder_describe (struct _mail_msg *mm, int complete)
{
	return g_strdup (_("Filtering Folder"));
}

/* filter a folder, or a subset thereof, uses source_folder/source_uids */
/* this is shared with fetch_mail */
static void
filter_folder_filter (struct _mail_msg *mm)
{
	struct _filter_mail_msg *m = (struct _filter_mail_msg *)mm;
	CamelFolder *folder;
	GPtrArray *uids, *folder_uids = NULL;
	
	if (m->cancel)
		camel_operation_register (m->cancel);
	
	folder = m->source_folder;
	
	if (folder == NULL || camel_folder_get_message_count (folder) == 0) {
		if (m->cancel)
			camel_operation_unregister (m->cancel);
		return;
	}
	
	if (m->destination) {
		camel_folder_freeze (m->destination);
		camel_filter_driver_set_default_folder (m->driver, m->destination);
	}
	
	camel_folder_freeze (folder);
	
	if (m->source_uids)
		uids = m->source_uids;
	else
		folder_uids = uids = camel_folder_get_uids (folder);
	
	camel_filter_driver_filter_folder (m->driver, folder, m->cache, uids, m->delete, &mm->ex);
	camel_filter_driver_flush (m->driver, &mm->ex);
	
	if (folder_uids)
		camel_folder_free_uids (folder, folder_uids);
	
	/* sync our source folder */
	if (!m->cache)
		camel_folder_sync (folder, FALSE, camel_exception_is_set (&mm->ex) ? NULL : &mm->ex);
	camel_folder_thaw (folder);
	
	if (m->destination)
		camel_folder_thaw (m->destination);
	
	if (m->cancel)
		camel_operation_unregister (m->cancel);
}

static void
filter_folder_filtered (struct _mail_msg *mm)
{
}

static void
filter_folder_free (struct _mail_msg *mm)
{
	struct _filter_mail_msg *m = (struct _filter_mail_msg *)mm;
	int i;
	
	if (m->source_folder)
		camel_object_unref (CAMEL_OBJECT (m->source_folder));
	
	if (m->source_uids) {
		for (i = 0; i < m->source_uids->len; i++)
			g_free (m->source_uids->pdata[i]);
		
		g_ptr_array_free (m->source_uids, TRUE);
	}
	
	if (m->cancel)
		camel_operation_unref (m->cancel);
	
	if (m->destination)
		camel_object_unref (CAMEL_OBJECT (m->destination));
	
	if (m->driver)
		camel_object_unref (CAMEL_OBJECT (m->driver));
}

static struct _mail_msg_op filter_folder_op = {
	filter_folder_describe,  /* we do our own progress reporting? */
	filter_folder_filter,
	filter_folder_filtered,
	filter_folder_free,
};

void
mail_filter_folder (CamelFolder *source_folder, GPtrArray *uids,
		    const char *type, gboolean notify,
		    CamelOperation *cancel)
{
	struct _filter_mail_msg *m;
	
	m = mail_msg_new (&filter_folder_op, NULL, sizeof (*m));
	m->source_folder = source_folder;
	camel_object_ref (CAMEL_OBJECT (source_folder));
	m->source_uids = uids;
	m->cache = NULL;
	m->delete = FALSE;
	if (cancel) {
		m->cancel = cancel;
		camel_operation_ref (cancel);
	}
	
	m->driver = camel_session_get_filter_driver (session, type, NULL);
	
	if (!notify) {
		/* FIXME: have a #define NOTIFY_FILTER_NAME macro? */
		/* the filter name has to stay in sync with mail-session::get_filter_driver */
		camel_filter_driver_remove_rule_by_name (m->driver, "new-mail-notification");
	}
	
	e_thread_put (mail_thread_new, (EMsg *)m);
}

/* convenience function for it */
void
mail_filter_on_demand (CamelFolder *folder, GPtrArray *uids)
{
	mail_filter_folder (folder, uids, FILTER_SOURCE_INCOMING, FALSE, NULL);
}

/* ********************************************************************** */

/* Temporary workaround for various issues. Gone before 0.11 */
static char *
uid_cachename_hack (CamelStore *store)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;
	char *encoded_url, *filename, *old_location;
	struct stat st;
	
	encoded_url = g_strdup_printf ("pop://%s%s%s@%s/", url->user,
				       url->authmech ? ";auth=" : "",
				       url->authmech ? url->authmech : "",
				       url->host);
	e_filename_make_safe (encoded_url);
	
	filename = g_strdup_printf ("%s/mail/pop3/cache-%s", evolution_dir, encoded_url);
	
	/* lame hack, but we can't expect user's to actually migrate
           their cache files - brain power requirements are too
           high. */
	if (stat (filename, &st) == -1) {
		/* This is either the first time the user has checked
                   mail with this POP provider or else their cache
                   file is in the old location... */
		old_location = g_strdup_printf ("%s/config/cache-%s", evolution_dir, encoded_url);
		if (stat (old_location, &st) == -1) {
			/* old location doesn't exist either so use the new location */
			g_free (old_location);
		} else {
			/* old location exists, so I guess we use the old cache file location */
			g_free (filename);
			filename = old_location;
		}
	}
	
	g_free (encoded_url);
	
	return filename;
}

static char *
fetch_mail_describe (struct _mail_msg *mm, int complete)
{
	return g_strdup (_("Fetching Mail"));
}

static void
fetch_mail_fetch (struct _mail_msg *mm)
{
	struct _fetch_mail_msg *m = (struct _fetch_mail_msg *)mm;
	struct _filter_mail_msg *fm = (struct _filter_mail_msg *)mm;
	int i;
	
	if (m->cancel)
		camel_operation_register (m->cancel);
	
	if ((fm->destination = mail_tool_get_local_inbox (&mm->ex)) == NULL) {
		if (m->cancel)
			camel_operation_unregister (m->cancel);
		return;
	}
	
	/* FIXME: this should support keep_on_server too, which would then perform a spool
	   access thingy, right?  problem is matching raw messages to uid's etc. */
	if (!strncmp (m->source_uri, "mbox:", 5)) {
		char *path = mail_tool_do_movemail (m->source_uri, &mm->ex);
		
		if (path && !camel_exception_is_set (&mm->ex)) {
			camel_folder_freeze (fm->destination);
			camel_filter_driver_set_default_folder (fm->driver, fm->destination);
			camel_filter_driver_filter_mbox (fm->driver, path, m->source_uri, &mm->ex);
			camel_folder_thaw (fm->destination);
			
			if (!camel_exception_is_set (&mm->ex))
				unlink (path);
		}
		g_free (path);
	} else {
		CamelFolder *folder = fm->source_folder = mail_tool_get_inbox (m->source_uri, &mm->ex);
		
		if (folder) {
			/* this handles 'keep on server' stuff, if we have any new uid's to copy
			   across, we need to copy them to a new array 'cause of the way fetch_mail_free works */
			CamelUIDCache *cache = NULL;
			char *cachename;
			
			cachename = uid_cachename_hack (folder->parent_store);
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
					
					fm->cache = cache;
					filter_folder_filter (mm);
					
					/* if we are not to delete the messages or there was an
					 * exception, save the UID cache */
					if (!fm->delete || camel_exception_is_set (&mm->ex))
						camel_uid_cache_save (cache);
					
					/* if we are deleting off the server and no exception occured
					 * then iterate through the folder uids and mark them all
					 * for deletion. */
					if (fm->delete && !camel_exception_is_set (&mm->ex)) {
						camel_folder_freeze (folder);
						
						for (i = 0; i < folder_uids->len; i++)
							camel_folder_delete_message (folder, folder_uids->pdata[i]);
						
						/* sync and expunge */
						camel_folder_sync (folder, TRUE, &mm->ex);
						
						camel_folder_thaw (folder);
					}
				}
				camel_uid_cache_destroy (cache);
				camel_folder_free_uids (folder, folder_uids);
			} else {
				filter_folder_filter (mm);
			}
			
			/* we unref the source folder here since we
			   may now block in finalize (we try to
			   disconnect cleanly) */
			camel_object_unref (CAMEL_OBJECT (fm->source_folder));
			fm->source_folder = NULL;
		}
	}
	
	if (m->cancel)
		camel_operation_unregister (m->cancel);
	
	/* we unref this here as it may have more work to do (syncing
	   folders and whatnot) before we are really done */
	/* should this be cancellable too? (i.e. above unregister above) */
	camel_object_unref (CAMEL_OBJECT (fm->driver));
	fm->driver = NULL;
}

static void
fetch_mail_fetched (struct _mail_msg *mm)
{
	struct _fetch_mail_msg *m = (struct _fetch_mail_msg *)mm;
	
	if (m->done)
		m->done (m->source_uri, m->data);
}

static void
fetch_mail_free (struct _mail_msg *mm)
{
	struct _fetch_mail_msg *m = (struct _fetch_mail_msg *)mm;
	
	g_free (m->source_uri);
	if (m->cancel)
		camel_operation_unref (m->cancel);
	
	filter_folder_free (mm);
}

static struct _mail_msg_op fetch_mail_op = {
	fetch_mail_describe, /* we do our own progress reporting */
	fetch_mail_fetch,
	fetch_mail_fetched,
	fetch_mail_free,
};

/* ouch, a 'do everything' interface ... */
void
mail_fetch_mail (const char *source, int keep, const char *type, CamelOperation *cancel,
		 CamelFilterGetFolderFunc get_folder, void *get_data,
		 CamelFilterStatusFunc *status, void *status_data,
		 void (*done)(char *source, void *data), void *data)
{
	struct _fetch_mail_msg *m;
	struct _filter_mail_msg *fm;
	
	m = mail_msg_new (&fetch_mail_op, NULL, sizeof (*m));
	fm = (struct _filter_mail_msg *)m;
	m->source_uri = g_strdup (source);
	fm->delete = !keep;
	fm->cache = NULL;
	if (cancel) {
		m->cancel = cancel;
		camel_operation_ref (cancel);
	}
	m->done = done;
	m->data = data;
	
	fm->driver = camel_session_get_filter_driver (session, type, NULL);
	camel_filter_driver_set_folder_func (fm->driver, get_folder, get_data);
	if (status)
		camel_filter_driver_set_status_func (fm->driver, status, status_data);
	
	e_thread_put (mail_thread_new, (EMsg *)m);
}

/* ********************************************************************** */
/* sending stuff */
/* ** SEND MAIL *********************************************************** */

extern CamelFolder *sent_folder;

static char *normal_recipients[] = {
	CAMEL_RECIPIENT_TYPE_TO,
	CAMEL_RECIPIENT_TYPE_CC,
	CAMEL_RECIPIENT_TYPE_BCC
};

static char *resent_recipients[] = {
	CAMEL_RECIPIENT_TYPE_RESENT_TO,
	CAMEL_RECIPIENT_TYPE_RESENT_CC,
	CAMEL_RECIPIENT_TYPE_RESENT_BCC
};

/* send 1 message to a specific transport */
static void
mail_send_message (CamelMimeMessage *message, const char *destination,
		   CamelFilterDriver *driver, CamelException *ex)
{
	const CamelInternetAddress *iaddr;
	CamelAddress *from, *recipients;
	CamelMessageInfo *info;
	CamelTransport *xport = NULL;
	char *transport_url = NULL;
	char *sent_folder_uri = NULL;
	const char *resent_from;
	CamelFolder *folder;
	XEvolution *xev;
	int i;
	
	camel_medium_set_header (CAMEL_MEDIUM (message), "X-Mailer",
				 "Evolution/" VERSION SUB_VERSION " " VERSION_COMMENT);
	
	camel_mime_message_set_date (message, CAMEL_MESSAGE_DATE_CURRENT, 0);
	
	xev = mail_tool_remove_xevolution_headers (message);
	
	if (xev->account) {
		const MailConfigAccount *account;
		char *name;
		
		name = g_strstrip (g_strdup (xev->account));
		account = mail_config_get_account_by_name (name);
		g_free (name);
		
		if (account && account->transport && account->transport->url)
			transport_url = g_strdup (account->transport->url);
	} else if (xev->transport) {
		transport_url = g_strstrip (g_strdup (xev->transport));
	}
	
	if (xev->fcc)
		sent_folder_uri = g_strstrip (g_strdup (xev->fcc));
	
	xport = camel_session_get_transport (session, transport_url ? transport_url : destination, ex);
	g_free (transport_url);
	if (!xport) {
		mail_tool_restore_xevolution_headers (message, xev);
		mail_tool_destroy_xevolution (xev);
		g_free (sent_folder_uri);
		return;
	}
	
	from = (CamelAddress *) camel_internet_address_new ();
	resent_from = camel_medium_get_header (CAMEL_MEDIUM (message), "Resent-From");
	if (resent_from) {
		camel_address_decode (from, resent_from);
	} else {
		iaddr = camel_mime_message_get_from (message);
		camel_address_copy (from, CAMEL_ADDRESS (iaddr));
	}
	
	recipients = (CamelAddress *) camel_internet_address_new ();
	for (i = 0; i < 3; i++) {
		const char *type;
		
		type = resent_from ? resent_recipients[i] : normal_recipients[i];
		iaddr = camel_mime_message_get_recipients (message, type);
		camel_address_cat (recipients, CAMEL_ADDRESS (iaddr));
	}
	
	camel_transport_send_to (xport, CAMEL_MEDIUM (message), from, recipients, ex);
	camel_object_unref (CAMEL_OBJECT (recipients));
	camel_object_unref (CAMEL_OBJECT (from));
	
	mail_tool_restore_xevolution_headers (message, xev);
	mail_tool_destroy_xevolution (xev);
	
	camel_object_unref (CAMEL_OBJECT (xport));
	if (camel_exception_is_set (ex)) {
		g_free (sent_folder_uri);
		return;
	}
	
	/* post-process */
	info = camel_message_info_new ();
	info->flags = CAMEL_MESSAGE_SEEN;
	
	if (driver) {
		camel_filter_driver_filter_message (driver, message, info,
						    NULL, NULL, NULL, "", ex);
		
		if (camel_exception_is_set (ex)) {
			ExceptionId id;
			
			id = camel_exception_get_id (ex);
			camel_exception_setv (ex, id, "%s\n%s", camel_exception_get_description (ex),
					      _("However, the message was successfully sent."));
			
			camel_message_info_free (info);
			g_free (sent_folder_uri);
			
			return;
		}
	}
	
	if (sent_folder_uri) {
		folder = mail_tool_uri_to_folder (sent_folder_uri, 0, NULL);
		g_free (sent_folder_uri);
		if (!folder) {
			/* FIXME */
			camel_object_ref (CAMEL_OBJECT (sent_folder));
			folder = sent_folder;
		}
	} else {
		camel_object_ref (CAMEL_OBJECT (sent_folder));
		folder = sent_folder;
	}
	
	if (folder) {
		camel_folder_append_message (folder, message, info, ex);
		if (camel_exception_is_set (ex)) {
			ExceptionId id;
			
			id = camel_exception_get_id (ex);
			camel_exception_setv (ex, id, "%s\n%s", camel_exception_get_description (ex),
					      _("However, the message was successfully sent."));
		}
		
		camel_folder_sync (folder, FALSE, NULL);
		camel_object_unref (CAMEL_OBJECT (folder));
	}
	
	camel_message_info_free (info);
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

static char *
send_mail_desc (struct _mail_msg *mm, int done)
{
	struct _send_mail_msg *m = (struct _send_mail_msg *)mm;
	char *subject;
	const char *subject_utf8;
	
	subject_utf8 = camel_mime_message_get_subject (m->message);
	
	if (subject_utf8) {
		char *desc;
		
		subject = e_utf8_to_locale_string (subject_utf8);
		desc = g_strdup_printf (_("Sending \"%s\""), subject);
		g_free (subject);
		return desc;
	} else
		return g_strdup (_("Sending message"));
}

static void
send_mail_send (struct _mail_msg *mm)
{
	struct _send_mail_msg *m = (struct _send_mail_msg *)mm;
	
	mail_send_message (m->message, m->destination, m->driver, &mm->ex);
}

static void
send_mail_sent (struct _mail_msg *mm)
{
	struct _send_mail_msg *m = (struct _send_mail_msg *)mm;
	
	if (m->done)
		m->done (m->destination, m->message, !camel_exception_is_set (&mm->ex), m->data);
}

static void
send_mail_free (struct _mail_msg *mm)
{
	struct _send_mail_msg *m = (struct _send_mail_msg *)mm;

	camel_object_unref (CAMEL_OBJECT (m->driver));
	camel_object_unref (CAMEL_OBJECT (m->message));
	g_free (m->destination);
}

static struct _mail_msg_op send_mail_op = {
	send_mail_desc,
	send_mail_send,
	send_mail_sent,
	send_mail_free,
};

int
mail_send_mail (const char *uri, CamelMimeMessage *message,
		void (*done) (char *uri, CamelMimeMessage *message, gboolean sent, void *data),
		void *data)
{
	struct _send_mail_msg *m;
	int id;
	
	m = mail_msg_new (&send_mail_op, NULL, sizeof (*m));
	m->destination = g_strdup (uri);
	m->message = message;
	camel_object_ref (CAMEL_OBJECT (message));
	m->data = data;
	m->done = done;
	
	id = m->msg.seq;
	
	m->driver = camel_session_get_filter_driver (session, FILTER_SOURCE_OUTGOING, NULL);
	
	e_thread_put (mail_thread_new, (EMsg *)m);
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
report_status (struct _send_queue_msg *m, enum camel_filter_status_t status, int pc, const char *desc, ...)
{
	va_list ap;
	char *str;
	
	if (m->status) {
		va_start (ap, desc);
		str = g_strdup_vprintf (desc, ap);
		va_end (ap);
		m->status (m->driver, status, pc, str, m->status_data);
		g_free (str);
	}
}

static void
send_queue_send(struct _mail_msg *mm)
{
	struct _send_queue_msg *m = (struct _send_queue_msg *)mm;
	extern CamelFolder *sent_folder; /* FIXME */
	GPtrArray *uids;
	int i;
	
	d(printf("sending queue\n"));
	
	uids = camel_folder_get_uids (m->queue);
	if (uids == NULL || uids->len == 0)
		return;

	if (m->cancel)
		camel_operation_register (m->cancel);
	
	for (i = 0; i < uids->len; i++) {
		CamelMimeMessage *message;
		CamelMessageInfo *info;
		int pc = (100 * i) / uids->len;
		
		report_status (m, CAMEL_FILTER_STATUS_START, pc, _("Sending message %d of %d"), i+1, uids->len);
		
		info = camel_folder_get_message_info (m->queue, uids->pdata[i]);
		if (info && info->flags & CAMEL_MESSAGE_DELETED)
			continue;
		
		message = camel_folder_get_message (m->queue, uids->pdata[i], &mm->ex);
		if (camel_exception_is_set (&mm->ex))
			break;
		
		mail_send_message (message, m->destination, m->driver, &mm->ex);
		
		if (camel_exception_is_set (&mm->ex))
			break;
		
		camel_folder_set_message_flags (m->queue, uids->pdata[i], CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);
	}

	if (camel_exception_is_set (&mm->ex))
		report_status (m, CAMEL_FILTER_STATUS_END, 100, _("Failed on message %d of %d"), i+1, uids->len);
	else
		report_status (m, CAMEL_FILTER_STATUS_END, 100, _("Complete."));

	if (m->driver) {
		camel_object_unref((CamelObject *)m->driver);
		m->driver = NULL;
	}
		
	camel_folder_free_uids (m->queue, uids);
	
	if (!camel_exception_is_set (&mm->ex))
		camel_folder_expunge (m->queue, &mm->ex);
	
	if (sent_folder)
		camel_folder_sync (sent_folder, FALSE, NULL);
	
	if (m->cancel)
		camel_operation_unregister (m->cancel);
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
	
	if (m->driver)
		camel_object_unref((CamelObject *)m->driver);
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
		const char *type, CamelOperation *cancel,
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

	m->driver = camel_session_get_filter_driver (session, type, NULL);
	camel_filter_driver_set_folder_func (m->driver, get_folder, get_data);

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

static char *
append_mail_desc (struct _mail_msg *mm, int done)
{
	return g_strdup (_("Saving message to folder"));
}

static void
append_mail_append (struct _mail_msg *mm)
{
	struct _append_msg *m = (struct _append_msg *)mm;

	camel_mime_message_set_date(m->message, CAMEL_MESSAGE_DATE_CURRENT, 0);
	camel_folder_append_message(m->folder, m->message, m->info, &mm->ex);
}

static void
append_mail_appended (struct _mail_msg *mm)
{
	struct _append_msg *m = (struct _append_msg *)mm;

	if (m->done)
		m->done(m->folder, m->message, m->info, !camel_exception_is_set(&mm->ex), m->data);
}

static void
append_mail_free (struct _mail_msg *mm)
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
mail_append_mail (CamelFolder *folder, CamelMimeMessage *message, CamelMessageInfo *info,
		  void (*done)(CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info, int ok, void *data),
		  void *data)
{
	struct _append_msg *m;
	
	g_assert(CAMEL_IS_FOLDER (folder));
	g_assert(CAMEL_IS_MIME_MESSAGE (message));
	
	m = mail_msg_new (&append_mail_op, NULL, sizeof (*m));
	m->folder = folder;
	camel_object_ref (CAMEL_OBJECT (folder));
	m->message = message;
	camel_object_ref (CAMEL_OBJECT (message));
	m->info = info;
	
	m->done = done;
	m->data = data;
	
	e_thread_put (mail_thread_new, (EMsg *)m);
}

/* ** TRANSFER MESSAGES **************************************************** */

struct _transfer_msg {
	struct _mail_msg msg;

	CamelFolder *source;
	GPtrArray *uids;
	gboolean delete;
	char *dest_uri;
	guint32 dest_flags;
	
	void (*done)(gboolean ok, void *data);
	void *data;
};

static char *
transfer_messages_desc (struct _mail_msg *mm, int done)
{
	struct _transfer_msg *m = (struct _transfer_msg *)mm;

	return g_strdup_printf(m->delete?_("Moving messages to %s"):_("Copying messages to %s"),
			       m->dest_uri);
			       
}

static void
transfer_messages_transfer (struct _mail_msg *mm)
{
	struct _transfer_msg *m = (struct _transfer_msg *)mm;
	CamelFolder *dest;
	char *desc;
	void (*func) (CamelFolder *, GPtrArray *, 
		      CamelFolder *, 
		      CamelException *);

	dest = mail_tool_uri_to_folder (m->dest_uri, m->dest_flags, &mm->ex);
	if (camel_exception_is_set (&mm->ex))
		return;

	if (dest == m->source) {
		camel_object_unref((CamelObject *)dest);
		/* no-op */
		return;
	}

	if (m->delete) {
		func = camel_folder_move_messages_to;
		desc = _("Moving");
	} else {
		func = camel_folder_copy_messages_to;
		desc = _("Copying");
	}
	
	camel_folder_freeze (m->source);
	camel_folder_freeze (dest);
	
	if (CAMEL_IS_VTRASH_FOLDER (dest)) {
		if (m->delete) {
			int i;
			
			/* Just mark all the messages as deleted */
			for (i = 0; i < m->uids->len; i++)
				camel_folder_delete_message (m->source, m->uids->pdata[i]);
		} else {
			/* no-op - can't copy messages to*/
		}
	} else {
		if (dest == m->source) {
			int i;
			
			/* Undelete the messages if they are marked as deleted */
			for (i = 0; i < m->uids->len; i++)
				camel_folder_set_message_flags (m->source, m->uids->pdata[i],
								CAMEL_MESSAGE_DELETED, 0);
		} else {
			(func) (m->source, m->uids, dest, &mm->ex);
		}
	}
	
	camel_folder_thaw (m->source);
	camel_folder_thaw (dest);
	camel_folder_sync (dest, FALSE, NULL);
	camel_object_unref (CAMEL_OBJECT (dest));
}

static void
transfer_messages_transferred (struct _mail_msg *mm)
{
	struct _transfer_msg *m = (struct _transfer_msg *)mm;
	
	if (m->done)
		m->done (!camel_exception_is_set (&mm->ex), m->data);
}

static void
transfer_messages_free (struct _mail_msg *mm)
{
	struct _transfer_msg *m = (struct _transfer_msg *)mm;
	int i;

	camel_object_unref (CAMEL_OBJECT (m->source));
	g_free (m->dest_uri);
	for (i = 0; i < m->uids->len; i++)
		g_free (m->uids->pdata[i]);
	g_ptr_array_free (m->uids, TRUE);

}

static struct _mail_msg_op transfer_messages_op = {
	transfer_messages_desc,
	transfer_messages_transfer,
	transfer_messages_transferred,
	transfer_messages_free,
};

void
mail_transfer_messages (CamelFolder *source, GPtrArray *uids,
			gboolean delete_from_source,
			const char *dest_uri,
			guint32 dest_flags,
			void (*done) (gboolean ok, void *data),
			void *data)
{
	struct _transfer_msg *m;
	
	g_assert(CAMEL_IS_FOLDER (source));
	g_assert(uids != NULL);
	g_assert(dest_uri != NULL);
	
	m = mail_msg_new(&transfer_messages_op, NULL, sizeof(*m));
	m->source = source;
	camel_object_ref (CAMEL_OBJECT (source));
	m->uids = uids;
	m->delete = delete_from_source;
	m->dest_uri = g_strdup (dest_uri);
	m->dest_flags = dest_flags;
	m->done = done;
	m->data = data;
	
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

static char *
get_folderinfo_desc (struct _mail_msg *mm, int done)
{
	struct _get_folderinfo_msg *m = (struct _get_folderinfo_msg *)mm;
	char *ret, *name;

	name = camel_service_get_name((CamelService *)m->store, TRUE);
	ret = g_strdup_printf(_("Scanning folders in \"%s\""), name);
	g_free(name);
	return ret;
}

static void
add_vtrash_info (CamelStore *store, CamelFolderInfo *info)
{
	CamelFolderInfo *fi, *vtrash, *parent;
	char *uri, *path;
	CamelURL *url;
	
	g_return_if_fail (info != NULL);

	parent = NULL;
	for (fi = info; fi; fi = fi->sibling) {
		if (!strcmp (fi->name, CAMEL_VTRASH_NAME))
			break;
		parent = fi;
	}
	
	/* create our vTrash URL */
	url = camel_url_new (info->url, NULL);
	path = g_strdup_printf ("/%s", CAMEL_VTRASH_NAME);
	if (url->fragment)
		camel_url_set_fragment (url, path);
	else
		camel_url_set_path (url, path);
	g_free (path);
	uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
	camel_url_free (url);
	
	if (fi) {
		/* We're going to replace the physical Trash folder with our vTrash folder */
		vtrash = fi;
		g_free (vtrash->full_name);
		g_free (vtrash->name);
		g_free (vtrash->url);
	} else {
		/* There wasn't a Trash folder so create a new folder entry */
		vtrash = g_new0 (CamelFolderInfo, 1);

		g_assert(parent != NULL);

		/* link it into the right spot */
		vtrash->sibling = parent->sibling;
		parent->sibling = vtrash;
	}
	
	/* Fill in the new fields */
	vtrash->full_name = g_strdup (U_("Trash"));
	vtrash->name = g_strdup(vtrash->full_name);
	vtrash->url = g_strdup_printf ("vtrash:%s", uri);
	vtrash->unread_message_count = -1;
	vtrash->path = g_strdup_printf("/%s", vtrash->name);
	g_free (uri);
}

static void
add_unmatched_info(CamelFolderInfo *fi)
{
	for (; fi; fi = fi->sibling) {
		if (!strcmp(fi->full_name, CAMEL_UNMATCHED_NAME)) {
			g_free(fi->name);
			fi->name = g_strdup(U_("Unmatched"));
			g_free(fi->path);
			fi->path = g_strdup_printf("/%s", fi->name);
			break;
		}
	}
}

static void
get_folderinfo_get (struct _mail_msg *mm)
{
	struct _get_folderinfo_msg *m = (struct _get_folderinfo_msg *)mm;
	guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE;
	
	if (camel_store_supports_subscriptions (m->store))
		flags |= CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
	
	m->info = camel_store_get_folder_info (m->store, NULL, flags, &mm->ex);
	if (m->info) {
		if (m->info->url && (m->store->flags & CAMEL_STORE_VTRASH))
			add_vtrash_info(m->store, m->info);
		if (CAMEL_IS_VEE_STORE(m->store))
			add_unmatched_info(m->info);
	}
}

static void
get_folderinfo_got (struct _mail_msg *mm)
{
	struct _get_folderinfo_msg *m = (struct _get_folderinfo_msg *)mm;
	
	if (camel_exception_is_set (&mm->ex)) {
		char *url;
		
		url = camel_service_get_url (CAMEL_SERVICE (m->store));
		w(g_warning ("Error getting folder info from store at %s: %s",
			     url, camel_exception_get_description (&mm->ex)));
		g_free (url);
	}
	
	/* 'done' is probably guaranteed to fail, but... */
	
	if (m->done)
		m->done (m->store, m->info, m->data);
}

static void
get_folderinfo_free (struct _mail_msg *mm)
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

int
mail_get_folderinfo (CamelStore *store, void (*done)(CamelStore *store, CamelFolderInfo *info, void *data), void *data)
{
	struct _get_folderinfo_msg *m;
	int id;

	m = mail_msg_new(&get_folderinfo_op, NULL, sizeof(*m));
	m->store = store;
	camel_object_ref((CamelObject *)store);
	m->done = done;
	m->data = data;
	id = m->msg.seq;

	e_thread_put(mail_thread_queued, (EMsg *)m);

	return id;
}

/* ** ATTACH MESSAGES ****************************************************** */

struct _build_data {
	void (*done)(CamelFolder *folder, GPtrArray *uids, CamelMimePart *part, char *subject, void *data);
	void *data;
};

static void
do_build_attachment (CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, void *data)
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
	guint32 flags;
	CamelFolder *folder;
	void (*done) (char *uri, CamelFolder *folder, void *data);
	void *data;
};

static char *
get_folder_desc (struct _mail_msg *mm, int done)
{
	struct _get_folder_msg *m = (struct _get_folder_msg *)mm;
	
	return g_strdup_printf(_("Opening folder %s"), m->uri);
}

static void
get_folder_get (struct _mail_msg *mm)
{
	struct _get_folder_msg *m = (struct _get_folder_msg *)mm;
	
	m->folder = mail_tool_uri_to_folder (m->uri, m->flags, &mm->ex);
}

static void
get_folder_got (struct _mail_msg *mm)
{
	struct _get_folder_msg *m = (struct _get_folder_msg *)mm;
	
	if (m->done)
		m->done (m->uri, m->folder, m->data);
}

static void
get_folder_free (struct _mail_msg *mm)
{
	struct _get_folder_msg *m = (struct _get_folder_msg *)mm;
	
	g_free (m->uri);
	if (m->folder)
		camel_object_unref (CAMEL_OBJECT (m->folder));
}

static struct _mail_msg_op get_folder_op = {
	get_folder_desc,
	get_folder_get,
	get_folder_got,
	get_folder_free,
};

int
mail_get_folder (const char *uri, guint32 flags,
		 void (*done)(char *uri, CamelFolder *folder, void *data),
		 void *data, EThread *thread)
{
	struct _get_folder_msg *m;
	int id;
	
	m = mail_msg_new(&get_folder_op, NULL, sizeof(*m));
	m->uri = g_strdup (uri);
	m->flags = flags;
	m->data = data;
	m->done = done;
	
	id = m->msg.seq;
	e_thread_put(thread, (EMsg *)m);
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

static char *
get_store_desc (struct _mail_msg *mm, int done)
{
	struct _get_store_msg *m = (struct _get_store_msg *)mm;
	
	return g_strdup_printf(_("Opening store %s"), m->uri);
}

static void
get_store_get (struct _mail_msg *mm)
{
	struct _get_store_msg *m = (struct _get_store_msg *)mm;
	
	m->store = camel_session_get_store (session, m->uri, &mm->ex);
}

static void
get_store_got (struct _mail_msg *mm)
{
	struct _get_store_msg *m = (struct _get_store_msg *)mm;

	if (m->done)
		m->done (m->uri, m->store, m->data);
}

static void
get_store_free (struct _mail_msg *mm)
{
	struct _get_store_msg *m = (struct _get_store_msg *)mm;
	
	g_free (m->uri);
	if (m->store)
		camel_object_unref (CAMEL_OBJECT (m->store));
}

static struct _mail_msg_op get_store_op = {
	get_store_desc,
	get_store_get,
	get_store_got,
	get_store_free,
};

int
mail_get_store (const char *uri, void (*done) (char *uri, CamelStore *store, void *data), void *data)
{
	struct _get_store_msg *m;
	int id;
	
	m = mail_msg_new (&get_store_op, NULL, sizeof (*m));
	m->uri = g_strdup (uri);
	m->data = data;
	m->done = done;
	
	id = m->msg.seq;
	e_thread_put (mail_thread_new, (EMsg *)m);
	return id;
}

/* ** REMOVE FOLDER ******************************************************* */

struct _remove_folder_msg {
	struct _mail_msg msg;

	char *uri;
	gboolean removed;
	void (*done) (char *uri, gboolean removed, void *data);
	void *data;
};

static char *
remove_folder_desc (struct _mail_msg *mm, int done)
{
	struct _remove_folder_msg *m = (struct _remove_folder_msg *)mm;
	
	return g_strdup_printf (_("Removing folder %s"), m->uri);
}

static void
remove_folder_get (struct _mail_msg *mm)
{
	struct _remove_folder_msg *m = (struct _remove_folder_msg *)mm;
	CamelStore *store;
	CamelFolder *folder;
	GPtrArray *uids;
	int i;
	
	m->removed = FALSE;
	
	folder = mail_tool_uri_to_folder (m->uri, 0, &mm->ex);
	if (!folder)
		return;
	
	store = folder->parent_store;
	
	/* Delete every message in this folder, then expunge it */
	uids = camel_folder_get_uids (folder);
	camel_folder_freeze(folder);
	for (i = 0; i < uids->len; i++)
		camel_folder_delete_message (folder, uids->pdata[i]);
	camel_folder_sync (folder, TRUE, NULL);
	camel_folder_thaw(folder);
	camel_folder_free_uids (folder, uids);
	
	/* if the store supports subscriptions, unsubscribe from this folder... */
	if (camel_store_supports_subscriptions (store))
		camel_store_unsubscribe_folder (store, folder->full_name, NULL);
	
	/* Then delete the folder from the store */
	camel_store_delete_folder (store, folder->full_name, &mm->ex);
	m->removed = !camel_exception_is_set (&mm->ex);
	camel_object_unref (CAMEL_OBJECT (folder));
}

static void
remove_folder_got (struct _mail_msg *mm)
{
	struct _remove_folder_msg *m = (struct _remove_folder_msg *)mm;

	if (m->removed) {
		/* FIXME: Remove this folder from the folder cache ??? */
	}

	if (m->done)
		m->done (m->uri, m->removed, m->data);
}

static void
remove_folder_free (struct _mail_msg *mm)
{
	struct _remove_folder_msg *m = (struct _remove_folder_msg *)mm;
	
	g_free (m->uri);
}

static struct _mail_msg_op remove_folder_op = {
	remove_folder_desc,
	remove_folder_get,
	remove_folder_got,
	remove_folder_free,
};

void
mail_remove_folder (const char *uri, void (*done) (char *uri, gboolean removed, void *data), void *data)
{
	struct _remove_folder_msg *m;
	
	m = mail_msg_new (&remove_folder_op, NULL, sizeof (*m));
	m->uri = g_strdup (uri);
	m->data = data;
	m->done = done;
	
	e_thread_put (mail_thread_new, (EMsg *)m);
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
	struct _sync_folder_msg *m = (struct _sync_folder_msg *)mm;

	return g_strdup_printf (_("Storing folder \'%s\'"), 
			       camel_folder_get_full_name (m->folder));
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

	e_thread_put(mail_thread_queued, (EMsg *)m);
}

/* ******************************************************************************** */

static char *refresh_folder_desc(struct _mail_msg *mm, int done)
{
	return g_strdup(_("Refreshing folder"));
}

static void refresh_folder_refresh(struct _mail_msg *mm)
{
	struct _sync_folder_msg *m = (struct _sync_folder_msg *)mm;

	camel_folder_refresh_info(m->folder, &mm->ex);
}

/* we just use the sync stuff where we can, since it would be the same */
static struct _mail_msg_op refresh_folder_op = {
	refresh_folder_desc,
	refresh_folder_refresh,
	sync_folder_synced,
	sync_folder_free,
};

void
mail_refresh_folder(CamelFolder *folder, void (*done) (CamelFolder *folder, void *data), void *data)
{
	struct _sync_folder_msg *m;

	m = mail_msg_new(&refresh_folder_op, NULL, sizeof(*m));
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

	e_thread_put(mail_thread_queued, (EMsg *)m);
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

	m->message = camel_folder_get_message(m->folder, m->uid, &mm->ex);
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
	struct _get_messages_msg *m = (struct _get_messages_msg *)mm;

	return g_strdup_printf(_("Retrieving %d message(s)"), m->uids->len);
}

static void get_messages_get(struct _mail_msg *mm)
{
	struct _get_messages_msg *m = (struct _get_messages_msg *)mm;
	int i;
	CamelMimeMessage *message;

	for (i=0; i<m->uids->len; i++) {
		int pc = ((i+1) * 100) / m->uids->len;

		message = camel_folder_get_message(m->folder, m->uids->pdata[i], &mm->ex);
		camel_operation_progress(mm->cancel, pc);
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
	struct _save_messages_msg *m = (struct _save_messages_msg *)mm;

	return g_strdup_printf(_("Saving %d messsage(s)"), m->uids->len);
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

static void
save_prepare_part (CamelMimePart *mime_part)
{
	CamelDataWrapper *wrapper;
	int parts, i;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	if (!wrapper)
		return;
	
	if (CAMEL_IS_MULTIPART (wrapper)) {
		parts = camel_multipart_get_number (CAMEL_MULTIPART (wrapper));
		for (i = 0; i < parts; i++) {
			CamelMimePart *part = camel_multipart_get_part (CAMEL_MULTIPART (wrapper), i);
			
			save_prepare_part (part);
		}
	} else {
		if (CAMEL_IS_MIME_MESSAGE (wrapper)) {
			/* prepare the message parts' subparts */
			save_prepare_part (CAMEL_MIME_PART (wrapper));
		} else {
			CamelContentType *type;
			
			/* We want to save textual parts as 8bit instead of encoded */
			type = camel_data_wrapper_get_mime_type_field (wrapper);
			if (header_content_type_is (type, "text", "*"))
				camel_mime_part_set_encoding (mime_part, CAMEL_MIME_PART_ENCODING_8BIT);
		}
	}
}

static void
save_messages_save (struct _mail_msg *mm)
{
	struct _save_messages_msg *m = (struct _save_messages_msg *)mm;
	CamelStreamFilter *filtered_stream;
	CamelMimeFilterFrom *from_filter;
	CamelStream *stream;
	int fd, i;
	char *from;
	
	fd = open (m->path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
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
		int pc = ((i+1) * 100) / m->uids->len;

		message = camel_folder_get_message(m->folder, m->uids->pdata[i], &mm->ex);
		camel_operation_progress(mm->cancel, pc);
		if (message == NULL)
			break;
		
		save_prepare_part (CAMEL_MIME_PART (message));
		
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

static void
save_part_save (struct _mail_msg *mm)
{
	struct _save_part_msg *m = (struct _save_part_msg *)mm;
	CamelMimeFilterCharset *charsetfilter;
	CamelContentType *content_type;
	CamelStreamFilter *filtered_stream;
	CamelStream *stream_fs;
	CamelDataWrapper *data;
	const char *charset;
	
	stream_fs = camel_stream_fs_new_with_name (m->path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (stream_fs == NULL) {
		camel_exception_setv (&mm->ex, 1, _("Cannot create output file: %s:\n %s"), m->path,
				      g_strerror (errno));
		return;
	}
	
	/* we only convert text/ parts, and we only convert if we have to
	   null charset param == us-ascii == utf8 always, and utf8 == utf8 obviously */
	/* this will also let "us-ascii that isn't really" parts pass out in
	   proper format, without us trying to treat it as what it isn't, which is
	   the same algorithm camel uses */
	
	data = camel_medium_get_content_object (CAMEL_MEDIUM (m->part));
	content_type = camel_mime_part_get_content_type (m->part);
	if (header_content_type_is (content_type, "text", "*")
	    && (charset = header_content_type_param (content_type, "charset"))
	    && g_strcasecmp (charset, "utf-8") != 0) {
		charsetfilter = camel_mime_filter_charset_new_convert ("utf-8", charset);
		filtered_stream = camel_stream_filter_new_with_stream (stream_fs);
		if (charsetfilter) {
			camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (charsetfilter));
			camel_object_unref (CAMEL_OBJECT (charsetfilter));
		}
	} else {
		/* no we can't use a CAMEL_BLAH() cast here, since its not true, HOWEVER
		   we only treat it as a normal stream from here on, so it is OK */
		filtered_stream = (CamelStreamFilter *)stream_fs;
		camel_object_ref (CAMEL_OBJECT (stream_fs));
	}
	
	if (camel_data_wrapper_write_to_stream (data, CAMEL_STREAM (filtered_stream)) == -1
	    || camel_stream_flush (CAMEL_STREAM (filtered_stream)) == -1)
		camel_exception_setv (&mm->ex, 1, _("Could not write data: %s"), g_strerror (errno));
	
	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (stream_fs));
}

static void
save_part_saved (struct _mail_msg *mm)
{
	struct _save_part_msg *m = (struct _save_part_msg *)mm;
	
	if (m->done)
		m->done (m->part, m->path, !camel_exception_is_set (&mm->ex), m->data);
}

static void
save_part_free (struct _mail_msg *mm)
{
	struct _save_part_msg *m = (struct _save_part_msg *)mm;

	camel_object_unref (CAMEL_OBJECT (m->part));
	g_free (m->path);
}

static struct _mail_msg_op save_part_op = {
	save_part_desc,
	save_part_save,
	save_part_saved,
	save_part_free,
};

int
mail_save_part (CamelMimePart *part, const char *path,
		void (*done)(CamelMimePart *part, char *path, int saved, void *data), void *data)
{
	struct _save_part_msg *m;
	int id;
	
	m = mail_msg_new (&save_part_op, NULL, sizeof (*m));
	m->part = part;
	camel_object_ref (CAMEL_OBJECT (part));
	m->path = g_strdup (path);
	m->data = data;
	m->done = done;
	
	id = m->msg.seq;
	e_thread_put (mail_thread_queued, (EMsg *)m);
	
	return id;
}


/* ** GO OFFLINE ***************************************************** */

struct _set_offline_msg {
	struct _mail_msg msg;

	CamelStore *store;
	gboolean offline;
	void (*done)(CamelStore *store, void *data);
	void *data;
};

static char *set_offline_desc(struct _mail_msg *mm, int done)
{
	struct _set_offline_msg *m = (struct _set_offline_msg *)mm;
	char *service_name = camel_service_get_name (CAMEL_SERVICE (m->store), TRUE);
	char *msg;

	msg = g_strdup_printf (m->offline ? _("Disconnecting from %s") :
			       _("Reconnecting to %s"), service_name);
	g_free (service_name);
	return msg;
}

static void set_offline_do(struct _mail_msg *mm)
{
	struct _set_offline_msg *m = (struct _set_offline_msg *)mm;

	if (!CAMEL_IS_DISCO_STORE (m->store) ||
	    !camel_disco_store_can_work_offline (CAMEL_DISCO_STORE (m->store))) {
		if (m->offline) {
			camel_service_disconnect (CAMEL_SERVICE (m->store),
						  TRUE, &mm->ex);
		}
		return;
	}

	if (m->offline && camel_disco_store_status (CAMEL_DISCO_STORE (m->store)) == CAMEL_DISCO_STORE_ONLINE) {
		CamelFolder *inbox;

		/* FIXME. Something more generic here... (bug 10755) */
		inbox = camel_store_get_inbox (m->store, NULL);
		if (inbox) {
			camel_disco_folder_prepare_for_offline (
				CAMEL_DISCO_FOLDER (inbox),
				"(match-all (not (system-flag \"Seen\")))",
				&mm->ex);
			camel_folder_sync (inbox, FALSE, NULL);
			camel_object_unref (CAMEL_OBJECT (inbox));
			if (camel_exception_is_set (&mm->ex))
				return;
		}
	}

	camel_disco_store_set_status (CAMEL_DISCO_STORE (m->store),
				      m->offline ? CAMEL_DISCO_STORE_OFFLINE :
				      CAMEL_DISCO_STORE_ONLINE, &mm->ex);
}

static void set_offline_done(struct _mail_msg *mm)
{
	struct _set_offline_msg *m = (struct _set_offline_msg *)mm;

	if (m->done)
		m->done(m->store, m->data);
}

static void set_offline_free(struct _mail_msg *mm)
{
	struct _set_offline_msg *m = (struct _set_offline_msg *)mm;

	camel_object_unref((CamelObject *)m->store);
}

static struct _mail_msg_op set_offline_op = {
	set_offline_desc,
	set_offline_do,
	set_offline_done,
	set_offline_free,
};

void
mail_store_set_offline (CamelStore *store, gboolean offline,
			void (*done)(CamelStore *, void *data),
			void *data)
{
	struct _set_offline_msg *m;

	/* Cancel any pending connect first so the set_offline_op
	 * thread won't get queued behind a hung connect op.
	 */
	if (offline)
		camel_service_cancel_connect (CAMEL_SERVICE (store));

	m = mail_msg_new(&set_offline_op, NULL, sizeof(*m));
	m->store = store;
	camel_object_ref((CamelObject *)store);
	m->offline = offline;
	m->data = data;
	m->done = done;

	e_thread_put(mail_thread_queued, (EMsg *)m);
}


/* ** Execute Shell Command ***************************************************** */

struct _execute_shell_command_msg {
	struct _mail_msg msg;
	
	char *command;
};

static char *execute_shell_command_desc (struct _mail_msg *mm, int done)
{
	struct _execute_shell_command_msg *m = (struct _execute_shell_command_msg *) mm;
	char *msg;
	
	msg = g_strdup_printf (_("Executing shell command: %s"), m->command);
	
	return msg;
}

static void execute_shell_command_do (struct _mail_msg *mm)
{
	struct _execute_shell_command_msg *m = (struct _execute_shell_command_msg *) mm;
	
	gnome_execute_shell (NULL, m->command);
}

static void execute_shell_command_free (struct _mail_msg *mm)
{
	struct _execute_shell_command_msg *m = (struct _execute_shell_command_msg *) mm;
	
	g_free (m->command);
}

static struct _mail_msg_op execute_shell_command_op = {
	execute_shell_command_desc,
	execute_shell_command_do,
	NULL,
	execute_shell_command_free,
};

void
mail_execute_shell_command (CamelFilterDriver *driver, const char *command, void *data)
{
	struct _execute_shell_command_msg *m;
	
	if (command == NULL)
		return;
	
	m = mail_msg_new (&execute_shell_command_op, NULL, sizeof (*m));
	m->command = g_strdup (command);
	
	e_thread_put (mail_thread_queued, (EMsg *) m);
}
