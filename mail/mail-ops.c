/*
 * mail-ops.c: callbacks for the mail toolbar/menus
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Dan Winship <danw@ximian.com>
 *	Jeffrey Stedfast <fejj@ximian.com>
 *      Peter Williams <peterw@ximian.com>
 *      Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <glib/gi18n.h>

#include <camel/camel-mime-filter-from.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-vfs.h>
#include <camel/camel-mime-filter-charset.h>
#include <camel/camel-offline-folder.h>
#include <camel/camel-offline-store.h>
#include <camel/camel-disco-folder.h>
#include <camel/camel-disco-store.h>
#include <camel/camel-operation.h>
#include <camel/camel-vtrash-folder.h>
#include <camel/camel-vee-store.h>
#include <camel/camel-transport.h>
#include <camel/camel-multipart.h>

#include "composer/e-msg-composer.h"

#include <libedataserver/e-data-server-util.h>
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"

#include "em-filter-rule.h"
#include "em-utils.h"
#include "mail-component.h"
#include "mail-config.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-session.h"
#include "mail-tools.h"
#include "mail-vfolder.h"

#define w(x)
#define d(x)

const gchar *x_mailer;

/* used for both just filtering a folder + uid's, and for filtering a whole folder */
/* used both for fetching mail, and for filtering mail */
struct _filter_mail_msg {
	MailMsg base;

	CamelFolder *source_folder; /* where they come from */
	GPtrArray *source_uids;	/* uids to copy, or NULL == copy all */
	CamelUIDCache *cache;  /* UID cache if we are to cache the uids, NULL otherwise */
	CamelOperation *cancel;
	CamelFilterDriver *driver;
	gint delete;		/* delete messages after filtering them? */
	CamelFolder *destination; /* default destination for any messages, NULL for none */
};

/* since fetching also filters, we subclass the data here */
struct _fetch_mail_msg {
	struct _filter_mail_msg fmsg;

	CamelOperation *cancel;	/* we have our own cancellation struct, the other should be empty */
	gint keep;		/* keep on server? */

	gchar *source_uri;

	void (*done)(const gchar *source, gpointer data);
	gpointer data;
};

static gchar *
em_filter_folder_element_desc (struct _filter_mail_msg *m)
{
	return g_strdup (_("Filtering Selected Messages"));
}

/* filter a folder, or a subset thereof, uses source_folder/source_uids */
/* this is shared with fetch_mail */
static void
em_filter_folder_element_exec (struct _filter_mail_msg *m)
{
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

	camel_filter_driver_filter_folder (m->driver, folder, m->cache, uids, m->delete, &m->base.ex);
	camel_filter_driver_flush (m->driver, &m->base.ex);

	if (folder_uids)
		camel_folder_free_uids (folder, folder_uids);

	/* sync our source folder */
	if (!m->cache)
		camel_folder_sync (folder, FALSE, camel_exception_is_set (&m->base.ex) ? NULL : &m->base.ex);
	camel_folder_thaw (folder);

	if (m->destination)
		camel_folder_thaw (m->destination);

	/* this may thaw/unref source folders, do it here so we dont do it in the main thread
	   see also fetch_mail_fetch() below */
	camel_object_unref(m->driver);
	m->driver = NULL;

	if (m->cancel)
		camel_operation_unregister (m->cancel);
}

static void
em_filter_folder_element_done (struct _filter_mail_msg *m)
{
}

static void
em_filter_folder_element_free (struct _filter_mail_msg *m)
{
	if (m->source_folder)
		camel_object_unref (m->source_folder);

	if (m->source_uids)
		em_utils_uids_free (m->source_uids);

	if (m->cancel)
		camel_operation_unref (m->cancel);

	if (m->destination)
		camel_object_unref (m->destination);

	if (m->driver)
		camel_object_unref (m->driver);

	mail_session_flush_filter_log ();
}

static MailMsgInfo em_filter_folder_element_info = {
	sizeof (struct _filter_mail_msg),
	(MailMsgDescFunc) em_filter_folder_element_desc,  /* we do our own progress reporting? */
	(MailMsgExecFunc) em_filter_folder_element_exec,
	(MailMsgDoneFunc) em_filter_folder_element_done,
	(MailMsgFreeFunc) em_filter_folder_element_free
};

void
mail_filter_folder (CamelFolder *source_folder, GPtrArray *uids,
		    const gchar *type, gboolean notify,
		    CamelOperation *cancel)
{
	struct _filter_mail_msg *m;

	m = mail_msg_new (&em_filter_folder_element_info);
	m->source_folder = source_folder;
	camel_object_ref (source_folder);
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

	mail_msg_unordered_push (m);
}

/* convenience functions for it */
void
mail_filter_on_demand (CamelFolder *folder, GPtrArray *uids)
{
	mail_filter_folder (folder, uids, FILTER_SOURCE_DEMAND, FALSE, NULL);
}

void
mail_filter_junk (CamelFolder *folder, GPtrArray *uids)
{
	mail_filter_folder (folder, uids, FILTER_SOURCE_JUNKTEST, FALSE, NULL);
}

/* ********************************************************************** */

/* Temporary workaround for various issues. Gone before 0.11 */
static gchar *
uid_cachename_hack (CamelStore *store)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;
	gchar *encoded_url, *filename;
	const gchar *evolution_dir;

	encoded_url = g_strdup_printf ("%s%s%s@%s", url->user,
				       url->authmech ? ";auth=" : "",
				       url->authmech ? url->authmech : "",
				       url->host);
	e_filename_make_safe (encoded_url);

	evolution_dir = mail_component_peek_base_directory (mail_component_peek ());
	filename = g_build_filename (evolution_dir, "pop", encoded_url, "uid-cache", NULL);
	g_free (encoded_url);

	return filename;
}

static gchar *
fetch_mail_desc (struct _fetch_mail_msg *m)
{
	return g_strdup (_("Fetching Mail"));
}

static void
fetch_mail_exec (struct _fetch_mail_msg *m)
{
	struct _filter_mail_msg *fm = (struct _filter_mail_msg *)m;
	gint i;

	if (m->cancel)
		camel_operation_register (m->cancel);

	if ((fm->destination = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_LOCAL_INBOX)) == NULL)
		goto fail;
	camel_object_ref(fm->destination);

	/* FIXME: this should support keep_on_server too, which would then perform a spool
	   access thingy, right?  problem is matching raw messages to uid's etc. */
	if (!strncmp (m->source_uri, "mbox:", 5)) {
		gchar *path = mail_tool_do_movemail (m->source_uri, &fm->base.ex);

		if (path && !camel_exception_is_set (&fm->base.ex)) {
			camel_folder_freeze (fm->destination);
			camel_filter_driver_set_default_folder (fm->driver, fm->destination);
			camel_filter_driver_filter_mbox (fm->driver, path, m->source_uri, &fm->base.ex);
			camel_folder_thaw (fm->destination);

			if (!camel_exception_is_set (&fm->base.ex))
				g_unlink (path);
		}
		g_free (path);
	} else {
		CamelFolder *folder = fm->source_folder = mail_tool_get_inbox (m->source_uri, &fm->base.ex);

		if (folder) {
			/* this handles 'keep on server' stuff, if we have any new uid's to copy
			   across, we need to copy them to a new array 'cause of the way fetch_mail_free works */
			CamelUIDCache *cache = NULL;
			gchar *cachename;

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
					em_filter_folder_element_exec (fm);

					/* need to uncancel so writes/etc. don't fail */
					if (fm->base.ex.id == CAMEL_EXCEPTION_USER_CANCEL)
						camel_operation_uncancel(NULL);

					/* save the cache of uids that we've just downloaded */
					camel_uid_cache_save (cache);
				}

				if (fm->delete && !camel_exception_is_set (&fm->base.ex)) {
					/* not keep on server - just delete all the actual messages on the server */
					for (i=0;i<folder_uids->len;i++) {
						d(printf("force delete uid '%s'\n", (gchar *)folder_uids->pdata[i]));
						camel_folder_delete_message(folder, folder_uids->pdata[i]);
					}
				}

				if ((fm->delete || cache_uids) && !camel_exception_is_set (&fm->base.ex)) {
					/* expunge messages (downloaded so far) */
					camel_folder_sync(folder, fm->delete, NULL);
				}

				camel_uid_cache_destroy (cache);
				camel_folder_free_uids (folder, folder_uids);
			} else {
				em_filter_folder_element_exec (fm);
			}

			/* we unref the source folder here since we
			   may now block in finalize (we try to
			   disconnect cleanly) */
			camel_object_unref (fm->source_folder);
			fm->source_folder = NULL;
		}
	}
fail:
	if (m->cancel)
		camel_operation_unregister (m->cancel);

	/* we unref this here as it may have more work to do (syncing
	   folders and whatnot) before we are really done */
	/* should this be cancellable too? (i.e. above unregister above) */
	if (fm->driver) {
		camel_object_unref (fm->driver);
		fm->driver = NULL;
	}
}

