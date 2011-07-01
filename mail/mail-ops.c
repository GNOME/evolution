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
 *      Dan Winship <danw@ximian.com>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *      Peter Williams <peterw@ximian.com>
 *      Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>

#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <libedataserver/e-data-server-util.h>
#include "e-util/e-account-utils.h"

#include "em-filter-rule.h"
#include "em-utils.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"

#include "e-mail-local.h"
#include "e-mail-session.h"

#define w(x)
#define d(x)

/* XXX Make this a preprocessor definition. */
const gchar *x_mailer = "Evolution " VERSION SUB_VERSION " " VERSION_COMMENT;

/* used for both just filtering a folder + uid's, and for filtering a whole folder */
/* used both for fetching mail, and for filtering mail */
struct _filter_mail_msg {
	MailMsg base;

	EMailSession *session;
	CamelFolder *source_folder; /* where they come from */
	GPtrArray *source_uids;	/* uids to copy, or NULL == copy all */
	CamelUIDCache *cache;  /* UID cache if we are to cache the uids, NULL otherwise */
	CamelFilterDriver *driver;
	gint delete;		/* delete messages after filtering them? */
	CamelFolder *destination; /* default destination for any messages, NULL for none */
};

/* since fetching also filters, we subclass the data here */
struct _fetch_mail_msg {
	struct _filter_mail_msg fmsg;

	CamelStore *store;
	GCancellable *cancellable;	/* we have our own cancellation
					 * struct, the other should be empty */
	gint keep;		/* keep on server? */

	void (*done)(gpointer data);
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
em_filter_folder_element_exec (struct _filter_mail_msg *m,
                               GCancellable *cancellable,
                               GError **error)
{
	CamelFolder *folder;
	GPtrArray *uids, *folder_uids = NULL;

	folder = m->source_folder;

	if (folder == NULL || camel_folder_get_message_count (folder) == 0)
		return;

	if (m->destination) {
		camel_folder_freeze (m->destination);
		camel_filter_driver_set_default_folder (m->driver, m->destination);
	}

	camel_folder_freeze (folder);

	if (m->source_uids)
		uids = m->source_uids;
	else
		folder_uids = uids = camel_folder_get_uids (folder);

	camel_filter_driver_filter_folder (
		m->driver, folder, m->cache, uids, m->delete,
		cancellable, error);
	camel_filter_driver_flush (m->driver, error);

	if (folder_uids)
		camel_folder_free_uids (folder, folder_uids);

	/* sync our source folder */
	if (!m->cache)
		camel_folder_synchronize_sync (
			folder, FALSE, cancellable, error);
	camel_folder_thaw (folder);

	if (m->destination)
		camel_folder_thaw (m->destination);

	/* this may thaw/unref source folders, do it here so we dont do
	 * it in the main thread see also fetch_mail_fetch () below */
	g_object_unref (m->driver);
	m->driver = NULL;
}

static void
em_filter_folder_element_done (struct _filter_mail_msg *m)
{
}

static void
em_filter_folder_element_free (struct _filter_mail_msg *m)
{
	mail_session_flush_filter_log (m->session);

	if (m->session)
		g_object_unref (m->session);

	if (m->source_folder)
		g_object_unref (m->source_folder);

	if (m->source_uids)
		em_utils_uids_free (m->source_uids);

	if (m->destination)
		g_object_unref (m->destination);

	if (m->driver)
		g_object_unref (m->driver);
}

static MailMsgInfo em_filter_folder_element_info = {
	sizeof (struct _filter_mail_msg),
	(MailMsgDescFunc) em_filter_folder_element_desc,
	(MailMsgExecFunc) em_filter_folder_element_exec,
	(MailMsgDoneFunc) em_filter_folder_element_done,
	(MailMsgFreeFunc) em_filter_folder_element_free
};

void
mail_filter_folder (EMailSession *session,
                    CamelFolder *source_folder,
                    GPtrArray *uids,
                    const gchar *type,
                    gboolean notify)
{
	struct _filter_mail_msg *m;

	m = mail_msg_new (&em_filter_folder_element_info);
	m->session = g_object_ref (session);
	m->source_folder = g_object_ref (source_folder);
	m->source_uids = uids;
	m->cache = NULL;
	m->delete = FALSE;

	m->driver = camel_session_get_filter_driver (
		CAMEL_SESSION (session), type, NULL);

	if (!notify) {
		/* FIXME: have a #define NOTIFY_FILTER_NAME macro? */
		/* the filter name has to stay in sync with mail-session::get_filter_driver */
		camel_filter_driver_remove_rule_by_name (m->driver, "new-mail-notification");
	}

	mail_msg_unordered_push (m);
}

/* ********************************************************************** */

static gchar *
fetch_mail_desc (struct _fetch_mail_msg *m)
{
	return g_strdup (_("Fetching Mail"));
}

static void
fetch_mail_exec (struct _fetch_mail_msg *m,
                 GCancellable *cancellable,
                 GError **error)
{
	struct _filter_mail_msg *fm = (struct _filter_mail_msg *) m;
	CamelFolder *folder = NULL;
	CamelURL *url;
	const gchar *uid;
	gboolean is_local_delivery;
	gint i;

	fm->destination = e_mail_local_get_folder (
		E_MAIL_LOCAL_FOLDER_LOCAL_INBOX);
	if (fm->destination == NULL)
		goto fail;
	g_object_ref (fm->destination);

	url = camel_service_get_camel_url (CAMEL_SERVICE (m->store));
	is_local_delivery = em_utils_is_local_delivery_mbox_file (url);
	if (is_local_delivery) {
		gchar *path;
		gchar *url_string;

		path = mail_tool_do_movemail (m->store, error);
		url_string = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);

		if (path && (!error || !*error)) {
			camel_folder_freeze (fm->destination);
			camel_filter_driver_set_default_folder (
				fm->driver, fm->destination);
			camel_filter_driver_filter_mbox (
				fm->driver, path, url_string,
				cancellable, error);
			camel_folder_thaw (fm->destination);

			if (!error || !*error)
				g_unlink (path);
		}

		g_free (path);
		g_free (url_string);
	} else {
		uid = camel_service_get_uid (CAMEL_SERVICE (m->store));

		folder = fm->source_folder =
			e_mail_session_get_inbox_sync (
				fm->session, uid, cancellable, error);
	}

