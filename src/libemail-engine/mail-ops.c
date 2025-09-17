/*
 * mail-ops.c: callbacks for the mail toolbar/menus
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

#include "evolution-config.h"

#include <errno.h>

#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <libedataserver/libedataserver.h>

#include <libemail-engine/mail-mt.h>

#include "e-mail-utils.h"
#include "mail-ops.h"
#include "mail-tools.h"

#include "e-mail-folder-utils.h"
#include "e-mail-session.h"
#include "e-mail-session-utils.h"

#define w(x)
#define d(x)

#define USER_AGENT ("Evolution " VERSION VERSION_SUBSTRING " " VERSION_COMMENT)

/* used for both just filtering a folder + uid's, and for filtering a whole folder */
/* used both for fetching mail, and for filtering mail */
struct _filter_mail_msg {
	MailMsg base;

	EMailSession *session;
	CamelFolder *source_folder;	/* where they come from */
	GPtrArray *source_uids;		/* uids to copy, or NULL == copy all */
	CamelUIDCache *cache;		/* UID cache if we are to cache
					 * the uids, NULL otherwise */
	CamelFilterDriver *driver;
	gint delete;			/* delete messages after filtering? */
	CamelFolder *destination;	/* default destination for any
					 * messages, NULL for none */
};

/* since fetching also filters, we subclass the data here */
struct _fetch_mail_msg {
	struct _filter_mail_msg fmsg;

	CamelStore *store;
	GCancellable *cancellable;	/* we have our own cancellation
					 * struct, the other should be empty */
	gint keep;			/* keep on server? */

	MailProviderFetchLockFunc provider_lock;
	MailProviderFetchUnlockFunc provider_unlock;
	MailProviderFetchInboxFunc provider_fetch_inbox;

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
static gboolean
em_filter_folder_element_exec (struct _filter_mail_msg *m,
                               GCancellable *cancellable,
                               GError **error)
{
	CamelFolder *folder;
	GPtrArray *uids, *folder_uids = NULL;
	gboolean success = TRUE;
	GError *local_error = NULL;

	folder = m->source_folder;

	if (folder == NULL || camel_folder_get_message_count (folder) == 0)
		return success;

	if (m->destination) {
		camel_folder_freeze (m->destination);
		camel_filter_driver_set_default_folder (m->driver, m->destination);
	}

	camel_folder_freeze (folder);

	if (m->source_uids)
		uids = m->source_uids;
	else
		folder_uids = uids = camel_folder_dup_uids (folder);

	success = camel_filter_driver_filter_folder (
		m->driver, folder, m->cache, uids, m->delete,
		cancellable, &local_error) == 0;
	camel_filter_driver_flush (m->driver, &local_error);

	if (folder_uids)
		g_ptr_array_unref (folder_uids);

	/* sync our source folder */
	if (!m->cache && !local_error)
		camel_folder_synchronize_sync (
			folder, FALSE, cancellable, &local_error);
	camel_folder_thaw (folder);

	if (m->destination)
		camel_folder_thaw (m->destination);

	/* this may thaw/unref source folders, do it here so we don't do
	 * it in the main thread see also fetch_mail_fetch () below */
	g_object_unref (m->driver);
	m->driver = NULL;

	if (g_error_matches (local_error, CAMEL_SERVICE_ERROR, CAMEL_SERVICE_ERROR_URL_INVALID) ||
	    g_error_matches (local_error, CAMEL_FOLDER_ERROR, CAMEL_FOLDER_ERROR_INVALID)) {
		g_set_error (
			error, local_error->domain, local_error->code,
			_("Failed to filter selected messages. One reason can be that folder "
			"location set in one or more filters is invalid. Please check your "
			"filters in Edit→Message Filters.\n"
			"Original error was: %s"), local_error->message);
		g_clear_error (&local_error);
	} else if (local_error)
		g_propagate_error (error, local_error);

	return success;
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
		g_ptr_array_unref (m->source_uids);

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
	m->source_uids = g_ptr_array_ref (uids);
	m->cache = NULL;
	m->delete = FALSE;

	m->driver = camel_session_get_filter_driver (CAMEL_SESSION (session), type, source_folder, NULL);

	if (!notify) {
		/* FIXME: have a #define NOTIFY_FILTER_NAME macro? */
		/* the filter name has to stay in sync
		 * with mail-session::get_filter_driver */
		camel_filter_driver_remove_rule_by_name (
			m->driver, "new-mail-notification");
	}

	mail_msg_unordered_push (m);
}

/* ********************************************************************** */

static gchar *
fetch_mail_desc (struct _fetch_mail_msg *m)
{
	return g_strdup_printf (
		_("Fetching mail from “%s”"),
		camel_service_get_display_name (CAMEL_SERVICE (m->store)));
}

static void
fetch_mail_exec (struct _fetch_mail_msg *m,
                 GCancellable *cancellable,
                 GError **error)
{
	struct _filter_mail_msg *fm = (struct _filter_mail_msg *) m;
	GObjectClass *class;
	CamelFolder *folder = NULL;
	CamelProvider *provider;
	CamelService *service;
	CamelSession *session;
	CamelSettings *settings;
	CamelStore *parent_store;
	CamelUIDCache *cache = NULL;
	gboolean keep = TRUE;
	gboolean delete_fetched;
	gboolean is_local_delivery = FALSE;
	const gchar *uid = NULL;
	const gchar *data_dir;
	gchar *cachename;
	gint i;

	service = CAMEL_SERVICE (m->store);
	session = camel_service_ref_session (service);
	provider = camel_service_get_provider (service);

	if (provider && (provider->flags & CAMEL_PROVIDER_IS_REMOTE) != 0 &&
	    !camel_session_get_online (session))
		goto exit;

	fm->destination = e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_LOCAL_INBOX);
	if (fm->destination == NULL)
		goto exit;
	g_object_ref (fm->destination);