static void
fetch_mail_done (struct _fetch_mail_msg *m)
{
	if (m->done)
		m->done (m->source_uri, m->data);
}

static void
fetch_mail_free (struct _fetch_mail_msg *m)
{
	g_free (m->source_uri);
	if (m->cancel)
		camel_operation_unref (m->cancel);

	em_filter_folder_element_free ((struct _filter_mail_msg *) m);
}

static MailMsgInfo fetch_mail_info = {
	sizeof (struct _fetch_mail_msg),
	(MailMsgDescFunc) fetch_mail_desc,
	(MailMsgExecFunc) fetch_mail_exec,
	(MailMsgDoneFunc) fetch_mail_done,
	(MailMsgFreeFunc) fetch_mail_free
};

/* ouch, a 'do everything' interface ... */
void
mail_fetch_mail (const gchar *source, gint keep, const gchar *type, CamelOperation *cancel,
		 CamelFilterGetFolderFunc get_folder, gpointer get_data,
		 CamelFilterStatusFunc *status, gpointer status_data,
		 void (*done)(const gchar *source, gpointer data), gpointer data)
{
	struct _fetch_mail_msg *m;
	struct _filter_mail_msg *fm;

	m = mail_msg_new (&fetch_mail_info);
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

	mail_msg_unordered_push (m);
}

/* ********************************************************************** */
/* sending stuff */
/* ** SEND MAIL *********************************************************** */

static const gchar *normal_recipients[] = {
	CAMEL_RECIPIENT_TYPE_TO,
	CAMEL_RECIPIENT_TYPE_CC,
	CAMEL_RECIPIENT_TYPE_BCC
};

static const gchar *resent_recipients[] = {
	CAMEL_RECIPIENT_TYPE_RESENT_TO,
	CAMEL_RECIPIENT_TYPE_RESENT_CC,
	CAMEL_RECIPIENT_TYPE_RESENT_BCC
};

/* send 1 message to a specific transport */
static void
mail_send_message(CamelFolder *queue, const gchar *uid, const gchar *destination, CamelFilterDriver *driver, CamelException *ex)
{
	EAccount *account = NULL;
	const CamelInternetAddress *iaddr;
	CamelAddress *from, *recipients;
	CamelMessageInfo *info = NULL;
	CamelTransport *xport = NULL;
	gchar *transport_url = NULL;
	gchar *sent_folder_uri = NULL;
	const gchar *resent_from, *tmp;
	CamelFolder *folder = NULL;
	GString *err = NULL;
	struct _camel_header_raw *xev, *header;
	CamelMimeMessage *message;
	gint i;

	message = camel_folder_get_message(queue, uid, ex);
	if (!message)
		return;

	camel_medium_set_header (CAMEL_MEDIUM (message), "X-Mailer", x_mailer);

	err = g_string_new("");
	xev = mail_tool_remove_xevolution_headers (message);

	tmp = camel_header_raw_find(&xev, "X-Evolution-Account", NULL);
	if (tmp) {
		gchar *name;

		name = g_strstrip(g_strdup(tmp));
		if ((account = mail_config_get_account_by_uid(name))
		    /* 'old' x-evolution-account stored the name, how silly */
		    || (account = mail_config_get_account_by_name(name))) {
			if (account->transport && account->transport->url)
				transport_url = g_strdup (account->transport->url);

			sent_folder_uri = g_strdup (account->sent_folder_uri);
		}
		g_free(name);
	}

	if (!account) {
		/* default back to these headers */
		tmp = camel_header_raw_find(&xev, "X-Evolution-Transport", NULL);
		if (tmp)
			transport_url = g_strstrip(g_strdup(tmp));

		tmp = camel_header_raw_find(&xev, "X-Evolution-Fcc", NULL);
		if (tmp)
			sent_folder_uri = g_strstrip(g_strdup(tmp));
	}

	/* Check for email sending */
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
		const gchar *type;

		type = resent_from ? resent_recipients[i] : normal_recipients[i];
		iaddr = camel_mime_message_get_recipients (message, type);
		camel_address_cat (recipients, CAMEL_ADDRESS (iaddr));
	}

	if (camel_address_length(recipients) > 0) {
		xport = camel_session_get_transport (session, transport_url ? transport_url : destination, ex);
		if (camel_exception_is_set(ex))
			goto exit;

		camel_transport_send_to(xport, message, from, recipients, ex);
		if (camel_exception_is_set(ex))
			goto exit;
	}

	/* Now check for posting, failures are ignored */
	info = camel_message_info_new(NULL);
	camel_message_info_set_flags(info, CAMEL_MESSAGE_SEEN, ~0);
	camel_mime_message_set_date(message, CAMEL_MESSAGE_DATE_CURRENT, 0);

	for (header = xev;header;header=header->next) {
		gchar *uri;

		if (strcmp(header->name, "X-Evolution-PostTo") != 0)
			continue;

		/* TODO: don't lose errors */

		uri = g_strstrip(g_strdup(header->value));
		folder = mail_tool_uri_to_folder(uri, 0, NULL);
		if (folder) {
			camel_folder_append_message(folder, message, info, NULL, NULL);
			camel_object_unref(folder);
			folder = NULL;
		}
		g_free(uri);
	}

	/* post process */
	mail_tool_restore_xevolution_headers (message, xev);

	if (driver) {
		camel_filter_driver_filter_message (driver, message, info,
						    NULL, NULL, NULL, "", ex);

		if (camel_exception_is_set (ex)) {
			if (camel_exception_get_id (ex) == CAMEL_EXCEPTION_USER_CANCEL)
				goto exit;

			/* sending mail, filtering failed */
			g_string_append_printf (err, _("Failed to apply outgoing filters: %s"),
						camel_exception_get_description (ex));
		}
	}

	camel_exception_clear (ex);

	if (xport == NULL
	    || !( ((CamelService *)xport)->provider->flags & CAMEL_PROVIDER_DISABLE_SENT_FOLDER)) {
		if (sent_folder_uri) {
			folder = mail_tool_uri_to_folder (sent_folder_uri, 0, ex);
			if (camel_exception_is_set(ex)) {
				g_string_append_printf (err, _("Failed to append to %s: %s\n"
							"Appending to local `Sent' folder instead."),
						sent_folder_uri, camel_exception_get_description (ex));
				camel_exception_clear (ex);
			}
		}

		if (!folder) {
			folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_SENT);
			camel_object_ref(folder);
		}

		camel_folder_append_message (folder, message, info, NULL, ex);
		if (camel_exception_is_set (ex)) {
			CamelFolder *sent_folder;

			if (camel_exception_get_id (ex) == CAMEL_EXCEPTION_USER_CANCEL)
				goto exit;

			sent_folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_SENT);

			if (folder != sent_folder) {
				const gchar *name;

				camel_object_get (folder, NULL, CAMEL_OBJECT_DESCRIPTION, (gchar **) &name, 0);
				if (err->len)
					g_string_append(err, "\n\n");
				g_string_append_printf (err, _("Failed to append to %s: %s\n"
							"Appending to local `Sent' folder instead."),
						name, camel_exception_get_description (ex));
				camel_object_ref (sent_folder);
				camel_object_unref (folder);
				folder = sent_folder;

				camel_exception_clear (ex);
				camel_folder_append_message (folder, message, info, NULL, ex);
			}

			if (camel_exception_is_set (ex)) {
				if (camel_exception_get_id (ex) == CAMEL_EXCEPTION_USER_CANCEL)
					goto exit;

				if (err->len)
					g_string_append(err, "\n\n");
				g_string_append_printf (err, _("Failed to append to local `Sent' folder: %s"),
						camel_exception_get_description (ex));
			}
		}
	}
	if (!camel_exception_is_set(ex)) {
		camel_folder_set_message_flags (queue, uid, CAMEL_MESSAGE_DELETED|CAMEL_MESSAGE_SEEN, ~0);
		/* Sync it to disk, since if it crashes in between, we keep sending it again on next start. */
		camel_folder_sync (queue, FALSE, NULL);
	}

	if (err->len) {
		/* set the culmulative exception report */
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, err->str);
	}

exit:
	if (folder) {
		camel_folder_sync(folder, FALSE, NULL);
		camel_object_unref(folder);
	}
	if (info)
		camel_message_info_free(info);
	camel_object_unref(recipients);
	camel_object_unref(from);
	if (xport)
		camel_object_unref(xport);
	g_free(sent_folder_uri);
	g_free(transport_url);
	camel_header_raw_clear(&xev);
	g_string_free(err, TRUE);
	camel_object_unref(message);

	return;
}

/* ** SEND MAIL QUEUE ***************************************************** */

struct _send_queue_msg {
	MailMsg base;

	CamelFolder *queue;
	gchar *destination;

	CamelFilterDriver *driver;
	CamelOperation *cancel;

	/* we use camelfilterstatusfunc, even though its not the filter doing it */
	CamelFilterStatusFunc *status;
	gpointer status_data;