	if (folder != NULL) {
		/* This handles 'keep on server' stuff, if we have any new
		 * uid's to copy across, we need to copy them to a new array
		 * 'cause of the way fetch_mail_free works. */
		CamelUIDCache *cache = NULL;
		CamelStore *parent_store;
		CamelService *service;
		const gchar *data_dir;
		gchar *cachename;

		parent_store = camel_folder_get_parent_store (folder);

		service = CAMEL_SERVICE (parent_store);
		data_dir = camel_service_get_user_data_dir (service);

		cachename = g_build_filename (data_dir, "uid-cache", NULL);
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
				em_filter_folder_element_exec (fm, cancellable, error);

				/* need to uncancel so writes/etc. don't fail */
				if (g_cancellable_is_cancelled (m->cancellable))
					g_cancellable_reset (m->cancellable);

				/* save the cache of uids that we've just downloaded */
				camel_uid_cache_save (cache);
			}

			if (fm->delete && (!error || !*error)) {
				/* not keep on server - just delete all
				 * the actual messages on the server */
				for (i=0;i<folder_uids->len;i++) {
					camel_folder_delete_message (
						folder, folder_uids->pdata[i]);
				}
			}

			if ((fm->delete || cache_uids) && (!error || !*error)) {
				/* expunge messages (downloaded so far) */
				/* FIXME Not passing a GCancellable or GError here. */
				camel_folder_synchronize_sync (
					folder, fm->delete, NULL, NULL);
			}

			camel_uid_cache_destroy (cache);
			camel_folder_free_uids (folder, folder_uids);
		} else {
			em_filter_folder_element_exec (fm, cancellable, error);
		}

		/* we unref the source folder here since we
		   may now block in finalize (we try to
		   disconnect cleanly) */
		g_object_unref (fm->source_folder);
		fm->source_folder = NULL;
	}

fail:
	/* we unref this here as it may have more work to do (syncing
	   folders and whatnot) before we are really done */
	/* should this be cancellable too? (i.e. above unregister above) */
	if (fm->driver) {
		g_object_unref (fm->driver);
		fm->driver = NULL;
	}

	/* also disconnect if not a local delivery mbox;
	   there is no need to keep the connection alive forever */
	if (!is_local_delivery)
		em_utils_disconnect_service_sync (
			CAMEL_SERVICE (m->store), TRUE, cancellable, NULL);
}

static void
fetch_mail_done (struct _fetch_mail_msg *m)
{
	if (m->done)
		m->done (m->data);
}