	uid = camel_service_get_uid (service);
	settings = camel_service_ref_settings (service);

	/* XXX This is a POP3-specific setting. */
	class = G_OBJECT_GET_CLASS (settings);
	if (g_object_class_find_property (class, "keep-on-server") != NULL)
		g_object_get (settings, "keep-on-server", &keep, NULL);

	g_object_unref (settings);

	/* Just for readability. */
	delete_fetched = !keep;

	if (em_utils_is_local_delivery_mbox_file (service)) {
		gchar *path;

		path = mail_tool_do_movemail (m->store, error);

		if (path && (!error || !*error)) {
			camel_folder_freeze (fm->destination);
			camel_filter_driver_set_default_folder (
				fm->driver, fm->destination);
			camel_filter_driver_filter_mbox (
				fm->driver, path, camel_service_get_uid (service),
				cancellable, error);
			camel_folder_thaw (fm->destination);

			if (!error || !*error)
				g_unlink (path);
		}

		g_free (path);
	} else {
		uid = camel_service_get_uid (service);
		if (m->provider_lock)
			m->provider_lock (uid);

		folder = fm->source_folder =
			e_mail_session_get_inbox_sync (
				fm->session, uid, cancellable, error);
	}

	if (folder == NULL)
		goto exit;

	parent_store = camel_folder_get_parent_store (folder);

	service = CAMEL_SERVICE (parent_store);
	data_dir = camel_service_get_user_data_dir (service);

	cachename = g_build_filename (data_dir, "uid-cache", NULL);
	cache = camel_uid_cache_new (cachename);
	g_free (cachename);

	if (cache) {
		GPtrArray *folder_uids, *cache_uids, *uids;
		GError *local_error = NULL;

		if (m->provider_fetch_inbox) {
			g_object_unref (fm->destination);
			fm->destination = m->provider_fetch_inbox (uid, cancellable, &local_error);
			if (fm->destination == NULL)
				goto exit;
			g_object_ref (fm->destination);
		}

		if (!local_error && !g_cancellable_is_cancelled (cancellable)) {
			folder_uids = camel_folder_dup_uids (folder);
			cache_uids = camel_uid_cache_dup_new_uids (cache, folder_uids);

			if (cache_uids) {
				gboolean success;

				/* need to copy this, sigh */
				fm->source_uids = uids = g_ptr_array_new_with_free_func ((GDestroyNotify) camel_pstring_free);
				g_ptr_array_set_size (uids, cache_uids->len);

				/* Reverse it so that we fetch the latest as first, while fetching POP  */
				for (i = 0; i < cache_uids->len; i++) {
					uids->pdata[cache_uids->len - i - 1] = (gpointer) camel_pstring_strdup (cache_uids->pdata[i]);
				}

				fm->cache = cache;

				success = em_filter_folder_element_exec (fm, cancellable, &local_error);

				/* need to uncancel so writes/etc. don't fail */
				if (g_cancellable_is_cancelled (m->cancellable))
					g_cancellable_reset (m->cancellable);

				if (!success) {
					GPtrArray *uncached_uids;
					GHashTable *uncached_hash;

					uncached_uids = camel_folder_dup_uncached_uids (folder, cache_uids, NULL);
					uncached_hash = g_hash_table_new (g_str_hash, g_str_equal);

					for (i = 0; uncached_uids && i < uncached_uids->len; i++) {
						g_hash_table_insert (uncached_hash, uncached_uids->pdata[i], uncached_uids->pdata[i]);
					}

					/* re-enter known UIDs, thus they are not
					 * re-fetched next time */
					for (i = 0; i < cache_uids->len; i++) {
						/* skip uncached UIDs */
						if (!g_hash_table_lookup (uncached_hash, cache_uids->pdata[i]))
							camel_uid_cache_save_uid (cache, cache_uids->pdata[i]);
					}

					g_hash_table_destroy (uncached_hash);
					g_ptr_array_unref (uncached_uids);
				}

				/* save the cache of uids that we've just downloaded */
				camel_uid_cache_save (cache);

				g_ptr_array_unref (cache_uids);
			}

			if (delete_fetched && !local_error) {
				/* not keep on server - just delete all
				 * the actual messages on the server */
				for (i = 0; i < folder_uids->len; i++) {
					camel_folder_delete_message (
						folder, folder_uids->pdata[i]);
				}
			}

			if ((delete_fetched || cache_uids) && !local_error) {
				/* expunge messages (downloaded so far) */
				/* FIXME Not passing a GCancellable or GError here. */
				camel_folder_synchronize_sync (
					folder, delete_fetched, NULL, NULL);
			}

			g_ptr_array_unref (folder_uids);
		}

		camel_uid_cache_destroy (cache);

		if (local_error)
			g_propagate_error (error, local_error);
	} else {
		em_filter_folder_element_exec (fm, cancellable, error);
	}