	void (*done)(const gchar *destination, gpointer data);
	gpointer data;
};

static void
report_status (struct _send_queue_msg *m, enum camel_filter_status_t status, gint pc, const gchar *desc, ...)
{
	va_list ap;
	gchar *str;

	if (m->status) {
		va_start (ap, desc);
		str = g_strdup_vprintf (desc, ap);
		va_end (ap);
		m->status (m->driver, status, pc, str, m->status_data);
		g_free (str);
	}
}

static void
send_queue_exec (struct _send_queue_msg *m)
{
	CamelFolder *sent_folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_SENT);
	GPtrArray *uids, *send_uids = NULL;
	CamelException ex;
	gint i, j;

	d(printf("sending queue\n"));

	if (!(uids = camel_folder_get_uids (m->queue)))
		return;

	send_uids = g_ptr_array_sized_new (uids->len);
	for (i = 0, j = 0; i < uids->len; i++) {
		CamelMessageInfo *info;

		info = camel_folder_get_message_info (m->queue, uids->pdata[i]);
		if (info) {
			if ((camel_message_info_flags(info) & CAMEL_MESSAGE_DELETED) == 0)
				send_uids->pdata[j++] = uids->pdata[i];
			camel_folder_free_message_info(m->queue, info);
		}
	}

	send_uids->len = j;
	if (send_uids->len == 0) {
		/* nothing to send */
		camel_folder_free_uids (m->queue, uids);
		g_ptr_array_free (send_uids, TRUE);
		return;
	}

	if (m->cancel)
		camel_operation_register (m->cancel);
	else
		camel_operation_register (m->base.cancel);

	if (!m->cancel)
		camel_operation_start (NULL, _("Sending message"));

	camel_exception_init (&ex);

	/* NB: This code somewhat abuses the 'exception' stuff.  Apart from fatal problems, it is also
	   used as a mechanism to accumualte warning messages and present them back to the user. */

	for (i = 0, j = 0; i < send_uids->len; i++) {
		gint pc = (100 * i) / send_uids->len;

		report_status (m, CAMEL_FILTER_STATUS_START, pc, _("Sending message %d of %d"), i+1, send_uids->len);
		if (!m->cancel)
			camel_operation_progress (NULL, (i+1) * 100 / send_uids->len);

		mail_send_message (m->queue, send_uids->pdata[i], m->destination, m->driver, &ex);
		if (camel_exception_is_set (&ex)) {
			if (ex.id != CAMEL_EXCEPTION_USER_CANCEL) {
				/* merge exceptions into one */
				if (camel_exception_is_set (&m->base.ex))
					camel_exception_setv (&m->base.ex, CAMEL_EXCEPTION_SYSTEM, "%s\n\n%s", m->base.ex.desc, ex.desc);
				else
					camel_exception_xfer (&m->base.ex, &ex);
				camel_exception_clear (&ex);

				/* keep track of the number of failures */
				j++;
			} else {
				/* transfer the USER_CANCEL exeption to the async op exception and then break */
				camel_exception_xfer (&m->base.ex, &ex);
				break;
			}
		}
	}

	j += (send_uids->len - i);

	if (j > 0)
		report_status (m, CAMEL_FILTER_STATUS_END, 100, _("Failed to send %d of %d messages"), j, send_uids->len);
	else if (m->base.ex.id == CAMEL_EXCEPTION_USER_CANCEL)
		report_status (m, CAMEL_FILTER_STATUS_END, 100, _("Canceled."));
	else
		report_status (m, CAMEL_FILTER_STATUS_END, 100, _("Complete."));

	if (m->driver) {
		camel_object_unref (m->driver);
		m->driver = NULL;
	}

	camel_folder_free_uids (m->queue, uids);
	g_ptr_array_free (send_uids, TRUE);

	if (j <= 0 && !camel_exception_is_set (&m->base.ex)) {
		camel_folder_sync (m->queue, TRUE, &ex);
		camel_exception_clear (&ex);
	}

	if (sent_folder) {
		camel_folder_sync (sent_folder, FALSE, &ex);
		camel_exception_clear (&ex);
	}

	if (!m->cancel)
		camel_operation_end (NULL);

	if (m->cancel)
		camel_operation_unregister (m->cancel);
	else
		camel_operation_unregister (m->base.cancel);

}

static void
send_queue_done (struct _send_queue_msg *m)
{
	if (m->done)
		m->done(m->destination, m->data);
}

static gchar *
send_queue_desc (struct _send_queue_msg *m)
{
	return g_strdup (_("Sending message"));
}

static void
send_queue_free (struct _send_queue_msg *m)
{
	if (m->driver)
		camel_object_unref(m->driver);
	camel_object_unref(m->queue);
	g_free(m->destination);
	if (m->cancel)
		camel_operation_unref(m->cancel);
}

static MailMsgInfo send_queue_info = {
	sizeof (struct _send_queue_msg),
	(MailMsgDescFunc) send_queue_desc,
	(MailMsgExecFunc) send_queue_exec,
	(MailMsgDoneFunc) send_queue_done,
	(MailMsgFreeFunc) send_queue_free
};

/* same interface as fetch_mail, just 'cause i'm lazy today (and we need to run it from the same spot?) */
void
mail_send_queue(CamelFolder *queue, const gchar *destination,
		const gchar *type, CamelOperation *cancel,
		CamelFilterGetFolderFunc get_folder, gpointer get_data,
		CamelFilterStatusFunc *status, gpointer status_data,
		void (*done)(const gchar *destination, gpointer data), gpointer data)
{
	struct _send_queue_msg *m;

	m = mail_msg_new(&send_queue_info);
	m->queue = queue;
	camel_object_ref(queue);
	m->destination = g_strdup(destination);
	if (cancel) {
		m->cancel = cancel;
		camel_operation_ref(cancel);
		camel_operation_unref (m->base.cancel);
		mail_msg_set_cancelable (m, FALSE);

		m->base.cancel = NULL;
	}
	m->status = status;
	m->status_data = status_data;
	m->done = done;
	m->data = data;

	m->driver = camel_session_get_filter_driver (session, type, NULL);
	camel_filter_driver_set_folder_func (m->driver, get_folder, get_data);

	mail_msg_unordered_push (m);
}

/* ** APPEND MESSAGE TO FOLDER ******************************************** */

struct _append_msg {
	MailMsg base;

        CamelFolder *folder;
	CamelMimeMessage *message;
        CamelMessageInfo *info;
	gchar *appended_uid;

	void (*done)(CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info, gint ok, const gchar *appended_uid, gpointer data);
	gpointer data;
};

static gchar *
append_mail_desc (struct _append_msg *m)
{
	return g_strdup (_("Saving message to folder"));
}

static void
append_mail_exec (struct _append_msg *m)
{
	camel_mime_message_set_date(m->message, CAMEL_MESSAGE_DATE_CURRENT, 0);
	camel_folder_append_message(m->folder, m->message, m->info, &m->appended_uid, &m->base.ex);
}

static void
append_mail_done (struct _append_msg *m)
{
	if (m->done)
		m->done(m->folder, m->message, m->info, !camel_exception_is_set(&m->base.ex), m->appended_uid, m->data);
}

static void
append_mail_free (struct _append_msg *m)
{
	camel_object_unref(m->message);
	camel_object_unref(m->folder);
	g_free (m->appended_uid);
}

static MailMsgInfo append_mail_info = {
	sizeof (struct _append_msg),
	(MailMsgDescFunc) append_mail_desc,
	(MailMsgExecFunc) append_mail_exec,
	(MailMsgDoneFunc) append_mail_done,
	(MailMsgFreeFunc) append_mail_free
};

void
mail_append_mail (CamelFolder *folder, CamelMimeMessage *message, CamelMessageInfo *info,
		  void (*done)(CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info, gint ok, const gchar *appended_uid, gpointer data),
		  gpointer data)
{
	struct _append_msg *m;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	if (!camel_medium_get_header (CAMEL_MEDIUM (message), "X-Mailer"))
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Mailer", x_mailer);

	m = mail_msg_new (&append_mail_info);
	m->folder = folder;
	camel_object_ref(folder);
	m->message = message;
	camel_object_ref(message);
	m->info = info;

	m->done = done;
	m->data = data;

	mail_msg_unordered_push (m);
}

/* ** TRANSFER MESSAGES **************************************************** */

struct _transfer_msg {
	MailMsg base;

	CamelFolder *source;
	GPtrArray *uids;
	gboolean delete;
	gchar *dest_uri;
	guint32 dest_flags;

	void (*done)(gboolean ok, gpointer data);
	gpointer data;
};

static gchar *
transfer_messages_desc (struct _transfer_msg *m)
{
	return g_strdup_printf(m->delete?_("Moving messages to %s"):_("Copying messages to %s"),
			       m->dest_uri);

}