static void
fetch_mail_free (struct _fetch_mail_msg *m)
{
	if (m->store != NULL)
		g_object_unref (m->store);

	if (m->cancellable != NULL)
		g_object_unref (m->cancellable);

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
mail_fetch_mail (CamelStore *store,
                 gint keep,
                 const gchar *type,
                 GCancellable *cancellable,
                 CamelFilterGetFolderFunc get_folder,
                 gpointer get_data,
                 CamelFilterStatusFunc *status,
                 gpointer status_data,
                 void (*done)(gpointer data),
                 gpointer data)
{
	struct _fetch_mail_msg *m;
	struct _filter_mail_msg *fm;
	CamelSession *session;

	g_return_if_fail (CAMEL_IS_STORE (store));

	session = camel_service_get_session (CAMEL_SERVICE (store));

	m = mail_msg_new (&fetch_mail_info);
	fm = (struct _filter_mail_msg *) m;
	fm->session = g_object_ref (session);
	m->store = g_object_ref (store);
	fm->delete = !keep;
	fm->cache = NULL;
	if (cancellable)
		m->cancellable = g_object_ref (cancellable);
	m->done = done;
	m->data = data;

	fm->driver = camel_session_get_filter_driver (session, type, NULL);
	camel_filter_driver_set_folder_func (fm->driver, get_folder, get_data);
	if (status)
		camel_filter_driver_set_status_func (fm->driver, status, status_data);

	mail_msg_unordered_push (m);
}

static gchar *
escape_percent_sign (const gchar *str)
{
	GString *res;

	if (!str)
		return NULL;

	res = g_string_sized_new (strlen (str));
	while (*str) {
		if (*str == '%') {
			g_string_append (res, "%%");
		} else {
			g_string_append_c (res, *str);
		}

		str++;
	}

	return g_string_free (res, FALSE);
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

struct _send_queue_msg {
	MailMsg base;

	EMailSession *session;
	CamelFolder *queue;
	CamelTransport *transport;

	CamelFilterDriver *driver;

	/* we use camelfilterstatusfunc, even though its not the filter doing it */
	CamelFilterStatusFunc *status;
	gpointer status_data;

	void (*done)(gpointer data);
	gpointer data;
};

static void	report_status		(struct _send_queue_msg *m,
					 enum camel_filter_status_t status,
					 gint pc,
					 const gchar *desc,
					 ...);

/* send 1 message to a specific transport */
static void
mail_send_message (struct _send_queue_msg *m,
                   CamelFolder *queue,
                   const gchar *uid,
                   CamelTransport *transport,
                   CamelFilterDriver *driver,
                   GCancellable *cancellable,
                   GError **error)
{
	EAccount *account = NULL;
	const CamelInternetAddress *iaddr;
	CamelAddress *from, *recipients;
	CamelMessageInfo *info = NULL;
	CamelProvider *provider;
	gchar *transport_uid = NULL;
	gchar *sent_folder_uri = NULL;
	const gchar *resent_from, *tmp;
	CamelFolder *folder = NULL;
	GString *err = NULL;
	struct _camel_header_raw *xev, *header;
	CamelMimeMessage *message;
	gint i;
	GError *local_error = NULL;

	message = camel_folder_get_message_sync (
		queue, uid, cancellable, error);
	if (!message)
		return;

	camel_medium_set_header (CAMEL_MEDIUM (message), "X-Mailer", x_mailer);

	err = g_string_new("");
	xev = mail_tool_remove_xevolution_headers (message);

	tmp = camel_header_raw_find(&xev, "X-Evolution-Account", NULL);
	if (tmp) {
		gchar *name;

		name = g_strstrip (g_strdup (tmp));
		if ((account = e_get_account_by_uid (name))
		    /* 'old' x-evolution-account stored the name, how silly */
		    || (account = e_get_account_by_name (name))) {
			if (account->transport) {
				CamelService *service;
				gchar *transport_uid;

				transport_uid = g_strconcat (
					account->uid, "-transport", NULL);
				service = camel_session_get_service (
					CAMEL_SESSION (m->session),
					transport_uid);
				g_free (transport_uid);

				if (CAMEL_IS_TRANSPORT (service))
					transport = CAMEL_TRANSPORT (service);
			}

			sent_folder_uri = g_strdup (account->sent_folder_uri);
		}
		g_free (name);
	}

	if (!account) {
		/* default back to these headers */
		tmp = camel_header_raw_find(&xev, "X-Evolution-Transport", NULL);
		if (tmp)
			transport_uid = g_strstrip (g_strdup (tmp));

		tmp = camel_header_raw_find(&xev, "X-Evolution-Fcc", NULL);
		if (tmp)
			sent_folder_uri = g_strstrip (g_strdup (tmp));
	}

	if (transport != NULL) {
		CamelURL *url;
		gchar *url_string;
		gchar *escaped;

		url = camel_service_get_camel_url (CAMEL_SERVICE (transport));
		url_string = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
		escaped = escape_percent_sign (url_string);

		/* Let the dialog know the right account it is using. */
		report_status (m, CAMEL_FILTER_STATUS_ACTION, 0, escaped);

		g_free (escaped);
		g_free (url_string);
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

	if (camel_address_length (recipients) > 0) {
		if (!em_utils_connect_service_sync (
			CAMEL_SERVICE (transport), cancellable, error))
			goto exit;

		if (!camel_transport_send_to_sync (
			transport, message, from,
			recipients, cancellable, error))
			goto exit;
	}

	/* Now check for posting, failures are ignored */
	info = camel_message_info_new (NULL);
	camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN, ~0);

	for (header = xev;header;header=header->next) {
		gchar *uri;

		if (strcmp(header->name, "X-Evolution-PostTo") != 0)
			continue;

		/* TODO: don't lose errors */

		uri = g_strstrip (g_strdup (header->value));
		/* FIXME Not passing a GCancellable or GError here. */
		folder = e_mail_session_uri_to_folder_sync (
			m->session, uri, 0, NULL, NULL);
		if (folder) {
			/* FIXME Not passing a GCancellable or GError here. */
			camel_folder_append_message_sync (
				folder, message, info, NULL, NULL, NULL);
			g_object_unref (folder);
			folder = NULL;
		}
		g_free (uri);
	}

	/* post process */
	mail_tool_restore_xevolution_headers (message, xev);

	if (driver) {
		camel_filter_driver_filter_message (
			driver, message, info, NULL, NULL,
			NULL, "", cancellable, &local_error);

		if (local_error != NULL) {
			if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
				goto exit;

			/* sending mail, filtering failed */
			g_string_append_printf (
				err, _("Failed to apply outgoing filters: %s"),
				local_error->message);

			g_clear_error (&local_error);
		}
	}

	provider = camel_service_get_provider (CAMEL_SERVICE (transport));

	if (provider == NULL
	    || !(provider->flags & CAMEL_PROVIDER_DISABLE_SENT_FOLDER)) {
		GError *local_error = NULL;

		if (sent_folder_uri) {
			folder = e_mail_session_uri_to_folder_sync (
				m->session, sent_folder_uri, 0,
				cancellable, &local_error);
			if (folder == NULL) {
				g_string_append_printf (
					err, _("Failed to append to %s: %s\n"
					"Appending to local 'Sent' folder instead."),
					sent_folder_uri,
					local_error ?
						local_error->message :
						_("Unknown error"));
				if (local_error)
					g_clear_error (&local_error);
			}
		}

		if (!folder) {
			folder = e_mail_local_get_folder (E_MAIL_LOCAL_FOLDER_SENT);
			g_object_ref (folder);
		}

		if (!camel_folder_append_message_sync (
			folder, message, info,
			NULL, cancellable, &local_error)) {

			CamelFolder *sent_folder;

			if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
				goto exit;

			sent_folder = e_mail_local_get_folder (E_MAIL_LOCAL_FOLDER_SENT);

			if (folder != sent_folder) {
				const gchar *description;

				description = camel_folder_get_description (folder);
				if (err->len)
					g_string_append(err, "\n\n");
				g_string_append_printf (
					err, _("Failed to append to %s: %s\n"
					"Appending to local 'Sent' folder instead."),
					description, local_error->message);
				g_object_ref (sent_folder);
				g_object_unref (folder);
				folder = sent_folder;

				g_clear_error (&local_error);
				camel_folder_append_message_sync (
					folder, message, info,
					NULL, cancellable, &local_error);
			}

			if (local_error != NULL) {
				if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
					goto exit;

				if (err->len)
					g_string_append(err, "\n\n");
				g_string_append_printf (
					err, _("Failed to append to local 'Sent' folder: %s"),
					local_error->message);
			}
		}
	}

	if (local_error == NULL) {
		camel_folder_set_message_flags (
			queue, uid, CAMEL_MESSAGE_DELETED |
			CAMEL_MESSAGE_SEEN, ~0);
		/* Sync it to disk, since if it crashes in between,
		 * we keep sending it again on next start. */
		/* FIXME Not passing a GCancellable or GError here. */
		camel_folder_synchronize_sync (queue, FALSE, NULL, NULL);
	}

	if (err->len) {
		/* set the culmulative exception report */
		g_set_error (
			&local_error, CAMEL_ERROR,
			CAMEL_ERROR_GENERIC, "%s", err->str);
	}

exit:
	if (local_error != NULL)
		g_propagate_error (error, local_error);

	/* FIXME Not passing a GCancellable or GError here. */
	if (folder) {
		camel_folder_synchronize_sync (folder, FALSE, NULL, NULL);
		g_object_unref (folder);
	}
	if (info)
		camel_message_info_free (info);
	g_object_unref (recipients);
	g_object_unref (from);
	g_free (sent_folder_uri);
	g_free (transport_uid);
	camel_header_raw_clear (&xev);
	g_string_free (err, TRUE);
	g_object_unref (message);

	return;
}

/* ** SEND MAIL QUEUE ***************************************************** */

static void
report_status (struct _send_queue_msg *m,
               enum camel_filter_status_t status,
               gint pc,
               const gchar *desc,
               ...)
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
send_queue_exec (struct _send_queue_msg *m,
                 GCancellable *cancellable,
                 GError **error)
{
	CamelFolder *sent_folder;
	GPtrArray *uids, *send_uids = NULL;
	gint i, j;
	GError *local_error = NULL;

	d(printf("sending queue\n"));

	sent_folder = e_mail_local_get_folder (E_MAIL_LOCAL_FOLDER_SENT);

	if (!(uids = camel_folder_get_uids (m->queue)))
		return;

	send_uids = g_ptr_array_sized_new (uids->len);
	for (i = 0, j = 0; i < uids->len; i++) {
		CamelMessageInfo *info;

		info = camel_folder_get_message_info (m->queue, uids->pdata[i]);
		if (info) {
			if ((camel_message_info_flags (info) & CAMEL_MESSAGE_DELETED) == 0)
				send_uids->pdata[j++] = uids->pdata[i];
			camel_folder_free_message_info (m->queue, info);
		}
	}

	send_uids->len = j;
	if (send_uids->len == 0) {
		/* nothing to send */
		camel_folder_free_uids (m->queue, uids);
		g_ptr_array_free (send_uids, TRUE);
		return;
	}

	camel_operation_push_message (cancellable, _("Sending message"));

	/* NB: This code somewhat abuses the 'exception' stuff.  Apart from
	 *     fatal problems, it is also used as a mechanism to accumualte
	 *     warning messages and present them back to the user. */

	for (i = 0, j = 0; i < send_uids->len; i++) {
		gint pc = (100 * i) / send_uids->len;

		report_status (
			m, CAMEL_FILTER_STATUS_START, pc,
			_("Sending message %d of %d"), i+1,
			send_uids->len);

		camel_operation_progress (
			cancellable, (i+1) * 100 / send_uids->len);

		mail_send_message (
			m, m->queue, send_uids->pdata[i], m->transport,
			m->driver, cancellable, &local_error);
		if (local_error != NULL) {
			if (!g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
				/* merge exceptions into one */
				if (m->base.error != NULL) {
					gchar *old_message;

					old_message = g_strdup (
						m->base.error->message);
					g_clear_error (&m->base.error);
					g_set_error (
						&m->base.error, CAMEL_ERROR,
						CAMEL_ERROR_GENERIC,
						"%s\n\n%s", old_message,
						local_error->message);
					g_free (old_message);

					g_clear_error (&local_error);
				} else {
					g_propagate_error (&m->base.error, local_error);
					local_error = NULL;
				}

				/* keep track of the number of failures */
				j++;
			} else {
				/* transfer the USER_CANCEL error to the
				 * async op exception and then break */
				g_propagate_error (&m->base.error, local_error);
				local_error = NULL;
				break;
			}
		}
	}

	j += (send_uids->len - i);

	if (j > 0)
		report_status (
			m, CAMEL_FILTER_STATUS_END, 100,
			_("Failed to send %d of %d messages"),
			j, send_uids->len);
	else if (g_error_matches (
			m->base.error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		report_status (m, CAMEL_FILTER_STATUS_END, 100, _("Canceled."));
	else
		report_status (m, CAMEL_FILTER_STATUS_END, 100, _("Complete."));

	if (m->driver) {
		g_object_unref (m->driver);
		m->driver = NULL;
	}

	camel_folder_free_uids (m->queue, uids);
	g_ptr_array_free (send_uids, TRUE);

	/* FIXME Not passing a GCancellable or GError here. */
	if (j <= 0 && m->base.error == NULL)
		camel_folder_synchronize_sync (m->queue, TRUE, NULL, NULL);

	/* FIXME Not passing a GCancellable or GError here. */
	if (sent_folder)
		camel_folder_synchronize_sync (sent_folder, FALSE, NULL, NULL);

	camel_operation_pop_message (cancellable);
}

static void
send_queue_done (struct _send_queue_msg *m)
{
	if (m->done)
		m->done (m->data);
}

static gchar *
send_queue_desc (struct _send_queue_msg *m)
{
	return g_strdup (_("Sending message"));
}

static void
send_queue_free (struct _send_queue_msg *m)
{
	if (m->session != NULL)
		g_object_unref (m->session);
	if (m->driver != NULL)
		g_object_unref (m->driver);
	if (m->transport != NULL)
		g_object_unref (m->transport);
	g_object_unref (m->queue);
}

static MailMsgInfo send_queue_info = {
	sizeof (struct _send_queue_msg),
	(MailMsgDescFunc) send_queue_desc,
	(MailMsgExecFunc) send_queue_exec,
	(MailMsgDoneFunc) send_queue_done,
	(MailMsgFreeFunc) send_queue_free
};

/* same interface as fetch_mail, just 'cause i'm lazy today
 * (and we need to run it from the same spot?) */
void
mail_send_queue (EMailSession *session,
                 CamelFolder *queue,
                 CamelTransport *transport,
                 const gchar *type,
                 GCancellable *cancellable,
                 CamelFilterGetFolderFunc get_folder,
                 gpointer get_data,
                 CamelFilterStatusFunc *status,
                 gpointer status_data,
                 void (*done)(gpointer data),
                 gpointer data)
{
	struct _send_queue_msg *m;

	m = mail_msg_new (&send_queue_info);
	m->session = g_object_ref (session);
	m->queue = g_object_ref (queue);
	m->transport = g_object_ref (transport);
	if (G_IS_CANCELLABLE (cancellable))
		e_activity_set_cancellable (m->base.activity, cancellable);
	m->status = status;
	m->status_data = status_data;
	m->done = done;
	m->data = data;

	m->driver = camel_session_get_filter_driver (
		CAMEL_SESSION (session), type, NULL);
	camel_filter_driver_set_folder_func (m->driver, get_folder, get_data);

	mail_msg_unordered_push (m);
}

/* ** TRANSFER MESSAGES **************************************************** */

struct _transfer_msg {
	MailMsg base;

	EMailSession *session;
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
	return g_strdup_printf (
		m->delete ?
			_("Moving messages to '%s'") :
			_("Copying messages to '%s'"),
		m->dest_uri);

}

static void
transfer_messages_exec (struct _transfer_msg *m,
                        GCancellable *cancellable,
                        GError **error)
{
	CamelFolder *dest;

	dest = e_mail_session_uri_to_folder_sync (
		m->session, m->dest_uri, m->dest_flags,
		cancellable, error);
	if (dest == NULL)
		return;

	if (dest == m->source) {
		g_object_unref (dest);
		/* no-op */
		return;
	}

	camel_folder_freeze (m->source);
	camel_folder_freeze (dest);

	camel_folder_transfer_messages_to_sync (
		m->source, m->uids, dest, m->delete, NULL,
		cancellable, error);

	/* make sure all deleted messages are marked as seen */

	if (m->delete) {
		gint i;

		for (i = 0; i < m->uids->len; i++)
			camel_folder_set_message_flags (
				m->source, m->uids->pdata[i],
				CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	}

	camel_folder_thaw (m->source);
	camel_folder_thaw (dest);

	/* FIXME Not passing a GCancellable or GError here. */
	camel_folder_synchronize_sync (dest, FALSE, NULL, NULL);
	g_object_unref (dest);
}

static void
transfer_messages_done (struct _transfer_msg *m)
{
	if (m->done)
		m->done (m->base.error == NULL, m->data);
}

static void
transfer_messages_free (struct _transfer_msg *m)
{
	g_object_unref (m->session);
	g_object_unref (m->source);
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
mail_transfer_messages (EMailSession *session,
                        CamelFolder *source,
                        GPtrArray *uids,
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

	m = mail_msg_new (&transfer_messages_info);
	m->session = g_object_ref (session);
	m->source = g_object_ref (source);
	m->uids = uids;
	m->delete = delete_from_source;
	m->dest_uri = g_strdup (dest_uri);
	m->dest_flags = dest_flags;
	m->done = done;
	m->data = data;

	mail_msg_slow_ordered_push (m);
}

/* ** SYNC FOLDER ********************************************************* */

struct _sync_folder_msg {
	MailMsg base;

	EMailSession *session;
	CamelFolder *folder;
	void (*done) (CamelFolder *folder, gpointer data);
	gpointer data;
};

static gchar *
sync_folder_desc (struct _sync_folder_msg *m)
{
	return g_strdup_printf (_("Storing folder '%s'"),
			       camel_folder_get_full_name (m->folder));
}

static void
sync_folder_exec (struct _sync_folder_msg *m,
                  GCancellable *cancellable,
                  GError **error)
{
	camel_folder_synchronize_sync (
		m->folder, FALSE, cancellable, error);
}

static void
sync_folder_done (struct _sync_folder_msg *m)
{
	if (m->done)
		m->done (m->folder, m->data);
}

static void
sync_folder_free (struct _sync_folder_msg *m)
{
	g_object_unref (m->folder);

	if (m->session)
		g_object_unref (m->session);
}

static MailMsgInfo sync_folder_info = {
	sizeof (struct _sync_folder_msg),
	(MailMsgDescFunc) sync_folder_desc,
	(MailMsgExecFunc) sync_folder_exec,
	(MailMsgDoneFunc) sync_folder_done,
	(MailMsgFreeFunc) sync_folder_free
};

void
mail_sync_folder (CamelFolder *folder,
                  void (*done) (CamelFolder *folder, gpointer data),
                  gpointer data)
{
	struct _sync_folder_msg *m;

	m = mail_msg_new (&sync_folder_info);
	m->folder = folder;
	g_object_ref (folder);
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
	CamelURL *url;
	gchar *uri, *res;

	url = camel_service_get_camel_url (CAMEL_SERVICE (m->store));
	uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);

	res = g_strdup_printf (m->expunge
			      ?_("Expunging and storing account '%s'")
			      :_("Storing account '%s'"),
			      uri);
	g_free (uri);

	return res;
}

static void
sync_store_exec (struct _sync_store_msg *m,
                 GCancellable *cancellable,
                 GError **error)
{
	camel_store_synchronize_sync (
		m->store, m->expunge,
		cancellable, error);
}

static void
sync_store_done (struct _sync_store_msg *m)
{
	if (m->done)
		m->done (m->store, m->data);
}

static void
sync_store_free (struct _sync_store_msg *m)
{
	g_object_unref (m->store);
}

static MailMsgInfo sync_store_info = {
	sizeof (struct _sync_store_msg),
	(MailMsgDescFunc) sync_store_desc,
	(MailMsgExecFunc) sync_store_exec,
	(MailMsgDoneFunc) sync_store_done,
	(MailMsgFreeFunc) sync_store_free
};

void
mail_sync_store (CamelStore *store,
                 gint expunge,
                 void (*done) (CamelStore *store, gpointer data),
                 gpointer data)
{
	struct _sync_store_msg *m;

	m = mail_msg_new (&sync_store_info);
	m->store = store;
	m->expunge = expunge;
	g_object_ref (store);
	m->data = data;
	m->done = done;

	mail_msg_slow_ordered_push (m);
}

/* ******************************************************************************** */

static gchar *
refresh_folder_desc (struct _sync_folder_msg *m)
{
	return g_strdup_printf (
		_("Refreshing folder '%s'"),
		camel_folder_get_full_name (m->folder));
}

static void
refresh_folder_exec (struct _sync_folder_msg *m,
                     GCancellable *cancellable,
                     GError **error)
{
	camel_folder_refresh_info_sync (
		m->folder, cancellable, error);
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
mail_refresh_folder (CamelFolder *folder,
                     void (*done) (CamelFolder *folder, gpointer data),
                     gpointer data)
{
	struct _sync_folder_msg *m;

	m = mail_msg_new (&refresh_folder_info);
	m->folder = folder;
	g_object_ref (folder);
	m->data = data;
	m->done = done;

	mail_msg_slow_ordered_push (m);
}

/* ******************************************************************************** */

static gboolean
folder_is_from_source_uid (CamelFolder *folder,
                           const gchar *source_uid)
{
	CamelStore *store;
	const gchar *uid;

	store = camel_folder_get_parent_store (folder);
	uid = camel_service_get_uid (CAMEL_SERVICE (store));

	return (g_strcmp0 (uid, source_uid) == 0);
}

/* This is because pop3 accounts are hidden under local Inbox,
 * thus whenever an expunge is done on a local trash or Inbox,
 * then also all active pop3 accounts should be expunged. */
static void
expunge_pop3_stores (CamelFolder *expunging,
                     EMailSession *session,
                     GCancellable *cancellable,
                     GError **error)
{
	GPtrArray *uids;
	CamelFolder *folder;
	EAccount *account;
	EIterator *iter;
	guint i;
	GHashTable *expunging_uids = NULL;

	uids = camel_folder_get_uids (expunging);
	if (!uids)
		return;

	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *info;

		info = camel_folder_get_message_info (
			expunging, uids->pdata[i]);

		if (!info)
			continue;

		if ((camel_message_info_flags (info) & CAMEL_MESSAGE_DELETED) != 0) {
			CamelMimeMessage *msg;
			GError *local_error = NULL;

			/* because the UID in the local store doesn't
			 * match with the UID in the pop3 store */
			msg = camel_folder_get_message_sync (
				expunging, uids->pdata[i],
				cancellable, &local_error);
			if (msg) {
				const gchar *pop3_uid;

				pop3_uid = camel_medium_get_header (
					CAMEL_MEDIUM (msg),
					"X-Evolution-POP3-UID");
				if (pop3_uid) {
					gchar *duped;

					duped = g_strstrip (g_strdup (pop3_uid));

					if (!expunging_uids)
						expunging_uids = g_hash_table_new_full (
							g_str_hash, g_str_equal,

							g_free, g_free);

					g_hash_table_insert (
						expunging_uids, duped,
						g_strdup (camel_mime_message_get_source (msg)));
				}

				g_object_unref (msg);
			}

			if (local_error)
				g_clear_error (&local_error);
		}

		camel_folder_free_message_info (expunging, info);
	}

	camel_folder_free_uids (expunging, uids);
	uids = NULL;

	if (!expunging_uids)
		return;

	for (iter = e_list_get_iterator ((EList *) e_get_account_list ());
	     e_iterator_is_valid (iter) && (!error || !*error);
	     e_iterator_next (iter)) {
		account = (EAccount *) e_iterator_get (iter);

		if (account->enabled &&
		    account->source && account->source->url &&
		    g_str_has_prefix (account->source->url, "pop://")) {
			gboolean any_found = FALSE;

			folder = e_mail_session_get_inbox_sync (
				session, account->uid, cancellable, error);
			if (!folder || (error && *error))
				continue;

			uids = camel_folder_get_uids (folder);
			if (uids) {
				for (i = 0; i < uids->len; i++) {
					/* ensure the ID is from this account,
					 * as it's generated by evolution */
					const gchar *source_uid;

					source_uid = g_hash_table_lookup (
						expunging_uids, uids->pdata[i]);
					if (folder_is_from_source_uid (folder, source_uid)) {
						any_found = TRUE;
						camel_folder_delete_message (folder, uids->pdata[i]);
					}
				}
				camel_folder_free_uids (folder, uids);
			}

			if (any_found)
				camel_folder_synchronize_sync (folder, TRUE, cancellable, error);

			g_object_unref (folder);
		}
	}

	if (iter)
		g_object_unref (iter);

	g_hash_table_destroy (expunging_uids);
}

static gchar *
expunge_folder_desc (struct _sync_folder_msg *m)
{
	return g_strdup_printf (
		_("Expunging folder '%s'"),
		camel_folder_get_full_name (m->folder));
}

static void
expunge_folder_exec (struct _sync_folder_msg *m,
                     GCancellable *cancellable,
                     GError **error)
{
	gboolean is_local_inbox_or_trash =
		m->folder == e_mail_local_get_folder (E_MAIL_LOCAL_FOLDER_INBOX);

	if (!is_local_inbox_or_trash && e_mail_local_get_store () ==
			camel_folder_get_parent_store (m->folder)) {
		CamelFolder *trash;

		trash = e_mail_session_get_trash_sync (
			m->session, "local", cancellable, error);

		is_local_inbox_or_trash = m->folder == trash;

		g_object_unref (trash);
	}

	/* do this before expunge, to know which messages will be expunged */
	if (is_local_inbox_or_trash && (!error || !*error))
		expunge_pop3_stores (m->folder, m->session, cancellable, error);

	if (!error || !*error)
		camel_folder_expunge_sync (m->folder, cancellable, error);
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
mail_expunge_folder (EMailSession *session,
                     CamelFolder *folder,
                     void (*done) (CamelFolder *folder, gpointer data),
                     gpointer data)
{
	struct _sync_folder_msg *m;

	m = mail_msg_new (&expunge_folder_info);
	m->session = g_object_ref (session);
	m->folder = folder;
	g_object_ref (folder);
	m->data = data;
	m->done = done;

	mail_msg_slow_ordered_push (m);
}

/* ******************************************************************************** */

struct _empty_trash_msg {
	MailMsg base;

	EMailSession *session;
	EAccount *account;
	void (*done) (EAccount *account, gpointer data);
	gpointer data;
};

static gchar *
empty_trash_desc (struct _empty_trash_msg *m)
{
	return g_strdup_printf (_("Emptying trash in '%s'"),
				m->account ? m->account->name : _("Local Folders"));
}

static void
empty_trash_exec (struct _empty_trash_msg *m,
                  GCancellable *cancellable,
                  GError **error)
{
	CamelFolder *trash;
	const gchar *uid;

	uid = (m->account != NULL) ? m->account->uid : "local";

	trash = e_mail_session_get_trash_sync (
		m->session, uid, cancellable, error);

	if (trash) {
		/* do this before expunge, to know which messages will be expunged */
		if (!m->account && (!error || !*error))
			expunge_pop3_stores (trash, m->session, cancellable, error);

		if (!error || !*error)
			camel_folder_expunge_sync (trash, cancellable, error);
		g_object_unref (trash);
	}
}

static void
empty_trash_done (struct _empty_trash_msg *m)
{
	if (m->done)
		m->done (m->account, m->data);
}

static void
empty_trash_free (struct _empty_trash_msg *m)
{
	if (m->session)
		g_object_unref (m->session);
	if (m->account)
		g_object_unref (m->account);
}

static MailMsgInfo empty_trash_info = {
	sizeof (struct _empty_trash_msg),
	(MailMsgDescFunc) empty_trash_desc,
	(MailMsgExecFunc) empty_trash_exec,
	(MailMsgDoneFunc) empty_trash_done,
	(MailMsgFreeFunc) empty_trash_free
};

void
mail_empty_trash (EMailSession *session,
                  EAccount *account,
                  void (*done) (EAccount *account, gpointer data),
                  gpointer data)
{
	struct _empty_trash_msg *m;

	m = mail_msg_new (&empty_trash_info);
	m->session = g_object_ref (session);
	m->account = account;
	if (account)
		g_object_ref (account);
	m->data = data;
	m->done = done;

	mail_msg_slow_ordered_push (m);
}

/* ** Execute Shell Command ************************************************ */

void
mail_execute_shell_command (CamelFilterDriver *driver,
                            gint argc,
                            gchar **argv,
                            gpointer data)
{
	if (argc <= 0)
		return;

	g_spawn_async (NULL, argv, NULL, 0, NULL, data, NULL, NULL);
}

/* ------------------------------------------------------------------------- */

struct _disconnect_msg {
	MailMsg base;

	CamelStore *store;
};

static gchar *
disconnect_service_desc (struct _disconnect_msg *m)
{
	gchar *name, *res;

	name = camel_service_get_name (CAMEL_SERVICE (m->store), TRUE);
	res = g_strdup_printf (_("Disconnecting %s"), name ? name : "");
	g_free (name);

	return res;
}

static void
disconnect_service_exec (struct _disconnect_msg *m,
                    GCancellable *cancellable,
                    GError **error)
{
	em_utils_disconnect_service_sync (
		CAMEL_SERVICE (m->store), TRUE, cancellable, error);
}

static void
disconnect_service_free (struct _disconnect_msg *m)
{
	g_object_unref (m->store);
}

static MailMsgInfo disconnect_service_info = {
	sizeof (struct _disconnect_msg),
	(MailMsgDescFunc) disconnect_service_desc,
	(MailMsgExecFunc) disconnect_service_exec,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) disconnect_service_free
};

gint
mail_disconnect_store (CamelStore *store)
{
	struct _disconnect_msg *m;
	gint id;

	g_return_val_if_fail (store != NULL, -1);

	m = mail_msg_new (&disconnect_service_info);
	m->store = g_object_ref (store);

	id = m->base.seq;
	mail_msg_unordered_push (m);

	return id;
}