	/* we unref the source folder here since we
	 * may now block in finalize (we try to
	 * disconnect cleanly) */
	g_object_unref (fm->source_folder);
	fm->source_folder = NULL;

exit:
	if (!is_local_delivery && m->provider_unlock)
		m->provider_unlock (uid);

	/* we unref this here as it may have more work to do (syncing
	 * folders and whatnot) before we are really done */
	/* should this be cancellable too? (i.e. above unregister above) */
	g_clear_object (&fm->driver);

	/* also disconnect if not a local delivery mbox;
	 * there is no need to keep the connection alive forever */
	if (!is_local_delivery) {
		gboolean was_cancelled;

		was_cancelled = g_cancellable_is_cancelled (cancellable);

		/* pity, but otherwise it doesn't disconnect */
		if (was_cancelled)
			g_cancellable_reset (cancellable);

		camel_service_disconnect_sync (
			service, !was_cancelled, cancellable, NULL);
	}

	g_object_unref (session);

	e_util_call_malloc_trim_limited ();
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
                 const gchar *type,
                 MailProviderFetchLockFunc lock_func,
                 MailProviderFetchUnlockFunc unlock_func,
                 MailProviderFetchInboxFunc fetch_inbox_func,
                 GCancellable *cancellable,
                 CamelFilterGetFolderFunc get_folder,
                 gpointer get_data,
                 CamelFilterStatusFunc status,
                 gpointer status_data,
                 void (*done)(gpointer data),
                 gpointer data)
{
	struct _fetch_mail_msg *m;
	struct _filter_mail_msg *fm;
	CamelSession *session;

	g_return_if_fail (CAMEL_IS_STORE (store));

	session = camel_service_ref_session (CAMEL_SERVICE (store));

	m = mail_msg_new (&fetch_mail_info);
	fm = (struct _filter_mail_msg *) m;
	fm->session = E_MAIL_SESSION (g_object_ref (session));
	m->store = g_object_ref (store);
	fm->cache = NULL;
	if (cancellable)
		m->cancellable = g_object_ref (cancellable);
	m->done = done;
	m->data = data;

	m->provider_lock = lock_func;
	m->provider_unlock = unlock_func;
	m->provider_fetch_inbox = fetch_inbox_func;

	fm->driver = camel_session_get_filter_driver (session, type, NULL, NULL);
	camel_filter_driver_set_folder_func (fm->driver, get_folder, get_data);
	if (status)
		camel_filter_driver_set_status_func (fm->driver, status, status_data);

	mail_msg_unordered_push (m);

	g_object_unref (session);
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
	gboolean immediately;

	CamelFilterDriver *driver;

	/* we use camelfilterstatusfunc, even though its not the filter doing it */
	CamelFilterStatusFunc status;
	gpointer status_data;

	GPtrArray *failed_uids;

	gboolean (* done)(gpointer data, const GError *error, const GPtrArray *failed_uids);
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
                   CamelFilterDriver *driver,
                   GCancellable *cancellable,
                   GError **error)
{
	CamelService *service;
	const CamelInternetAddress *iaddr;
	CamelAddress *from, *recipients;
	CamelMessageInfo *info = NULL;
	CamelProvider *provider = NULL;
	const gchar *resent_from;
	CamelFolder *folder = NULL;
	GString *err = NULL;
	CamelNameValueArray *xev_headers = NULL;
	CamelMimeMessage *message;
	gint i;
	gsize msg_size;
	guint jj, len;
	GError *local_error = NULL;
	gboolean did_connect = FALSE;
	gboolean sent_message_saved = FALSE;
	gboolean request_dsn;

	message = camel_folder_get_message_sync (
		queue, uid, cancellable, error);
	if (!message)
		return;

	if (!camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Is-Redirect"))
		camel_medium_set_header (CAMEL_MEDIUM (message), "User-Agent", USER_AGENT);

	request_dsn = g_strcmp0 (camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Request-DSN"), "1") == 0;

	/* Do this before removing "X-Evolution" headers. */
	service = e_mail_session_ref_transport_for_message (
		m->session, message);
	if (service != NULL)
		provider = camel_service_get_provider (service);

	if (CAMEL_IS_TRANSPORT (service)) {
		const gchar *tuid;

		/* Let the dialog know the right account it is using. */
		tuid = camel_service_get_uid (service);
		report_status (m, CAMEL_FILTER_STATUS_ACTION, 0, tuid);
	}

	if (service && !e_mail_session_mark_service_used_sync (m->session, service, cancellable)) {
		g_warn_if_fail (g_cancellable_set_error_if_cancelled (cancellable, error));
		g_clear_object (&service);
		g_clear_object (&message);
		return;
	}

	err = g_string_new ("");
	xev_headers = mail_tool_remove_xevolution_headers (message);

	/* Check for email sending */
	from = (CamelAddress *) camel_internet_address_new ();
	resent_from = camel_medium_get_header (
		CAMEL_MEDIUM (message), "Resent-From");
	if (resent_from != NULL) {
		camel_address_decode (from, resent_from);
	} else {
		iaddr = camel_mime_message_get_from (message);
		camel_address_copy (from, CAMEL_ADDRESS (iaddr));
	}

	recipients = (CamelAddress *) camel_internet_address_new ();
	for (i = 0; i < 3; i++) {
		const gchar *type;

		if (resent_from != NULL)
			type = resent_recipients[i];
		else
			type = normal_recipients[i];
		iaddr = camel_mime_message_get_recipients (message, type);
		camel_address_cat (recipients, CAMEL_ADDRESS (iaddr));
	}

	if (camel_address_length (recipients) > 0) {
		if (provider && (provider->flags & CAMEL_PROVIDER_IS_REMOTE) != 0 &&
		    !camel_session_get_online (CAMEL_SESSION (m->session))) {
			/* silently ignore */
			goto exit;
		}
		if (camel_service_get_connection_status (service) != CAMEL_SERVICE_CONNECTED) {
			EMailSession *session;
			ESourceRegistry *registry;
			ESource *source;

			/* Make sure user will be asked for a password, in case he/she cancelled it */
			session = E_MAIL_SESSION (camel_service_ref_session (service));
			registry = e_mail_session_get_registry (session);
			source = e_source_registry_ref_source (registry, camel_service_get_uid (service));
			g_object_unref (session);

			if (source) {
				e_mail_session_emit_allow_auth_prompt (m->session, source);
				g_object_unref (source);
			}

			if (!camel_service_connect_sync (service, cancellable, error))
				goto exit;

			did_connect = TRUE;
		}

		/* expand, or remove empty, group addresses */
		em_utils_expand_groups (CAMEL_INTERNET_ADDRESS (recipients));

		camel_transport_set_request_dsn (CAMEL_TRANSPORT (service), request_dsn);

		if (!camel_transport_send_to_sync (
			CAMEL_TRANSPORT (service), message,
			from, recipients, &sent_message_saved, cancellable, error))
			goto exit;
	}

	/* Now check for posting, failures are ignored */
	info = camel_message_info_new_from_headers (NULL, camel_medium_get_headers (CAMEL_MEDIUM (message)));
	msg_size = camel_data_wrapper_calculate_size_sync (CAMEL_DATA_WRAPPER (message), cancellable, NULL);
	if (msg_size != ((gsize) -1))
		camel_message_info_set_size (info, msg_size);
	camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN |
		(camel_mime_message_has_attachment (message) ? CAMEL_MESSAGE_ATTACHMENTS : 0), ~0);

	len = camel_name_value_array_get_length (xev_headers);
	for (jj = 0; jj < len && !local_error; jj++) {
		const gchar *header_name = NULL, *header_value = NULL;
		gchar *uri;

		if (!camel_name_value_array_get (xev_headers, jj, &header_name, &header_value) ||
		    !header_name ||
		    g_ascii_strcasecmp (header_name, "X-Evolution-PostTo") != 0)
			continue;

		uri = g_strstrip (g_strdup (header_value));
		folder = e_mail_session_uri_to_folder_sync (
			m->session, uri, 0, cancellable, &local_error);
		if (folder != NULL) {
			camel_operation_push_message (cancellable, _("Posting message to “%s”"), camel_folder_get_full_display_name (folder));

			camel_folder_append_message_sync (
				folder, message, info, NULL, cancellable, &local_error);

			camel_operation_pop_message (cancellable);

			g_object_unref (folder);
			folder = NULL;
		}
		g_free (uri);
	}

	/* post process */
	mail_tool_restore_xevolution_headers (message, xev_headers);

	if (local_error == NULL && driver) {
		const gchar *transport_uid = service ? camel_service_get_uid (service) : NULL;

		camel_filter_driver_filter_message (
			driver, message, info, NULL, NULL,
			transport_uid, transport_uid ? transport_uid : "",
			cancellable, &local_error);

		if (local_error != NULL) {
			if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
				goto exit;

			/* sending mail, filtering failed */
			if (g_error_matches (local_error, CAMEL_SERVICE_ERROR, CAMEL_SERVICE_ERROR_URL_INVALID) ||
			    g_error_matches (local_error, CAMEL_FOLDER_ERROR, CAMEL_FOLDER_ERROR_INVALID)) {
				g_string_append_printf (
					err,
					_("Failed to apply outgoing filters. One reason can be that folder "
					"location set in one or more filters is invalid. Please check your "
					"filters in Edit→Message Filters.\n"
					"Original error was: %s"), local_error->message);
			} else {
				g_string_append_printf (
					err, _("Failed to apply outgoing filters: %s"),
					local_error->message);
			}

			g_clear_error (&local_error);
		}
	}

	if (local_error == NULL && !sent_message_saved && (provider == NULL
	    || !(provider->flags & CAMEL_PROVIDER_DISABLE_SENT_FOLDER))) {
		CamelFolder *local_sent_folder;
		gboolean use_sent_folder = TRUE;

		folder = e_mail_session_get_fcc_for_message_sync (
			m->session, message, &use_sent_folder, cancellable, &local_error);

		if (!use_sent_folder)
			goto cleanup;

		local_sent_folder = e_mail_session_get_local_folder (
			m->session, E_MAIL_LOCAL_FOLDER_SENT);

		/* Sanity check. */
		g_return_if_fail (
			((folder == NULL) && (local_error != NULL)) ||
			((folder != NULL) && (local_error == NULL)));

		if (local_error == NULL) {
			camel_operation_push_message (cancellable, _("Storing sent message to “%s”"), camel_folder_get_full_display_name (folder));

			camel_folder_append_message_sync (
				folder, message, info, NULL,
				cancellable, &local_error);

			camel_operation_pop_message (cancellable);
		}

		if (g_error_matches (
			local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			goto exit;

		if (local_error != NULL && folder != local_sent_folder) {

			if (folder != NULL) {
				const gchar *description;

				description =
					camel_folder_get_description (folder);
				if (err->len > 0)
					g_string_append (err, "\n\n");
				g_string_append_printf (
					err,
					_("Failed to append to %s: %s\n"
					"Appending to local “Sent” folder instead."),
					description,
					local_error->message);

				g_object_unref (folder);
			}

			g_clear_error (&local_error);
			folder = g_object_ref (local_sent_folder);

			camel_operation_push_message (cancellable, _("Storing sent message to “%s”"), camel_folder_get_full_display_name (folder));

			camel_folder_append_message_sync (
				folder, message, info, NULL,
				cancellable, &local_error);

			camel_operation_pop_message (cancellable);

			if (g_error_matches (
				local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
				goto exit;

			if (local_error != NULL) {
				if (err->len > 0)
					g_string_append (err, "\n\n");
				g_string_append_printf (
					err,
					_("Failed to append to "
					"local “Sent” folder: %s"),
					local_error->message);
				g_clear_error (&local_error);
			}
		}
	}

 cleanup:
	if (local_error == NULL) {
		/* Mark the draft message for deletion, if present. */
		e_mail_session_handle_draft_headers_sync (
			m->session, message, cancellable, &local_error);
		if (local_error != NULL) {
			g_warning (
				"%s: Failed to handle draft headers: %s",
				G_STRFUNC, local_error->message);
			g_clear_error (&local_error);
		}

		/* Set flags on the original source message, if present.
		 * Source message refers to the message being forwarded
		 * or replied to. */
		e_mail_session_handle_source_headers_sync (
			m->session, message, cancellable, &local_error);
		if (local_error &&
		    !g_error_matches (local_error, CAMEL_FOLDER_ERROR, CAMEL_FOLDER_ERROR_INVALID_UID) &&
		    !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_warning (
				"%s: Failed to handle source headers: %s",
				G_STRFUNC, local_error->message);
		}
		g_clear_error (&local_error);
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

	if (local_error == NULL && err->len > 0) {
		/* set the culmulative exception report */
		g_set_error (
			&local_error, CAMEL_ERROR,
			CAMEL_ERROR_GENERIC, "%s", err->str);
	}

exit:
	if (did_connect) {
		/* Disconnect regardless of error or cancellation,
		 * but be mindful of these conditions when calling
		 * camel_service_disconnect_sync(). */
		if (g_cancellable_is_cancelled (cancellable)) {
			camel_service_disconnect_sync (service, FALSE, NULL, NULL);
		} else if (local_error != NULL) {
			camel_service_disconnect_sync (service, FALSE, cancellable, NULL);
		} else {
			camel_service_disconnect_sync (service, TRUE, cancellable, &local_error);
		}
	}

	if (service)
		e_mail_session_unmark_service_used (m->session, service);

	if (local_error != NULL)
		g_propagate_error (error, local_error);

	/* FIXME Not passing a GCancellable or GError here. */
	if (folder != NULL) {
		camel_folder_synchronize_sync (folder, FALSE, NULL, NULL);
		g_object_unref (folder);
	}

	g_clear_object (&info);
	g_clear_object (&service);

	g_object_unref (recipients);
	g_object_unref (from);
	camel_name_value_array_free (xev_headers);
	g_string_free (err, TRUE);
	g_object_unref (message);
}

/* ** SEND MAIL QUEUE ***************************************************** */

static void
maybe_schedule_next_flush (EMailSession *session,
			   time_t nearest_next_flush)
{
	gint delay_seconds, delay_minutes;

	if (!session || nearest_next_flush <= 0)
		return;

	delay_seconds = nearest_next_flush - time (NULL);
	if (delay_seconds <= 0)
		delay_seconds = 1;

	delay_minutes = delay_seconds / 60 + ((delay_seconds % 60) > 0 ? 1 : 0);

	if (!delay_minutes)
		delay_minutes = 1;

	e_mail_session_schedule_outbox_flush (session, delay_minutes);
}

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
	gint i, j, delay_flush = 0;
	time_t delay_send = 0, nearest_next_flush = 0;
	GError *local_error = NULL;

	d (printf ("sending queue\n"));

	if (!m->immediately) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		if (g_settings_get_boolean (settings, "composer-use-outbox")) {
			delay_flush = g_settings_get_int (settings, "composer-delay-outbox-flush");

			if (delay_flush > 0)
				delay_send = time (NULL) - (60 * delay_flush);
		}
		g_object_unref (settings);
	}

	sent_folder =
		e_mail_session_get_local_folder (
		m->session, E_MAIL_LOCAL_FOLDER_SENT);

	if (!(uids = camel_folder_dup_uids (m->queue)))
		return;

	send_uids = g_ptr_array_sized_new (uids->len);
	for (i = 0, j = 0; i < uids->len; i++) {
		CamelMessageInfo *info;

		info = camel_folder_get_message_info (m->queue, uids->pdata[i]);
		if (info) {
			if (!(camel_message_info_get_flags (info) & CAMEL_MESSAGE_DELETED)) {
				gboolean is_editing = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (info), MAIL_USER_KEY_EDITING)) != 0;

				if (!delay_send || (!is_editing && camel_message_info_get_date_sent (info) <= delay_send)) {
					send_uids->pdata[j++] = uids->pdata[i];
				} else if (!is_editing && (!nearest_next_flush || nearest_next_flush > camel_message_info_get_date_sent (info))) {
					nearest_next_flush = camel_message_info_get_date_sent (info);
				}
			}

			g_clear_object (&info);
		}
	}

	if (nearest_next_flush > 0)
		nearest_next_flush += (delay_flush * 60);

	send_uids->len = j;
	if (send_uids->len == 0) {
		maybe_schedule_next_flush (m->session, nearest_next_flush);

		/* nothing to send */
		g_ptr_array_unref (uids);
		g_ptr_array_free (send_uids, TRUE);
		return;
	}

	camel_operation_push_message (cancellable, _("Sending message"));

	/* NB: This code somewhat abuses the 'exception' stuff.  Apart from
	 *     fatal problems, it is also used as a mechanism to accumulate
	 *     warning messages and present them back to the user. */

	for (i = 0, j = 0; i < send_uids->len; i++) {
		gint pc = (100 * i) / send_uids->len;

		report_status (
			m, CAMEL_FILTER_STATUS_START, pc,
			_("Sending message %d of %d"), i + 1,
			send_uids->len);

		camel_operation_progress (
			cancellable, (i + 1) * 100 / send_uids->len);

		mail_send_message (
			m, m->queue, send_uids->pdata[i],
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

				if (!m->failed_uids)
					m->failed_uids = g_ptr_array_new_with_free_func ((GDestroyNotify) camel_pstring_free);

				g_ptr_array_add (m->failed_uids, (gpointer) camel_pstring_strdup (send_uids->pdata[i]));

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
			ngettext (
				/* Translators: The string is distinguished by total
				 * count of messages to be sent.  Failed messages is
				 * always more than zero. */
				"Failed to send a message",
				"Failed to send %d of %d messages",
				send_uids->len),
			j, send_uids->len);
	else if (g_error_matches (
			m->base.error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		report_status (m, CAMEL_FILTER_STATUS_END, 100, _("Cancelled."));
	else
		report_status (m, CAMEL_FILTER_STATUS_END, 100, _("Complete."));

	g_clear_object (&m->driver);

	g_ptr_array_unref (uids);
	g_ptr_array_free (send_uids, TRUE);

	/* FIXME Not passing a GCancellable or GError here. */
	if (j <= 0 && m->base.error == NULL)
		camel_folder_synchronize_sync (m->queue, TRUE, NULL, NULL);

	/* FIXME Not passing a GCancellable or GError here. */
	if (sent_folder)
		camel_folder_synchronize_sync (sent_folder, FALSE, NULL, NULL);

	camel_operation_pop_message (cancellable);

	maybe_schedule_next_flush (m->session, nearest_next_flush);
}

static void
send_queue_done (struct _send_queue_msg *m)
{
	if (m->done) {
		if (g_error_matches (m->base.error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			m->done (m->data, NULL, NULL);
		} else if (m->done (m->data, m->base.error, m->failed_uids)) {
			g_clear_error (&m->base.error);
		}
	}
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
	if (m->failed_uids)
		g_ptr_array_unref (m->failed_uids);
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
		 gboolean immediately,
                 GCancellable *cancellable,
                 CamelFilterGetFolderFunc get_folder,
                 gpointer get_data,
                 CamelFilterStatusFunc status,
                 gpointer status_data,
                 gboolean (* done)(gpointer data, const GError *error, const GPtrArray *failed_uids),
                 gpointer data)
{
	struct _send_queue_msg *m;

	g_return_if_fail (E_IS_MAIL_SESSION (session));

	e_mail_session_cancel_scheduled_outbox_flush (session);

	m = mail_msg_new (&send_queue_info);
	m->session = g_object_ref (session);
	m->queue = g_object_ref (queue);
	m->transport = g_object_ref (transport);
	m->immediately = immediately;
	if (G_IS_CANCELLABLE (cancellable))
		m->base.cancellable = g_object_ref (cancellable);
	m->status = status;
	m->status_data = status_data;
	m->done = done;
	m->data = data;

	m->driver = camel_session_get_filter_driver (CAMEL_SESSION (session), type, queue, NULL);
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
			_("Moving messages to “%s”") :
			_("Copying messages to “%s”"),
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
	g_ptr_array_unref (m->uids);
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
                        void (*done) (gboolean ok,
                                      gpointer data),
                        gpointer data)
{
	struct _transfer_msg *m;

	g_return_if_fail (CAMEL_IS_FOLDER (source));
	g_return_if_fail (uids != NULL);
	g_return_if_fail (dest_uri != NULL);

	m = mail_msg_new (&transfer_messages_info);
	m->session = g_object_ref (session);
	m->source = g_object_ref (source);
	m->uids = g_ptr_array_ref (uids);
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

	CamelFolder *folder;
	gboolean test_for_expunge;
	void (*done) (CamelFolder *folder, gpointer data);
	gpointer data;
};

static gchar *
sync_folder_desc (struct _sync_folder_msg *m)
{
	return g_strdup_printf (
		_("Storing folder “%s”"),
		camel_folder_get_full_display_name (m->folder));
}

static void
sync_folder_exec (struct _sync_folder_msg *m,
                  GCancellable *cancellable,
                  GError **error)
{
	gboolean expunge = FALSE;

	if (m->test_for_expunge) {
		GSettings *settings;
		gboolean delete_junk;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");

		expunge = g_settings_get_boolean (settings, "trash-empty-on-exit") &&
			  g_settings_get_int (settings, "trash-empty-on-exit-days") == -1;
		delete_junk = g_settings_get_boolean (settings, "junk-empty-on-exit") &&
			      g_settings_get_int (settings, "junk-empty-on-exit-days") == -1;

		g_object_unref (settings);

		/* delete junk first, if requested */
		if (delete_junk) {
			CamelStore *store;
			CamelFolder *folder;

			store = camel_folder_get_parent_store (m->folder);
			folder = camel_store_get_junk_folder_sync (store, cancellable, error);
			if (folder != NULL) {
				GPtrArray *uids;
				guint32 flags;
				guint32 mask;
				guint ii;

				uids = camel_folder_dup_uids (folder);
				flags = mask = CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN;

				camel_folder_freeze (folder);

				for (ii = 0; ii < uids->len && !g_cancellable_is_cancelled (cancellable); ii++) {
					const gchar *uid = uids->pdata[ii];
					camel_folder_set_message_flags (folder, uid, flags, mask);
				}

				camel_folder_thaw (folder);
				g_ptr_array_unref (uids);

				g_object_unref (folder);

				if (g_cancellable_set_error_if_cancelled (cancellable, error))
					return;
			}

			/* error should be set already, from the get_junk_folder_sync() call */
			if (g_cancellable_is_cancelled (cancellable))
				return;
		}
	}

	camel_folder_synchronize_sync (m->folder, expunge, cancellable, error);
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
	if (m->folder)
		g_object_unref (m->folder);
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
                  gboolean test_for_expunge,
                  void (*done) (CamelFolder *folder,
                                gpointer data),
                  gpointer data)
{
	struct _sync_folder_msg *m;

	m = mail_msg_new (&sync_folder_info);
	m->folder = g_object_ref (folder);
	m->test_for_expunge = test_for_expunge;
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
	CamelService *service;
	gchar *display_name;
	gchar *description;

	service = CAMEL_SERVICE (m->store);
	display_name = camel_service_dup_display_name (service);

	description = g_strdup_printf (
		m->expunge ?
		_("Expunging and storing account “%s”") :
		_("Storing account “%s”"),
		display_name);

	g_free (display_name);

	return description;
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
                 void (*done) (CamelStore *store,
                               gpointer data),
                 gpointer data)
{
	struct _sync_store_msg *m;

	m = mail_msg_new (&sync_store_info);
	m->store = g_object_ref (store);
	m->expunge = expunge;
	m->data = data;
	m->done = done;

	mail_msg_slow_ordered_push (m);
}

/* ******************************************************************************** */

struct _empty_trash_msg {
	MailMsg base;

	CamelStore *store;
};

static gchar *
empty_trash_desc (struct _empty_trash_msg *m)
{
	CamelService *service;
	const gchar *display_name;

	service = CAMEL_SERVICE (m->store);
	display_name = camel_service_get_display_name (service);

	return g_strdup_printf (
		_("Emptying trash in “%s”"), display_name);
}

static void
empty_trash_exec (struct _empty_trash_msg *m,
                  GCancellable *cancellable,
                  GError **error)
{
	CamelService *service;
	CamelFolder *trash;

	service = CAMEL_SERVICE (m->store);

	if (!camel_service_connect_sync (service, cancellable, error))
		return;

	trash = camel_store_get_trash_folder_sync (
		m->store, cancellable, error);

	if (trash != NULL) {
		e_mail_folder_expunge_sync (trash, cancellable, error);
		g_object_unref (trash);
	}
}

static void
empty_trash_done (struct _empty_trash_msg *m)
{
}

static void
empty_trash_free (struct _empty_trash_msg *m)
{
	if (m->store)
		g_object_unref (m->store);
}

static MailMsgInfo empty_trash_info = {
	sizeof (struct _empty_trash_msg),
	(MailMsgDescFunc) empty_trash_desc,
	(MailMsgExecFunc) empty_trash_exec,
	(MailMsgDoneFunc) empty_trash_done,
	(MailMsgFreeFunc) empty_trash_free
};

void
mail_empty_trash (CamelStore *store)
{
	struct _empty_trash_msg *m;

	g_return_if_fail (CAMEL_IS_STORE (store));

	m = mail_msg_new (&empty_trash_info);
	m->store = g_object_ref (store);

	mail_msg_slow_ordered_push (m);
}

/* ** Execute Shell Command ************************************************ */

void
mail_execute_shell_command (CamelFilterDriver *driver,
                            gint argc,
                            gchar **argv,
                            gpointer data)
{
	GError *error = NULL;

	if (argc <= 0) {
		camel_filter_driver_log_info (driver, "Cannot execute shell command, no arguments passed in");
		return;
	}

	if (!g_spawn_async (NULL, argv, NULL, 0, NULL, data, NULL, &error))
		camel_filter_driver_log_info (driver, "Failed to execute shell command: %s", error ? error->message : "Unknown error");

	g_clear_error (&error);
}

/* ** Process Folder Changes *********************************************** */

struct _process_folder_changes_msg {
	MailMsg base;

	CamelFolder *folder;
	CamelFolderChangeInfo *changes;
	void (*process) (CamelFolder *folder,
			 CamelFolderChangeInfo *changes,
			 GCancellable *cancellable,
			 GError **error,
			 gpointer user_data);
	void (* done) (gpointer user_data);
	gpointer user_data;
};

static gchar *
process_folder_changes_desc (struct _process_folder_changes_msg *m)
{
	return g_strdup_printf (
		_("Processing folder changes in “%s”"), camel_folder_get_full_display_name (m->folder));
}

static void
process_folder_changes_exec (struct _process_folder_changes_msg *m,
			     GCancellable *cancellable,
			     GError **error)
{
	m->process (m->folder, m->changes, cancellable, error, m->user_data);
}

static void
process_folder_changes_done (struct _process_folder_changes_msg *m)
{
	if (m->done)
		m->done (m->user_data);
}

static void
process_folder_changes_free (struct _process_folder_changes_msg *m)
{
	g_clear_object (&m->folder);
	camel_folder_change_info_free (m->changes);
}

static MailMsgInfo process_folder_changes_info = {
	sizeof (struct _process_folder_changes_msg),
	(MailMsgDescFunc) process_folder_changes_desc,
	(MailMsgExecFunc) process_folder_changes_exec,
	(MailMsgDoneFunc) process_folder_changes_done,
	(MailMsgFreeFunc) process_folder_changes_free
};

void
mail_process_folder_changes (CamelFolder *folder,
			     CamelFolderChangeInfo *changes,
			     void (*process) (CamelFolder *folder,
					      CamelFolderChangeInfo *changes,
					      GCancellable *cancellable,
					      GError **error,
					      gpointer user_data),
			     void (* done) (gpointer user_data),
			     gpointer user_data)
{
	struct _process_folder_changes_msg *m;
	CamelFolderChangeInfo *changes_copy;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (changes != NULL);
	g_return_if_fail (process != NULL);

	changes_copy = camel_folder_change_info_new ();
	camel_folder_change_info_cat (changes_copy, changes);

	m = mail_msg_new (&process_folder_changes_info);
	m->folder = g_object_ref (folder);
	m->changes = changes_copy;
	m->process = process;
	m->done = done;
	m->user_data = user_data;

	mail_msg_unordered_push (m);
}