static void
transfer_messages_exec (struct _transfer_msg *m)
{
	CamelFolder *dest;

	dest = mail_tool_uri_to_folder (m->dest_uri, m->dest_flags, &m->base.ex);
	if (camel_exception_is_set (&m->base.ex))
		return;

	if (dest == m->source) {
		camel_object_unref(dest);
		/* no-op */
		return;
	}

	camel_folder_freeze (m->source);
	camel_folder_freeze (dest);

	camel_folder_transfer_messages_to (m->source, m->uids, dest, NULL, m->delete, &m->base.ex);

	/* make sure all deleted messages are marked as seen */

	if (m->delete) {
		gint i;

		for (i = 0; i < m->uids->len; i++)
			camel_folder_set_message_flags (m->source, m->uids->pdata[i],
							CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	}

	camel_folder_thaw (m->source);
	camel_folder_thaw (dest);
	camel_folder_sync (dest, FALSE, NULL);
	camel_object_unref (dest);
}

static void
transfer_messages_done (struct _transfer_msg *m)
{
	if (m->done)
		m->done (!camel_exception_is_set (&m->base.ex), m->data);
}

static void
transfer_messages_free (struct _transfer_msg *m)
{
	camel_object_unref (m->source);
	g_free (m->dest_uri);
	em_utils_uids_free (m->uids);
}

static MailMsgInfo transfer_messages_info = {
	sizeof (struct _transfer_msg),
	(MailMsgDescFunc) transfer_messages_desc,
	(MailMsgExecFunc) transfer_messages_exec,
	(MailMsgDoneFunc) transfer_messages_done,
	(MailMsgFreeFunc) transfer_messages_free
};

void
mail_transfer_messages (CamelFolder *source, GPtrArray *uids,
			gboolean delete_from_source,
			const gchar *dest_uri,
			guint32 dest_flags,
			void (*done) (gboolean ok, gpointer data),
			gpointer data)
{
	struct _transfer_msg *m;

	g_return_if_fail (CAMEL_IS_FOLDER (source));
	g_return_if_fail (uids != NULL);
	g_return_if_fail (dest_uri != NULL);

	m = mail_msg_new(&transfer_messages_info);
	m->source = source;
	camel_object_ref (source);
	m->uids = uids;
	m->delete = delete_from_source;
	m->dest_uri = g_strdup (dest_uri);
	m->dest_flags = dest_flags;
	m->done = done;
	m->data = data;

	mail_msg_slow_ordered_push (m);
}

/* ** SCAN SUBFOLDERS ***************************************************** */

struct _get_folderinfo_msg {
	MailMsg base;

	CamelStore *store;
	CamelFolderInfo *info;
	gboolean (*done)(CamelStore *store, CamelFolderInfo *info, gpointer data);
	gpointer data;
	gboolean can_clear; /* whether we can clear folder info */
};

static gchar *
get_folderinfo_desc (struct _get_folderinfo_msg *m)
{
	gchar *ret, *name;

	name = camel_service_get_name((CamelService *)m->store, TRUE);
	ret = g_strdup_printf(_("Scanning folders in \"%s\""), name);
	g_free(name);
	return ret;
}

static void
get_folderinfo_exec (struct _get_folderinfo_msg *m)
{
	guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE|CAMEL_STORE_FOLDER_INFO_FAST | CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;

	m->info = camel_store_get_folder_info (m->store, NULL, flags, &m->base.ex);
}

static void
get_folderinfo_done (struct _get_folderinfo_msg *m)
{
	if (!m->info && camel_exception_is_set (&m->base.ex)) {
		gchar *url;

		url = camel_service_get_url (CAMEL_SERVICE (m->store));
		w(g_warning ("Error getting folder info from store at %s: %s",
			     url, camel_exception_get_description (&m->base.ex)));
		g_free (url);
	}

	if (m->done)
		m->can_clear = m->done (m->store, m->info, m->data);
	else
		m->can_clear = TRUE;
}

static void
get_folderinfo_free (struct _get_folderinfo_msg *m)
{
	if (m->info && m->can_clear)
		camel_store_free_folder_info(m->store, m->info);
	camel_object_unref(m->store);
}

static MailMsgInfo get_folderinfo_info = {
	sizeof (struct _get_folderinfo_msg),
	(MailMsgDescFunc) get_folderinfo_desc,
	(MailMsgExecFunc) get_folderinfo_exec,
	(MailMsgDoneFunc) get_folderinfo_done,
	(MailMsgFreeFunc) get_folderinfo_free
};

gint
mail_get_folderinfo (CamelStore *store, CamelOperation *op, gboolean (*done)(CamelStore *store, CamelFolderInfo *info, gpointer data), gpointer data)
{
	struct _get_folderinfo_msg *m;
	gint id;

	m = mail_msg_new(&get_folderinfo_info);
	if (op) {
		camel_operation_unref(m->base.cancel);
		m->base.cancel = op;
		camel_operation_ref(op);
	}
	m->store = store;
	camel_object_ref(store);
	m->done = done;
	m->data = data;
	id = m->base.seq;

	mail_msg_unordered_push (m);

	return id;
}

/* ** ATTACH MESSAGES ****************************************************** */

struct _build_data {
	void (*done)(CamelFolder *folder, GPtrArray *uids, CamelMimePart *part, gchar *subject, gpointer data);
	gpointer data;
};

static void
do_build_attachment (CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, gpointer data)
{
	struct _build_data *d = data;
	CamelMultipart *multipart;
	CamelMimePart *part;
	gchar *subject;
	gint i;

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
			camel_object_unref(part);
		}
		part = camel_mime_part_new();
		camel_medium_set_content_object(CAMEL_MEDIUM (part), CAMEL_DATA_WRAPPER(multipart));
		camel_object_unref(multipart);

		camel_mime_part_set_description(part, _("Forwarded messages"));
	}

	subject = mail_tool_generate_forward_subject(messages->pdata[0]);
	d->done(folder, messages, part, subject, d->data);
	g_free(subject);
	camel_object_unref(part);

	g_free(d);
}

void
mail_build_attachment(CamelFolder *folder, GPtrArray *uids,
		      void (*done)(CamelFolder *folder, GPtrArray *messages, CamelMimePart *part, gchar *subject, gpointer data), gpointer data)
{
	struct _build_data *d;

	d = g_malloc(sizeof(*d));
	d->done = done;
	d->data = data;
	mail_get_messages(folder, uids, do_build_attachment, d);
}

/* ** LOAD FOLDER ********************************************************* */

/* there should be some way to merge this and create folder, since both can
   presumably create a folder ... */

struct _get_folder_msg {
	MailMsg base;

	gchar *uri;
	guint32 flags;
	CamelFolder *folder;
	void (*done) (gchar *uri, CamelFolder *folder, gpointer data);
	gpointer data;
};

static gchar *
get_folder_desc (struct _get_folder_msg *m)
{
	return g_strdup_printf(_("Opening folder %s"), m->uri);
}

static void
get_folder_exec (struct _get_folder_msg *m)
{
	m->folder = mail_tool_uri_to_folder (m->uri, m->flags, &m->base.ex);
}

static void
get_folder_done (struct _get_folder_msg *m)
{
	if (m->done)
		m->done (m->uri, m->folder, m->data);
}

static void
get_folder_free (struct _get_folder_msg *m)
{
	g_free (m->uri);
	if (m->folder)
		camel_object_unref (m->folder);
}

static MailMsgInfo get_folder_info = {
	sizeof (struct _get_folder_msg),
	(MailMsgDescFunc) get_folder_desc,
	(MailMsgExecFunc) get_folder_exec,
	(MailMsgDoneFunc) get_folder_done,
	(MailMsgFreeFunc) get_folder_free
};

gint
mail_get_folder (const gchar *uri, guint32 flags,
		 void (*done)(gchar *uri, CamelFolder *folder, gpointer data),
		 gpointer data, MailMsgDispatchFunc dispatch)
{
	struct _get_folder_msg *m;
	gint id;

	m = mail_msg_new(&get_folder_info);
	m->uri = g_strdup (uri);
	m->flags = flags;
	m->data = data;
	m->done = done;

	id = m->base.seq;
	dispatch (m);
	return id;
}

/* ** GET FOLDER'S QUOTA ********************************************************* */

struct _get_quota_msg {
	MailMsg base;

	CamelFolder *folder;
	CamelFolderQuotaInfo *quota;
	void (*done) (CamelFolder *folder, CamelFolderQuotaInfo *quota, gpointer data);
	gpointer data;
};

static gchar *
get_quota_desc (struct _get_quota_msg *m)
{
	return g_strdup_printf(_("Retrieving quota information for folder %s"), camel_folder_get_name (m->folder));
}

static void
get_quota_exec (struct _get_quota_msg *m)
{
	m->quota = camel_folder_get_quota_info (m->folder);
}

static void
get_quota_done (struct _get_quota_msg *m)
{
	if (m->done)
		m->done (m->folder, m->quota, m->data);
}

static void
get_quota_free (struct _get_quota_msg *m)
{
	if (m->folder)
		camel_object_unref (m->folder);
	if (m->quota)
		camel_folder_quota_info_free (m->quota);
}

static MailMsgInfo get_quota_info = {
	sizeof (struct _get_quota_msg),
	(MailMsgDescFunc) get_quota_desc,
	(MailMsgExecFunc) get_quota_exec,
	(MailMsgDoneFunc) get_quota_done,
	(MailMsgFreeFunc) get_quota_free
};

gint
mail_get_folder_quota (CamelFolder *folder,
		 void (*done)(CamelFolder *folder, CamelFolderQuotaInfo *quota, gpointer data),
		 gpointer data, MailMsgDispatchFunc dispatch)
{
	struct _get_quota_msg *m;
	gint id;

	g_return_val_if_fail (folder != NULL, -1);

	m = mail_msg_new (&get_quota_info);
	m->folder = folder;
	m->data = data;
	m->done = done;

	camel_object_ref (m->folder);

	id = m->base.seq;
	dispatch (m);
	return id;
}

/* ** GET STORE ******************************************************* */

struct _get_store_msg {
	MailMsg base;

	gchar *uri;
	CamelStore *store;
	void (*done) (gchar *uri, CamelStore *store, gpointer data);
	gpointer data;
};

static gchar *
get_store_desc (struct _get_store_msg *m)
{
	return g_strdup_printf(_("Opening store %s"), m->uri);
}

static void
get_store_exec (struct _get_store_msg *m)
{
	/*camel_session_get_store connects us, which we don't want to do on startup. */

	m->store = (CamelStore *) camel_session_get_service (session, m->uri,
							     CAMEL_PROVIDER_STORE,
							     &m->base.ex);
}

static void
get_store_done (struct _get_store_msg *m)
{
	if (m->done)
		m->done (m->uri, m->store, m->data);
}

static void
get_store_free (struct _get_store_msg *m)
{
	g_free (m->uri);
	if (m->store)
		camel_object_unref (m->store);
}

static MailMsgInfo get_store_info = {
	sizeof (struct _get_store_msg),
	(MailMsgDescFunc) get_store_desc,
	(MailMsgExecFunc) get_store_exec,
	(MailMsgDoneFunc) get_store_done,
	(MailMsgFreeFunc) get_store_free
};

gint
mail_get_store (const gchar *uri, CamelOperation *op, void (*done) (gchar *uri, CamelStore *store, gpointer data), gpointer data)
{
	struct _get_store_msg *m;
	gint id;

	m = mail_msg_new (&get_store_info);
	if (op) {
		camel_operation_unref(m->base.cancel);
		m->base.cancel = op;
		camel_operation_ref(op);
	}
	m->uri = g_strdup (uri);
	m->data = data;
	m->done = done;

	id = m->base.seq;
	mail_msg_unordered_push (m);
	return id;
}

/* ** REMOVE FOLDER ******************************************************* */

struct _remove_folder_msg {
	MailMsg base;

	CamelFolder *folder;
	gboolean removed;
	void (*done) (CamelFolder *folder, gboolean removed, CamelException *ex, gpointer data);
	gpointer data;
};

static gchar *
remove_folder_desc (struct _remove_folder_msg *m)
{
	return g_strdup_printf (_("Removing folder %s"), m->folder->full_name);
}

static void
remove_folder_rec (CamelStore *store, CamelFolderInfo *fi, CamelException *ex)
{
	while (fi) {
		CamelFolder *folder;

		if (fi->child) {
			remove_folder_rec (store, fi->child, ex);
			if (camel_exception_is_set (ex))
				return;
		}

		d(printf ("deleting folder '%s'\n", fi->full_name));

		if (!(folder = camel_store_get_folder (store, fi->full_name, 0, ex)))
			return;

		if (!CAMEL_IS_VEE_FOLDER (folder)) {
			GPtrArray *uids = camel_folder_get_uids (folder);
			gint i;

			/* Delete every message in this folder, then expunge it */
			camel_folder_freeze (folder);
			for (i = 0; i < uids->len; i++)
				camel_folder_delete_message (folder, uids->pdata[i]);

			camel_folder_free_uids (folder, uids);

			camel_folder_sync (folder, TRUE, NULL);
			camel_folder_thaw (folder);
		}

		/* if the store supports subscriptions, unsubscribe from this folder... */
		if (camel_store_supports_subscriptions (store))
			camel_store_unsubscribe_folder (store, fi->full_name, NULL);

		/* Then delete the folder from the store */
		camel_store_delete_folder (store, fi->full_name, ex);
		if (camel_exception_is_set (ex))
			return;

		fi = fi->next;
	}
}

static void
remove_folder_exec (struct _remove_folder_msg *m)
{
	CamelStore *store;
	CamelFolderInfo *fi;

	m->removed = FALSE;

	store = m->folder->parent_store;

	fi = camel_store_get_folder_info (store, m->folder->full_name, CAMEL_STORE_FOLDER_INFO_RECURSIVE | CAMEL_STORE_FOLDER_INFO_FAST | CAMEL_STORE_FOLDER_INFO_SUBSCRIBED, &m->base.ex);
	if (camel_exception_is_set (&m->base.ex))
		return;

	remove_folder_rec (store, fi, &m->base.ex);
	camel_store_free_folder_info (store, fi);

	m->removed = !camel_exception_is_set (&m->base.ex);
}

static void
remove_folder_done (struct _remove_folder_msg *m)
{
	if (m->done)
		m->done (m->folder, m->removed, &m->base.ex, m->data);
}

static void
remove_folder_free (struct _remove_folder_msg *m)
{
	camel_object_unref (m->folder);
}

static MailMsgInfo remove_folder_info = {
	sizeof (struct _remove_folder_msg),
	(MailMsgDescFunc) remove_folder_desc,
	(MailMsgExecFunc) remove_folder_exec,
	(MailMsgDoneFunc) remove_folder_done,
	(MailMsgFreeFunc) remove_folder_free
};

void
mail_remove_folder (CamelFolder *folder, void (*done) (CamelFolder *folder, gboolean removed, CamelException *ex, gpointer data), gpointer data)
{
	struct _remove_folder_msg *m;

	g_return_if_fail (folder != NULL);

	m = mail_msg_new (&remove_folder_info);
	m->folder = folder;
	camel_object_ref (folder);
	m->data = data;
	m->done = done;

	mail_msg_unordered_push (m);
}

/* ** SYNC FOLDER ********************************************************* */

struct _sync_folder_msg {
	MailMsg base;

	CamelFolder *folder;
	void (*done) (CamelFolder *folder, gpointer data);
	gpointer data;
};

static gchar *
sync_folder_desc (struct _sync_folder_msg *m)
{
	return g_strdup_printf (_("Storing folder \'%s\'"),
			       camel_folder_get_full_name (m->folder));
}

static void
sync_folder_exec (struct _sync_folder_msg *m)
{
	camel_folder_sync(m->folder, FALSE, &m->base.ex);
}

static void
sync_folder_done (struct _sync_folder_msg *m)
{
	if (m->done)
		m->done(m->folder, m->data);
}

static void
sync_folder_free (struct _sync_folder_msg *m)
{
	camel_object_unref((CamelObject *)m->folder);
}

static MailMsgInfo sync_folder_info = {
	sizeof (struct _sync_folder_msg),
	(MailMsgDescFunc) sync_folder_desc,
	(MailMsgExecFunc) sync_folder_exec,
	(MailMsgDoneFunc) sync_folder_done,
	(MailMsgFreeFunc) sync_folder_free
};

void
mail_sync_folder(CamelFolder *folder, void (*done) (CamelFolder *folder, gpointer data), gpointer data)
{
	struct _sync_folder_msg *m;

	m = mail_msg_new(&sync_folder_info);
	m->folder = folder;
	camel_object_ref(folder);
	m->data = data;
	m->done = done;

	mail_msg_slow_ordered_push (m);
}

/* ** SYNC STORE ********************************************************* */

struct _sync_store_msg {
	MailMsg base;

	CamelStore *store;
	gint expunge;
	void (*done) (CamelStore *store, gpointer data);
	gpointer data;
};

static gchar *
sync_store_desc (struct _sync_store_msg *m)
{
	gchar *uri, *res;

	uri = camel_url_to_string(((CamelService *)m->store)->url, CAMEL_URL_HIDE_ALL);
	res = g_strdup_printf(m->expunge
			      ?_("Expunging and storing account '%s'")
			      :_("Storing account '%s'"),
			      uri);
	g_free(uri);

	return res;
}

static void
sync_store_exec (struct _sync_store_msg *m)
{
	camel_store_sync(m->store, m->expunge, &m->base.ex);
}

static void
sync_store_done (struct _sync_store_msg *m)
{
	if (m->done)
		m->done(m->store, m->data);
}

static void
sync_store_free (struct _sync_store_msg *m)
{
	camel_object_unref(m->store);
}

static MailMsgInfo sync_store_info = {
	sizeof (struct _sync_store_msg),
	(MailMsgDescFunc) sync_store_desc,
	(MailMsgExecFunc) sync_store_exec,
	(MailMsgDoneFunc) sync_store_done,
	(MailMsgFreeFunc) sync_store_free
};

void
mail_sync_store(CamelStore *store, gint expunge, void (*done) (CamelStore *store, gpointer data), gpointer data)
{
	struct _sync_store_msg *m;

	m = mail_msg_new(&sync_store_info);
	m->store = store;
	m->expunge = expunge;
	camel_object_ref(store);
	m->data = data;
	m->done = done;

	mail_msg_slow_ordered_push (m);
}

/* ******************************************************************************** */

static gchar *
refresh_folder_desc (struct _sync_folder_msg *m)
{
	return g_strdup(_("Refreshing folder"));
}

static void
refresh_folder_exec (struct _sync_folder_msg *m)
{
	/* camel_folder_sync (m->folder, FALSE, &m->base.ex); */

	/* if (!camel_exception_is_set (&m->base.ex)) */
		camel_folder_refresh_info(m->folder, &m->base.ex);
}

/* we just use the sync stuff where we can, since it would be the same */
static MailMsgInfo refresh_folder_info = {
	sizeof (struct _sync_folder_msg),
	(MailMsgDescFunc) refresh_folder_desc,
	(MailMsgExecFunc) refresh_folder_exec,
	(MailMsgDoneFunc) sync_folder_done,
	(MailMsgFreeFunc) sync_folder_free
};

void
mail_refresh_folder(CamelFolder *folder, void (*done) (CamelFolder *folder, gpointer data), gpointer data)
{
	struct _sync_folder_msg *m;

	m = mail_msg_new(&refresh_folder_info);
	m->folder = folder;
	camel_object_ref(folder);
	m->data = data;
	m->done = done;

	mail_msg_slow_ordered_push (m);
}

/* ******************************************************************************** */

static gchar *
expunge_folder_desc (struct _sync_folder_msg *m)
{
	return g_strdup(_("Expunging folder"));
}

static void
expunge_folder_exec (struct _sync_folder_msg *m)
{
	camel_folder_expunge(m->folder, &m->base.ex);
}

/* we just use the sync stuff where we can, since it would be the same */
static MailMsgInfo expunge_folder_info = {
	sizeof (struct _sync_folder_msg),
	(MailMsgDescFunc) expunge_folder_desc,
	(MailMsgExecFunc) expunge_folder_exec,
	(MailMsgDoneFunc) sync_folder_done,
	(MailMsgFreeFunc) sync_folder_free
};

void
mail_expunge_folder(CamelFolder *folder, void (*done) (CamelFolder *folder, gpointer data), gpointer data)
{
	struct _sync_folder_msg *m;

	m = mail_msg_new(&expunge_folder_info);
	m->folder = folder;
	camel_object_ref(folder);
	m->data = data;
	m->done = done;

	mail_msg_slow_ordered_push (m);
}

/* ******************************************************************************** */

struct _empty_trash_msg {
	MailMsg base;

	EAccount *account;
	void (*done) (EAccount *account, gpointer data);
	gpointer data;
};

static gchar *
empty_trash_desc (struct _empty_trash_msg *m)
{
	/* FIXME after 1.4 is out and we're not in string freeze any more. */
#if 0
	return g_strdup_printf (_("Emptying trash in \'%s\'"),
				m->account ? m->account->name : _("Local Folders"));
#else
	return g_strdup(_("Expunging folder"));
#endif
}

static void
empty_trash_exec (struct _empty_trash_msg *m)
{
	const gchar *evolution_dir;
	CamelFolder *trash;
	gchar *uri;

	if (m->account) {
		trash = mail_tool_get_trash (m->account->source->url, FALSE, &m->base.ex);
	} else {
		evolution_dir = mail_component_peek_base_directory (mail_component_peek ());
		uri = g_strdup_printf ("mbox:%s/local", evolution_dir);
		trash = mail_tool_get_trash (uri, TRUE, &m->base.ex);
		g_free (uri);
	}

	if (trash) {
		camel_folder_expunge (trash, &m->base.ex);
		camel_object_unref (trash);
	}
}

static void
empty_trash_done (struct _empty_trash_msg *m)
{
	if (m->done)
		m->done(m->account, m->data);
}

static void
empty_trash_free (struct _empty_trash_msg *m)
{
	if (m->account)
		g_object_unref(m->account);
}

static MailMsgInfo empty_trash_info = {
	sizeof (struct _empty_trash_msg),
	(MailMsgDescFunc) empty_trash_desc,
	(MailMsgExecFunc) empty_trash_exec,
	(MailMsgDoneFunc) empty_trash_done,
	(MailMsgFreeFunc) empty_trash_free
};

void
mail_empty_trash(EAccount *account, void (*done) (EAccount *account, gpointer data), gpointer data)
{
	struct _empty_trash_msg *m;

	m = mail_msg_new(&empty_trash_info);
	m->account = account;
	if (account)
		g_object_ref(account);
	m->data = data;
	m->done = done;

	mail_msg_slow_ordered_push (m);
}

/* ** GET MESSAGE(s) ***************************************************** */

struct _get_message_msg {
	MailMsg base;

	CamelFolder *folder;
	gchar *uid;
	void (*done) (CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, gpointer data);
	gpointer data;
	CamelMimeMessage *message;
	CamelOperation *cancel;
};

static gchar *
get_message_desc (struct _get_message_msg *m)
{
	return g_strdup_printf(_("Retrieving message %s"), m->uid);
}

static void
get_message_exec (struct _get_message_msg *m)
{
	m->message = camel_folder_get_message(m->folder, m->uid, &m->base.ex);
}

static void
get_message_done (struct _get_message_msg *m)
{
	if (m->done)
		m->done(m->folder, m->uid, m->message, m->data);
}

static void
get_message_free (struct _get_message_msg *m)
{
	g_free (m->uid);
	camel_object_unref (m->folder);
	camel_operation_unref (m->cancel);

	if (m->message)
		camel_object_unref (m->message);
}

static MailMsgInfo get_message_info = {
	sizeof (struct _get_message_msg),
	(MailMsgDescFunc) get_message_desc,
	(MailMsgExecFunc) get_message_exec,
	(MailMsgDoneFunc) get_message_done,
	(MailMsgFreeFunc) get_message_free
};

void
mail_get_message(CamelFolder *folder, const gchar *uid, void (*done) (CamelFolder *folder, const gchar *uid,
								     CamelMimeMessage *msg, gpointer data),
		 gpointer data, MailMsgDispatchFunc dispatch)
{
	struct _get_message_msg *m;

	m = mail_msg_new(&get_message_info);
	m->folder = folder;
	camel_object_ref(folder);
	m->uid = g_strdup(uid);
	m->data = data;
	m->done = (void (*) (CamelFolder *, const gchar *, CamelMimeMessage *, gpointer )) done;
	m->cancel = camel_operation_new(NULL, NULL);

	dispatch (m);
}

typedef void (*get_done)(CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, gpointer data, CamelException *);

static void
get_messagex_done (struct _get_message_msg *m)
{
	if (m->done && !camel_operation_cancel_check (m->cancel)) {
		get_done done = (get_done)m->done;
		done(m->folder, m->uid, m->message, m->data, &m->base.ex);
	}
}

static MailMsgInfo get_messagex_info = {
	sizeof (struct _get_message_msg),
	(MailMsgDescFunc) get_message_desc,
	(MailMsgExecFunc) get_message_exec,
	(MailMsgDoneFunc) get_messagex_done,
	(MailMsgFreeFunc) get_message_free
};

/* This is temporary, to avoid having to rewrite everything that uses
   mail_get_message; it adds an exception argument to the callback */
CamelOperation *
mail_get_messagex(CamelFolder *folder, const gchar *uid, void (*done) (CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, gpointer data, CamelException *),
		 gpointer data, MailMsgDispatchFunc dispatch)
{
	struct _get_message_msg *m;

	m = mail_msg_new(&get_messagex_info);
	m->folder = folder;
	camel_object_ref(folder);
	m->uid = g_strdup(uid);
	m->data = data;
	m->done = (void (*) (CamelFolder *, const gchar *, CamelMimeMessage *, gpointer )) done;
	m->cancel = camel_operation_new(NULL, NULL);

	dispatch (m);

	return m->cancel;
}

/* ********************************************************************** */

struct _get_messages_msg {
	MailMsg base;

	CamelFolder *folder;
	GPtrArray *uids;
	GPtrArray *messages;

	void (*done) (CamelFolder *folder, GPtrArray *uids, GPtrArray *msgs, gpointer data);
	gpointer data;
};

static gchar *
get_messages_desc (struct _get_messages_msg *m)
{
	return g_strdup_printf(ngettext("Retrieving %d message",
					"Retrieving %d messages", m->uids->len),
			       m->uids->len);
}

static void
get_messages_exec (struct _get_messages_msg *m)
{
	gint i;
	CamelMimeMessage *message;

	for (i=0; i<m->uids->len; i++) {
		gint pc = ((i+1) * 100) / m->uids->len;

		message = camel_folder_get_message(m->folder, m->uids->pdata[i], &m->base.ex);
		camel_operation_progress(m->base.cancel, pc);
		if (message == NULL)
			break;

		g_ptr_array_add(m->messages, message);
	}
}

static void
get_messages_done (struct _get_messages_msg *m)
{
	if (m->done)
		m->done(m->folder, m->uids, m->messages, m->data);
}

static void
get_messages_free (struct _get_messages_msg *m)
{
	gint i;

	em_utils_uids_free (m->uids);
	for (i=0;i<m->messages->len;i++) {
		if (m->messages->pdata[i])
			camel_object_unref(m->messages->pdata[i]);
	}
	g_ptr_array_free(m->messages, TRUE);
	camel_object_unref(m->folder);
}

static MailMsgInfo get_messages_info = {
	sizeof (struct _get_messages_msg),
	(MailMsgDescFunc) get_messages_desc,
	(MailMsgExecFunc) get_messages_exec,
	(MailMsgDoneFunc) get_messages_done,
	(MailMsgFreeFunc) get_messages_free
};

void
mail_get_messages(CamelFolder *folder, GPtrArray *uids,
		  void (*done) (CamelFolder *folder, GPtrArray *uids, GPtrArray *msgs, gpointer data),
		  gpointer data)
{
	struct _get_messages_msg *m;

	m = mail_msg_new(&get_messages_info);
	m->folder = folder;
	camel_object_ref(folder);
	m->uids = uids;
	m->messages = g_ptr_array_new();
	m->data = data;
	m->done = done;

	mail_msg_unordered_push (m);
}

/* ** SAVE MESSAGES ******************************************************* */

struct _save_messages_msg {
	MailMsg base;

	CamelFolder *folder;
	GPtrArray *uids;
	gchar *path;
	void (*done)(CamelFolder *folder, GPtrArray *uids, gchar *path, gpointer data);
	gpointer data;
};

static gchar *
save_messages_desc (struct _save_messages_msg *m)
{
	return g_strdup_printf(ngettext("Saving %d message",
					"Saving %d messages", m->uids->len),
			       m->uids->len);
}

static void
save_prepare_part (CamelMimePart *mime_part)
{
	CamelDataWrapper *wrapper;
	gint parts, i;

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
			if (camel_content_type_is (type, "text", "*"))
				camel_mime_part_set_encoding (mime_part, CAMEL_TRANSFER_ENCODING_8BIT);
		}
	}
}

static void
save_messages_exec (struct _save_messages_msg *m)
{
	CamelStreamFilter *filtered_stream;
	CamelMimeFilterFrom *from_filter;
	CamelStream *stream;
	gint i;
	gchar *from, *path;

	if (strstr (m->path, "://"))
		path = m->path;
	else
		path = g_filename_to_uri (m->path, NULL, NULL);

	stream = camel_stream_vfs_new_with_uri (path, CAMEL_STREAM_VFS_CREATE);
	from_filter = camel_mime_filter_from_new();
	filtered_stream = camel_stream_filter_new_with_stream(stream);
	camel_stream_filter_add(filtered_stream, (CamelMimeFilter *)from_filter);
	camel_object_unref(from_filter);

	if (path != m->path)
		g_free (path);

	for (i=0; i<m->uids->len; i++) {
		CamelMimeMessage *message;
		gint pc = ((i+1) * 100) / m->uids->len;

		message = camel_folder_get_message(m->folder, m->uids->pdata[i], &m->base.ex);
		camel_operation_progress(m->base.cancel, pc);
		if (message == NULL)
			break;

		save_prepare_part (CAMEL_MIME_PART (message));

		/* we need to flush after each stream write since we are writing to the same fd */
		from = camel_mime_message_build_mbox_from(message);
		if (camel_stream_write_string(stream, from) == -1
		    || camel_stream_flush(stream) == -1
		    || camel_data_wrapper_write_to_stream((CamelDataWrapper *)message, (CamelStream *)filtered_stream) == -1
		    || camel_stream_flush((CamelStream *)filtered_stream) == -1
		    || camel_stream_write_string(stream, "\n") == -1
		    || camel_stream_flush(stream) == -1) {
			camel_exception_setv(&m->base.ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Error saving messages to: %s:\n %s"), m->path, g_strerror(errno));
			g_free(from);
			camel_object_unref((CamelObject *)message);
			break;
		}
		g_free(from);
		camel_object_unref(message);
	}

	camel_object_unref(filtered_stream);
	camel_object_unref(stream);
}

static void
save_messages_done (struct _save_messages_msg *m)
{
	if (m->done)
		m->done(m->folder, m->uids, m->path, m->data);
}

static void
save_messages_free (struct _save_messages_msg *m)
{
	em_utils_uids_free (m->uids);
	camel_object_unref(m->folder);
	g_free(m->path);
}

static MailMsgInfo save_messages_info = {
	sizeof (struct _save_messages_msg),
	(MailMsgDescFunc) save_messages_desc,
	(MailMsgExecFunc) save_messages_exec,
	(MailMsgDoneFunc) save_messages_done,
	(MailMsgFreeFunc) save_messages_free
};

gint
mail_save_messages(CamelFolder *folder, GPtrArray *uids, const gchar *path,
		   void (*done) (CamelFolder *folder, GPtrArray *uids, gchar *path, gpointer data), gpointer data)
{
	struct _save_messages_msg *m;
	gint id;

	m = mail_msg_new(&save_messages_info);
	m->folder = folder;
	camel_object_ref(folder);
	m->uids = uids;
	m->path = g_strdup(path);
	m->data = data;
	m->done = done;

	id = m->base.seq;
	mail_msg_unordered_push (m);

	return id;
}

/* ** SAVE PART ******************************************************* */

struct _save_part_msg {
	MailMsg base;

	CamelMimePart *part;
	gchar *path;
	void (*done)(CamelMimePart *part, gchar *path, gint saved, gpointer data);
	gpointer data;
	gboolean readonly;
};

static gchar *
save_part_desc (struct _save_part_msg *m)
{
	return g_strdup(_("Saving attachment"));
}

static void
save_part_exec (struct _save_part_msg *m)
{
	CamelDataWrapper *content;
	CamelStream *stream;
	gchar *path;

	if (strstr (m->path, "://"))
		path = m->path;
	else
		path = g_filename_to_uri (m->path, NULL, NULL);

	if (!m->readonly) {
		if (!(stream = camel_stream_vfs_new_with_uri (path, CAMEL_STREAM_VFS_CREATE))) {
			camel_exception_setv (&m->base.ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Cannot create output file: %s:\n %s"),
					      path, g_strerror (errno));
			if (path != m->path)
				g_free (path);
			return;
		}
	} else if (!(stream = camel_stream_vfs_new_with_uri (path, CAMEL_STREAM_VFS_CREATE))) {
		camel_exception_setv (&m->base.ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot create output file: %s:\n %s"),
				      path, g_strerror (errno));
		if (path != m->path)
			g_free (path);
		return;
	}

	if (path != m->path)
		g_free (path);

	content = camel_medium_get_content_object (CAMEL_MEDIUM (m->part));

	if (camel_data_wrapper_decode_to_stream (content, stream) == -1
	    || camel_stream_flush (stream) == -1)
		camel_exception_setv (&m->base.ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not write data: %s"),
				      g_strerror (errno));

	camel_object_unref (stream);
}

static void
save_part_done (struct _save_part_msg *m)
{
	if (m->done)
		m->done (m->part, m->path, !camel_exception_is_set (&m->base.ex), m->data);
}

static void
save_part_free (struct _save_part_msg *m)
{
	camel_object_unref (m->part);
	g_free (m->path);
}

static MailMsgInfo save_part_info = {
	sizeof (struct _save_part_msg),
	(MailMsgDescFunc) save_part_desc,
	(MailMsgExecFunc) save_part_exec,
	(MailMsgDoneFunc) save_part_done,
	(MailMsgFreeFunc) save_part_free
};

gint
mail_save_part (CamelMimePart *part, const gchar *path,
		void (*done)(CamelMimePart *part, gchar *path, gint saved, gpointer data), gpointer data, gboolean readonly)
{
	struct _save_part_msg *m;
	gint id;
	m = mail_msg_new (&save_part_info);
	m->part = part;
	camel_object_ref (part);
	m->path = g_strdup (path);
	m->data = data;
	m->done = done;
	m->readonly = readonly;

	id = m->base.seq;
	mail_msg_unordered_push (m);

	return id;
}

/* ** PREPARE OFFLINE ***************************************************** */

struct _prep_offline_msg {
	MailMsg base;

	CamelOperation *cancel;
	gchar *uri;
	void (*done)(const gchar *uri, gpointer data);
	gpointer data;
};

static void
prep_offline_exec (struct _prep_offline_msg *m)
{
	CamelFolder *folder;

	if (m->cancel)
		camel_operation_register(m->cancel);

	folder = mail_tool_uri_to_folder(m->uri, 0, &m->base.ex);
	if (folder) {
		if (CAMEL_IS_DISCO_FOLDER(folder)) {
			camel_disco_folder_prepare_for_offline((CamelDiscoFolder *)folder,
							       "(match-all)",
							       &m->base.ex);
		} else if (CAMEL_IS_OFFLINE_FOLDER (folder)) {
			camel_offline_folder_downsync ((CamelOfflineFolder *) folder, "(match-all)", &m->base.ex);
		}
		/* prepare_for_offline should do this? */
		/* of course it should all be atomic, but ... */
		camel_folder_sync(folder, FALSE, NULL);
		camel_object_unref(folder);
	}

	if (m->cancel)
		camel_operation_unregister(m->cancel);
}

static void
prep_offline_done (struct _prep_offline_msg *m)
{
	if (m->done)
		m->done(m->uri, m->data);
}

static void
prep_offline_free (struct _prep_offline_msg *m)
{
	if (m->cancel)
		camel_operation_unref(m->cancel);
	g_free(m->uri);
}

static MailMsgInfo prep_offline_info = {
	sizeof (struct _prep_offline_msg),
	(MailMsgDescFunc) NULL, /* DO NOT CHANGE THIS, IT MUST BE NULL FOR CANCELLATION TO WORK */
	(MailMsgExecFunc) prep_offline_exec,
	(MailMsgDoneFunc) prep_offline_done,
	(MailMsgFreeFunc) prep_offline_free
};

void
mail_prep_offline(const gchar *uri,
		  CamelOperation *cancel,
		  void (*done)(const gchar *, gpointer data),
		  gpointer data)
{
	struct _prep_offline_msg *m;

	m = mail_msg_new(&prep_offline_info);
	m->cancel = cancel;
	if (cancel)
		camel_operation_ref(cancel);
	m->uri = g_strdup(uri);
	m->data = data;
	m->done = done;

	mail_msg_slow_ordered_push (m);
}

/* ** GO OFFLINE ***************************************************** */

struct _set_offline_msg {
	MailMsg base;

	CamelStore *store;
	gboolean offline;
	void (*done)(CamelStore *store, gpointer data);
	gpointer data;
};

static gchar *
set_offline_desc (struct _set_offline_msg *m)
{
	gchar *service_name = camel_service_get_name (CAMEL_SERVICE (m->store), TRUE);
	gchar *msg;

	msg = g_strdup_printf(m->offline?_("Disconnecting from %s"):_("Reconnecting to %s"),
			      service_name);
	g_free(service_name);
	return msg;
}

static void
set_offline_exec (struct _set_offline_msg *m)
{
	if (CAMEL_IS_DISCO_STORE (m->store)) {
		if (!m->offline) {
			camel_disco_store_set_status (CAMEL_DISCO_STORE (m->store),
						      CAMEL_DISCO_STORE_ONLINE,
						      &m->base.ex);
			return;
		} else if (camel_disco_store_can_work_offline (CAMEL_DISCO_STORE (m->store))) {
			camel_disco_store_set_status (CAMEL_DISCO_STORE (m->store),
						      CAMEL_DISCO_STORE_OFFLINE,
						      &m->base.ex);
			return;
		}
	} else if (CAMEL_IS_OFFLINE_STORE (m->store)) {
		if (!m->offline) {
			camel_offline_store_set_network_state (CAMEL_OFFLINE_STORE (m->store),
							       CAMEL_OFFLINE_STORE_NETWORK_AVAIL,
							       &m->base.ex);
			return;
		} else {
			camel_offline_store_set_network_state (CAMEL_OFFLINE_STORE (m->store),
							       CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL,
							       &m->base.ex);
			return;
		}
	}

	if (m->offline)
		camel_service_disconnect (CAMEL_SERVICE (m->store),
					  TRUE, &m->base.ex);
}

static void
set_offline_done (struct _set_offline_msg *m)
{
	if (m->done)
		m->done(m->store, m->data);
}

static void
set_offline_free (struct _set_offline_msg *m)
{
	camel_object_unref(m->store);
}

static MailMsgInfo set_offline_info = {
	sizeof (struct _set_offline_msg),
	(MailMsgDescFunc) set_offline_desc,
	(MailMsgExecFunc) set_offline_exec,
	(MailMsgDoneFunc) set_offline_done,
	(MailMsgFreeFunc) set_offline_free
};

gint
mail_store_set_offline (CamelStore *store, gboolean offline,
			void (*done)(CamelStore *, gpointer data),
			gpointer data)
{
	struct _set_offline_msg *m;
	gint id;

	/* Cancel any pending connect first so the set_offline_op
	 * thread won't get queued behind a hung connect op.
	 */
	if (offline)
		camel_service_cancel_connect (CAMEL_SERVICE (store));

	m = mail_msg_new(&set_offline_info);
	m->store = store;
	camel_object_ref(store);
	m->offline = offline;
	m->data = data;
	m->done = done;

	id = m->base.seq;
	mail_msg_unordered_push (m);

	return id;
}

/* ** Prepare OFFLINE ***************************************************** */

static gchar *
prepare_offline_desc (struct _set_offline_msg *m)
{
	gchar *service_name = camel_service_get_name (CAMEL_SERVICE (m->store), TRUE);
	gchar *msg;

	msg = g_strdup_printf (_("Preparing account '%s' for offline"), service_name);
	g_free(service_name);

	return msg;
}

static void
prepare_offline_exec (struct _set_offline_msg *m)
{
	if (CAMEL_IS_DISCO_STORE (m->store)) {
		camel_disco_store_prepare_for_offline (CAMEL_DISCO_STORE (m->store),
					       &m->base.ex);
	} else if (CAMEL_IS_OFFLINE_STORE (m->store)) {
		camel_offline_store_prepare_for_offline (CAMEL_OFFLINE_STORE (m->store),
							 &m->base.ex);
	}
}

static void
prepare_offline_done (struct _set_offline_msg *m)
{
	if (m->done)
		m->done (m->store, m->data);
}

static void
prepare_offline_free (struct _set_offline_msg *m)
{
	camel_object_unref (m->store);
}

static MailMsgInfo prepare_offline_info = {
	sizeof (struct _set_offline_msg),
	(MailMsgDescFunc) prepare_offline_desc,
	(MailMsgExecFunc) prepare_offline_exec,
	(MailMsgDoneFunc) prepare_offline_done,
	(MailMsgFreeFunc) prepare_offline_free
};

gint
mail_store_prepare_offline (CamelStore *store)
{
	struct _set_offline_msg *m;
	gint id;

	/* Cancel any pending connect first so the set_offline_op
	 * thread won't get queued behind a hung connect op.
	 */

	m = mail_msg_new(&prepare_offline_info);
	m->store = store;
	camel_object_ref(store);
	m->data = NULL;
	m->done = NULL;

	id = m->base.seq;
	mail_msg_unordered_push (m);

	return id;
}
/* ** Execute Shell Command ***************************************************** */

void
mail_execute_shell_command (CamelFilterDriver *driver, gint argc, gchar **argv, gpointer data)
{
	if (argc <= 0)
		return;

	g_spawn_async (NULL, argv, NULL, 0, NULL, data, NULL, NULL);
}

/* Async service-checking/authtype-lookup code. */
struct _check_msg {
	MailMsg base;

	gchar *url;
	CamelProviderType type;
	GList *authtypes;

	void (*done)(const gchar *url, CamelProviderType type, GList *types, gpointer data);
	gpointer data;
};

static gchar *
check_service_desc (struct _check_msg *m)
{
	return g_strdup(_("Checking Service"));
}

static void
check_service_exec (struct _check_msg *m)
{
	CamelService *service;

	service = camel_session_get_service(session, m->url, m->type, &m->base.ex);
	if (!service) {
		camel_operation_unregister(m->base.cancel);
		return;
	}

	m->authtypes = camel_service_query_auth_types(service, &m->base.ex);
	camel_object_unref(service);
}

static void
check_service_done (struct _check_msg *m)
{
	if (m->done)
		m->done(m->url, m->type, m->authtypes, m->data);
}

static void
check_service_free (struct _check_msg *m)
{
	g_free(m->url);
	g_list_free(m->authtypes);
}

static MailMsgInfo check_service_info = {
	sizeof (struct _check_msg),
	(MailMsgDescFunc) check_service_desc,
	(MailMsgExecFunc) check_service_exec,
	(MailMsgDoneFunc) check_service_done,
	(MailMsgFreeFunc) check_service_free
};

gint
mail_check_service(const gchar *url, CamelProviderType type, void (*done)(const gchar *url, CamelProviderType type, GList *authtypes, gpointer data), gpointer data)
{
	struct _check_msg *m;
	gint id;

	m = mail_msg_new (&check_service_info);
	m->url = g_strdup(url);
	m->type = type;
	m->done = done;
	m->data = data;

	id = m->base.seq;
	mail_msg_unordered_push (m);

	return id;
}
