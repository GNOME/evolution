/*
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
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#include <glib/gstdio.h>

#ifdef G_OS_WIN32
/* Work around namespace clobbage in <windows.h> */
#define DATADIR windows_DATADIR
#include <windows.h>
#undef DATADIR
#undef interface
#endif

#include <libebook/e-book-client.h>
#include <libebook/e-book-query.h>

#include <glib/gi18n.h>

#include <gio/gio.h>

#include <libedataserver/e-data-server-util.h>
#include <libedataserver/e-flag.h>
#include <libedataserver/e-proxy.h>

#include "libemail-utils/e-account-utils.h"
#include "libemail-utils/mail-mt.h"

#include "e-mail-folder-utils.h"
#include "e-mail-session.h"
#include "e-mail-utils.h"
#include "mail-tools.h"

#define d(x)

/**
 * em_utils_folder_is_templates:
 * @folder: a #CamelFolder
 *
 * Decides if @folder is a Templates folder.
 *
 * Returns %TRUE if this is a Templates folder or %FALSE otherwise.
 **/

gboolean
em_utils_folder_is_templates (CamelFolder *folder)
{
	CamelFolder *local_templates_folder;
	CamelSession *session;
	CamelStore *store;
	EAccountList *account_list;
	EIterator *iterator;
	gchar *folder_uri;
	gboolean is_templates = FALSE;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	store = camel_folder_get_parent_store (folder);
	session = camel_service_get_session (CAMEL_SERVICE (store));

	local_templates_folder =
		e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_TEMPLATES);

	if (folder == local_templates_folder)
		return TRUE;

	folder_uri = e_mail_folder_uri_from_folder (folder);

	account_list = e_get_account_list ();
	iterator = e_list_get_iterator (E_LIST (account_list));

	while (!is_templates && e_iterator_is_valid (iterator)) {
		EAccount *account;

		/* XXX EIterator misuses const. */
		account = (EAccount *) e_iterator_get (iterator);

		if (account->templates_folder_uri != NULL)
			is_templates = e_mail_folder_uri_equal (
				session, folder_uri,
				account->templates_folder_uri);

		e_iterator_next (iterator);
	}

	g_object_unref (iterator);
	g_free (folder_uri);

	return is_templates;
}

/**
 * em_utils_folder_is_drafts:
 * @folder: a #CamelFolder
 *
 * Decides if @folder is a Drafts folder.
 *
 * Returns %TRUE if this is a Drafts folder or %FALSE otherwise.
 **/
gboolean
em_utils_folder_is_drafts (CamelFolder *folder)
{
	CamelFolder *local_drafts_folder;
	CamelSession *session;
	CamelStore *store;
	MailFolderCache *cache;
	EMailSession *mail_session;
	CamelFolderInfoFlags flags = 0;
	EAccountList *account_list;
	EIterator *iterator;
	gchar *folder_uri;
	gboolean is_drafts = FALSE;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	store = camel_folder_get_parent_store (folder);
	session = camel_service_get_session (CAMEL_SERVICE (store));
	mail_session = E_MAIL_SESSION (session);

	local_drafts_folder =
		e_mail_session_get_local_folder (
		mail_session, E_MAIL_LOCAL_FOLDER_DRAFTS);

	if (folder == local_drafts_folder)
		return TRUE;

	cache = e_mail_session_get_folder_cache (mail_session);

	/* user can select Inbox as his Draft folder - in that case prefer Inbox type */
	if (mail_folder_cache_get_folder_info_flags (cache, folder, &flags) &&
	    (flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX)
		return FALSE;

	folder_uri = e_mail_folder_uri_from_folder (folder);

	account_list = e_get_account_list ();
	iterator = e_list_get_iterator (E_LIST (account_list));

	while (!is_drafts && e_iterator_is_valid (iterator)) {
		EAccount *account;

		/* XXX EIterator misuses const. */
		account = (EAccount *) e_iterator_get (iterator);

		if (account->drafts_folder_uri != NULL)
			is_drafts = e_mail_folder_uri_equal (
				session, folder_uri,
				account->drafts_folder_uri);

		e_iterator_next (iterator);
	}

	g_object_unref (iterator);
	g_free (folder_uri);

	return is_drafts;
}

/**
 * em_utils_folder_is_sent:
 * @folder: a #CamelFolder
 *
 * Decides if @folder is a Sent folder.
 *
 * Returns %TRUE if this is a Sent folder or %FALSE otherwise.
 **/
gboolean
em_utils_folder_is_sent (CamelFolder *folder)
{
	CamelFolder *local_sent_folder;
	CamelSession *session;
	CamelStore *store;
	MailFolderCache *cache;
	EMailSession *mail_session;
	CamelFolderInfoFlags flags = 0;
	EAccountList *account_list;
	EIterator *iterator;
	gchar *folder_uri;
	gboolean is_sent = FALSE;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	store = camel_folder_get_parent_store (folder);
	session = camel_service_get_session (CAMEL_SERVICE (store));
	mail_session = E_MAIL_SESSION (session);

	local_sent_folder =
		e_mail_session_get_local_folder (
		mail_session, E_MAIL_LOCAL_FOLDER_SENT);

	if (folder == local_sent_folder)
		return TRUE;

	cache = e_mail_session_get_folder_cache (mail_session);

	/* user can select Inbox as his Sent folder - in that case prefer Inbox type */
	if (mail_folder_cache_get_folder_info_flags (cache, folder, &flags) &&
	    (flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX)
		return FALSE;

	folder_uri = e_mail_folder_uri_from_folder (folder);

	account_list = e_get_account_list ();
	iterator = e_list_get_iterator (E_LIST (account_list));

	while (!is_sent && e_iterator_is_valid (iterator)) {
		EAccount *account;

		/* XXX EIterator misuses const. */
		account = (EAccount *) e_iterator_get (iterator);

		if (account->sent_folder_uri != NULL)
			is_sent = e_mail_folder_uri_equal (
				session, folder_uri,
				account->sent_folder_uri);

		e_iterator_next (iterator);
	}

	g_object_unref (iterator);
	g_free (folder_uri);

	return is_sent;
}

/**
 * em_utils_folder_is_outbox:
 * @folder: a #CamelFolder
 *
 * Decides if @folder is an Outbox folder.
 *
 * Returns %TRUE if this is an Outbox folder or %FALSE otherwise.
 **/
gboolean
em_utils_folder_is_outbox (CamelFolder *folder)
{
	CamelStore *store;
	CamelSession *session;
	CamelFolder *local_outbox_folder;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	store = camel_folder_get_parent_store (folder);
	session = camel_service_get_session (CAMEL_SERVICE (store));

	local_outbox_folder =
		e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_OUTBOX);

	return (folder == local_outbox_folder);
}

/* ********************************************************************** */

/* runs sync, in main thread */
static gpointer
emu_addr_setup (gpointer user_data)
{
	GError *err = NULL;
	ESourceList **psource_list = user_data;

	if (!e_book_client_get_sources (psource_list, &err))
		g_error_free (err);

	return NULL;
}

static void
emu_addr_cancel_stop (gpointer data)
{
	gboolean *stop = data;

	g_return_if_fail (stop != NULL);

	*stop = TRUE;
}

static void
emu_addr_cancel_cancellable (gpointer data)
{
	GCancellable *cancellable = data;

	g_return_if_fail (cancellable != NULL);

	g_cancellable_cancel (cancellable);
}

struct TryOpenEBookStruct {
	GError **error;
	EFlag *flag;
	gboolean result;
};

static void
try_open_book_client_cb (GObject *source_object,
                         GAsyncResult *result,
                         gpointer closure)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	struct TryOpenEBookStruct *data = (struct TryOpenEBookStruct *) closure;
	GError *error = NULL;

	if (!data)
		return;

	e_client_open_finish (E_CLIENT (book_client), result, &error);

	data->result = error == NULL;

	if (!data->result) {
		g_clear_error (data->error);
		g_propagate_error (data->error, error);
	}

	e_flag_set (data->flag);
}

/*
 * try_open_book_client:
 * Tries to open address book asynchronously, but acts as synchronous.
 * The advantage is it checks periodically whether the camel_operation
 * has been canceled or not, and if so, then stops immediately, with
 * result FALSE. Otherwise returns same as e_client_open()
 */
static gboolean
try_open_book_client (EBookClient *book_client,
                      gboolean only_if_exists,
                      GCancellable *cancellable,
                      GError **error)
{
	struct TryOpenEBookStruct data;
	gboolean canceled = FALSE;
	EFlag *flag = e_flag_new ();

	data.error = error;
	data.flag = flag;
	data.result = FALSE;

	e_client_open (
		E_CLIENT (book_client), only_if_exists,
		cancellable, try_open_book_client_cb, &data);

	while (canceled = g_cancellable_is_cancelled (cancellable),
			!canceled && !e_flag_is_set (flag)) {
		GTimeVal wait;

		g_get_current_time (&wait);
		g_time_val_add (&wait, 250000); /* waits 250ms */

		e_flag_timed_wait (flag, &wait);
	}

	if (canceled) {
		g_cancellable_cancel (cancellable);

		g_clear_error (error);
		g_propagate_error (
			error, e_client_error_create (
			E_CLIENT_ERROR_CANCELLED, NULL));
	}

	e_flag_wait (flag);
	e_flag_free (flag);

	return data.result && (!error || !*error);
}

#define NOT_FOUND_BOOK (GINT_TO_POINTER (1))

G_LOCK_DEFINE_STATIC (contact_cache);

/* key is lowercased contact email; value is EBook pointer
 * (just for comparison) where it comes from */
static GHashTable *contact_cache = NULL;

/* key is source ID; value is pointer to EBook */
static GHashTable *emu_books_hash = NULL;

/* key is source ID; value is same pointer as key; this is hash of
 * broken books, which failed to open for some reason */
static GHashTable *emu_broken_books_hash = NULL;

static ESourceList *emu_books_source_list = NULL;

static gboolean
search_address_in_addressbooks (const gchar *address,
                                gboolean local_only,
                                gboolean (*check_contact) (EContact *contact,
                                                           gpointer user_data),
                                gpointer user_data)
{
	gboolean found = FALSE, stop = FALSE, found_any = FALSE;
	gchar *lowercase_addr;
	gpointer ptr;
	EBookQuery *book_query;
	gchar *query;
	GSList *s, *g, *addr_sources = NULL;
	GHook *hook_cancellable;
	GCancellable *cancellable;

	if (!address || !*address)
		return FALSE;

	G_LOCK (contact_cache);

	if (!emu_books_source_list) {
		mail_call_main (
			MAIL_CALL_p_p, (MailMainFunc)
			emu_addr_setup, &emu_books_source_list);
		emu_books_hash = g_hash_table_new_full (
			g_str_hash, g_str_equal, g_free, g_object_unref);
		emu_broken_books_hash = g_hash_table_new_full (
			g_str_hash, g_str_equal, g_free, NULL);
		contact_cache = g_hash_table_new_full (
			g_str_hash, g_str_equal, g_free, NULL);
	}

	if (!emu_books_source_list) {
		G_UNLOCK (contact_cache);
		return FALSE;
	}

	lowercase_addr = g_utf8_strdown (address, -1);
	ptr = g_hash_table_lookup (contact_cache, lowercase_addr);
	if (ptr != NULL && (check_contact == NULL || ptr == NOT_FOUND_BOOK)) {
		g_free (lowercase_addr);
		G_UNLOCK (contact_cache);
		return ptr != NOT_FOUND_BOOK;
	}

	book_query = e_book_query_field_test (E_CONTACT_EMAIL, E_BOOK_QUERY_IS, address);
	query = e_book_query_to_string (book_query);
	e_book_query_unref (book_query);

	for (g = e_source_list_peek_groups (emu_books_source_list);
			g; g = g_slist_next (g)) {
		ESourceGroup *group = g->data;

		if (!group)
			continue;

		if (local_only && !(e_source_group_peek_base_uri (group) &&
			g_str_has_prefix (
			e_source_group_peek_base_uri (group), "local:")))
			continue;

		for (s = e_source_group_peek_sources (group); s; s = g_slist_next (s)) {
			ESource *source = s->data;
			const gchar *completion = e_source_get_property (source, "completion");

			if (completion && g_ascii_strcasecmp (completion, "true") == 0) {
				addr_sources = g_slist_prepend (
					addr_sources, g_object_ref (source));
			}
		}
	}

	cancellable = g_cancellable_new ();
	hook_cancellable = mail_cancel_hook_add (
		emu_addr_cancel_cancellable, cancellable);

	for (s = addr_sources; !stop && !found && s; s = g_slist_next (s)) {
		ESource *source = s->data;
		GSList *contacts;
		EBookClient *book_client = NULL;
		GHook *hook_stop;
		gboolean cached_book = FALSE;
		const gchar *display_name;
		const gchar *uid;
		GError *err = NULL;

		uid = e_source_peek_uid (source);
		display_name = e_source_peek_name (source);

		/* failed to load this book last time, skip it now */
		if (g_hash_table_lookup (emu_broken_books_hash, uid) != NULL) {
			d(printf ("%s: skipping broken book '%s'\n",
				G_STRFUNC, display_name));
			continue;
		}

		d(printf(" checking '%s'\n", e_source_get_uri(source)));

		hook_stop = mail_cancel_hook_add (emu_addr_cancel_stop, &stop);

		book_client = g_hash_table_lookup (emu_books_hash, uid);
		if (!book_client) {
			book_client = e_book_client_new (source, &err);

			if (book_client == NULL) {
				if (err && (g_error_matches (err, E_CLIENT_ERROR, E_CLIENT_ERROR_CANCELLED) ||
				    g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))) {
					stop = TRUE;
				} else if (err) {
					gchar *source_uid;

					source_uid = g_strdup (uid);

					g_hash_table_insert (
						emu_broken_books_hash,
						source_uid, source_uid);

					g_warning (
						"%s: Unable to create addressbook '%s': %s",
						G_STRFUNC,
						display_name,
						err->message);
				}
				g_clear_error (&err);
			} else if (!stop && !try_open_book_client (book_client, TRUE, cancellable, &err)) {
				g_object_unref (book_client);
				book_client = NULL;

				if (err && (g_error_matches (err, E_CLIENT_ERROR, E_CLIENT_ERROR_CANCELLED) ||
				    g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))) {
					stop = TRUE;
				} else if (err) {
					gchar *source_uid;

					source_uid = g_strdup (uid);

					g_hash_table_insert (
						emu_broken_books_hash,
						source_uid, source_uid);

					g_warning (
						"%s: Unable to open addressbook '%s': %s",
						G_STRFUNC,
						display_name,
						err->message);
				}
				g_clear_error (&err);
			}
		} else {
			cached_book = TRUE;
		}

		if (book_client && !stop &&
		    e_book_client_get_contacts_sync (
		    book_client, query, &contacts, cancellable, &err)) {
			if (contacts != NULL) {
				if (!found_any) {
					g_hash_table_insert (
						contact_cache,
						g_strdup (lowercase_addr),
						book_client);
				}
				found_any = TRUE;

				if (check_contact) {
					GSList *l;

					for (l = contacts; l && !found; l = l->next) {
						EContact *contact = l->data;

						found = check_contact (contact, user_data);
					}
				} else {
					found = TRUE;
				}

				g_slist_foreach (contacts, (GFunc) g_object_unref, NULL);
				g_slist_free (contacts);
			}
		} else if (book_client) {
			stop = stop || (err &&
			    (g_error_matches (err, E_CLIENT_ERROR, E_CLIENT_ERROR_CANCELLED) ||
			     g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)));
			if (err && !stop) {
				gchar *source_uid = g_strdup (uid);

				g_hash_table_insert (
					emu_broken_books_hash,
					source_uid, source_uid);

				g_warning (
					"%s: Can't get contacts from '%s': %s",
					G_STRFUNC,
					display_name,
					err->message);
			}
			g_clear_error (&err);
		}

		mail_cancel_hook_remove (hook_stop);

		if (stop && !cached_book && book_client) {
			g_object_unref (book_client);
		} else if (!stop && book_client && !cached_book) {
			g_hash_table_insert (
				emu_books_hash, g_strdup (uid), book_client);
		}
	}

	mail_cancel_hook_remove (hook_cancellable);
	g_object_unref (cancellable);

	g_slist_free_full (addr_sources, (GDestroyNotify) g_object_unref);

	g_free (query);

	if (!found_any) {
		g_hash_table_insert (contact_cache, lowercase_addr, NOT_FOUND_BOOK);
		lowercase_addr = NULL;
	}

	G_UNLOCK (contact_cache);

	g_free (lowercase_addr);

	return found_any;
}

gboolean
em_utils_in_addressbook (CamelInternetAddress *iaddr,
                         gboolean local_only)
{
	const gchar *addr;

	/* TODO: check all addresses? */
	if (iaddr == NULL || !camel_internet_address_get (iaddr, 0, NULL, &addr))
		return FALSE;

	return search_address_in_addressbooks (addr, local_only, NULL, NULL);
}

static gboolean
extract_photo_data (EContact *contact,
                    gpointer user_data)
{
	EContactPhoto **photo = user_data;

	g_return_val_if_fail (contact != NULL, FALSE);
	g_return_val_if_fail (user_data != NULL, FALSE);

	*photo = e_contact_get (contact, E_CONTACT_PHOTO);
	if (!*photo)
		*photo = e_contact_get (contact, E_CONTACT_LOGO);

	return *photo != NULL;
}

typedef struct _PhotoInfo {
	gchar *address;
	EContactPhoto *photo;
} PhotoInfo;

static void
emu_free_photo_info (PhotoInfo *pi)
{
	if (!pi)
		return;

	if (pi->address)
		g_free (pi->address);
	if (pi->photo)
		e_contact_photo_free (pi->photo);
	g_free (pi);
}

G_LOCK_DEFINE_STATIC (photos_cache);
static GSList *photos_cache = NULL; /* list of PhotoInfo-s */

CamelMimePart *
em_utils_contact_photo (CamelInternetAddress *cia,
                        gboolean local_only)
{
	const gchar *addr = NULL;
	CamelMimePart *part = NULL;
	EContactPhoto *photo = NULL;
	GSList *p, *first_not_null = NULL;
	gint count_not_null = 0;

	if (cia == NULL || !camel_internet_address_get (cia, 0, NULL, &addr) || !addr) {
		return NULL;
	}

	G_LOCK (photos_cache);

	/* search a cache first */
	for (p = photos_cache; p; p = p->next) {
		PhotoInfo *pi = p->data;

		if (!pi)
			continue;

		if (pi->photo) {
			if (!first_not_null)
				first_not_null = p;
			count_not_null++;
		}

		if (g_ascii_strcasecmp (addr, pi->address) == 0) {
			photo = pi->photo;
			break;
		}
	}

	/* !p means the address had not been found in the cache */
	if (!p && search_address_in_addressbooks (
			addr, local_only, extract_photo_data, &photo)) {
		PhotoInfo *pi;

		if (photo && photo->type != E_CONTACT_PHOTO_TYPE_INLINED) {
			e_contact_photo_free (photo);
			photo = NULL;
		}

		/* keep only up to 10 photos in memory */
		if (photo && count_not_null >= 10 && first_not_null) {
			pi = first_not_null->data;

			photos_cache = g_slist_remove (photos_cache, pi);

			emu_free_photo_info (pi);
		}

		pi = g_new0 (PhotoInfo, 1);
		pi->address = g_strdup (addr);
		pi->photo = photo;

		photos_cache = g_slist_append (photos_cache, pi);
	}

	/* some photo found, use it */
	if (photo) {
		/* Form a mime part out of the photo */
		part = camel_mime_part_new ();
		camel_mime_part_set_content (part,
				    (const gchar *) photo->data.inlined.data,
				    photo->data.inlined.length, "image/jpeg");
	}

	G_UNLOCK (photos_cache);

	return part;
}

/* list of email addresses (strings) to remove from local cache of photos and
 * contacts, but only if the photo doesn't exist or is an not-found contact */
void
emu_remove_from_mail_cache (const GSList *addresses)
{
	const GSList *a;
	GSList *p;
	CamelInternetAddress *cia;

	cia = camel_internet_address_new ();

	for (a = addresses; a; a = a->next) {
		const gchar *addr = NULL;

		if (!a->data)
			continue;

		if (camel_address_decode ((CamelAddress *) cia, a->data) != -1 &&
		    camel_internet_address_get (cia, 0, NULL, &addr) && addr) {
			gchar *lowercase_addr = g_utf8_strdown (addr, -1);

			G_LOCK (contact_cache);
			if (g_hash_table_lookup (contact_cache, lowercase_addr) == NOT_FOUND_BOOK)
				g_hash_table_remove (contact_cache, lowercase_addr);
			G_UNLOCK (contact_cache);

			g_free (lowercase_addr);

			G_LOCK (photos_cache);
			for (p = photos_cache; p; p = p->next) {
				PhotoInfo *pi = p->data;

				if (pi && !pi->photo && g_ascii_strcasecmp (pi->address, addr) == 0) {
					photos_cache = g_slist_remove (photos_cache, pi);
					emu_free_photo_info (pi);
					break;
				}
			}
			G_UNLOCK (photos_cache);
		}
	}

	g_object_unref (cia);
}

void
emu_remove_from_mail_cache_1 (const gchar *address)
{
	GSList *l;

	g_return_if_fail (address != NULL);

	l = g_slist_append (NULL, (gpointer) address);

	emu_remove_from_mail_cache (l);

	g_slist_free (l);
}

/* frees all data created by call of em_utils_in_addressbook() or
 * em_utils_contact_photo() */
void
emu_free_mail_cache (void)
{
	G_LOCK (contact_cache);

	if (emu_books_hash) {
		g_hash_table_destroy (emu_books_hash);
		emu_books_hash = NULL;
	}

	if (emu_broken_books_hash) {
		g_hash_table_destroy (emu_broken_books_hash);
		emu_broken_books_hash = NULL;
	}

	if (emu_books_source_list) {
		g_object_unref (emu_books_source_list);
		emu_books_source_list = NULL;
	}

	if (contact_cache) {
		g_hash_table_destroy (contact_cache);
		contact_cache = NULL;
	}

	G_UNLOCK (contact_cache);

	G_LOCK (photos_cache);

	g_slist_foreach (photos_cache, (GFunc) emu_free_photo_info, NULL);
	g_slist_free (photos_cache);
	photos_cache = NULL;

	G_UNLOCK (photos_cache);
}

static EAccount *
guess_account_from_folder (CamelFolder *folder)
{
	CamelStore *store;
	const gchar *uid;

	store = camel_folder_get_parent_store (folder);
	uid = camel_service_get_uid (CAMEL_SERVICE (store));

	return e_get_account_by_uid (uid);
}

static EAccount *
guess_account_from_message (CamelMimeMessage *message)
{
	const gchar *uid;

	uid = camel_mime_message_get_source (message);

	return (uid != NULL) ? e_get_account_by_uid (uid) : NULL;
}

EAccount *
em_utils_guess_account (CamelMimeMessage *message,
                        CamelFolder *folder)
{
	EAccount *account = NULL;

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	if (folder != NULL)
		g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	/* check for newsgroup header */
	if (folder != NULL
	    && camel_medium_get_header (CAMEL_MEDIUM (message), "Newsgroups"))
		account = guess_account_from_folder (folder);

	/* check for source folder */
	if (account == NULL && folder != NULL)
		account = guess_account_from_folder (folder);

	/* then message source */
	if (account == NULL)
		account = guess_account_from_message (message);

	return account;
}

EAccount *
em_utils_guess_account_with_recipients (CamelMimeMessage *message,
                                        CamelFolder *folder)
{
	EAccount *account = NULL;
	EAccountList *account_list;
	GHashTable *recipients;
	EIterator *iterator;
	CamelInternetAddress *addr;
	const gchar *type;
	const gchar *key;

	/* This policy is subject to debate and tweaking,
	 * but please also document the rational here. */

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	/* Build a set of email addresses in which to test for membership.
	 * Only the keys matter here; the values just need to be non-NULL. */
	recipients = g_hash_table_new (g_str_hash, g_str_equal);

	type = CAMEL_RECIPIENT_TYPE_TO;
	addr = camel_mime_message_get_recipients (message, type);
	if (addr != NULL) {
		gint index = 0;

		while (camel_internet_address_get (addr, index++, NULL, &key))
			g_hash_table_insert (
				recipients, (gpointer) key,
				GINT_TO_POINTER (1));
	}

	type = CAMEL_RECIPIENT_TYPE_CC;
	addr = camel_mime_message_get_recipients (message, type);
	if (addr != NULL) {
		gint index = 0;

		while (camel_internet_address_get (addr, index++, NULL, &key))
			g_hash_table_insert (
				recipients, (gpointer) key,
				GINT_TO_POINTER (1));
	}

	/* First Preference: We were given a folder that maps to an
	 * enabled account, and that account's email address appears
	 * in the list of To: or Cc: recipients. */

	if (folder != NULL)
		account = guess_account_from_folder (folder);

	if (account == NULL || !account->enabled)
		goto second_preference;

	if ((key = account->id->address) == NULL)
		goto second_preference;

	if (g_hash_table_lookup (recipients, key) != NULL)
		goto exit;

second_preference:

	/* Second Preference: Choose any enabled account whose email
	 * address appears in the list to To: or Cc: recipients. */

	account_list = e_get_account_list ();
	iterator = e_list_get_iterator (E_LIST (account_list));

	while (e_iterator_is_valid (iterator)) {
		account = (EAccount *) e_iterator_get (iterator);
		e_iterator_next (iterator);

		if (account == NULL || !account->enabled)
			continue;

		if ((key = account->id->address) == NULL)
			continue;

		if (g_hash_table_lookup (recipients, key) != NULL) {
			g_object_unref (iterator);
			goto exit;
		}
	}
	g_object_unref (iterator);

	/* Last Preference: Defer to em_utils_guess_account(). */
	account = em_utils_guess_account (message, folder);

exit:
	g_hash_table_destroy (recipients);

	return account;
}

static void
cancel_service_connect_cb (GCancellable *cancellable,
                           CamelService *service)
{
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	camel_service_cancel_connect (service);
}

gboolean
em_utils_connect_service_sync (CamelService *service,
                               GCancellable *cancellable,
                               GError **error)
{
	gboolean res;
	gulong handler_id = 0;

	g_return_val_if_fail (CAMEL_IS_SERVICE (service), FALSE);

	if (cancellable != NULL)
		handler_id = g_cancellable_connect (
			cancellable,
			G_CALLBACK (cancel_service_connect_cb),
			service, NULL);

	res = camel_service_connect_sync (service, error);

	if (handler_id)
		g_cancellable_disconnect (cancellable, handler_id);

	return res;
}

gboolean
em_utils_disconnect_service_sync (CamelService *service,
                                  gboolean clean,
                                  GCancellable *cancellable,
                                  GError **error)
{
	gboolean res;
	gulong handler_id = 0;

	g_return_val_if_fail (CAMEL_IS_SERVICE (service), FALSE);

	if (cancellable != NULL)
		handler_id = g_cancellable_connect (
			cancellable,
			G_CALLBACK (cancel_service_connect_cb),
			service, NULL);

	res = camel_service_disconnect_sync (service, clean, error);

	if (handler_id)
		g_cancellable_disconnect (cancellable, handler_id);

	return res;
}

/**
 * em_utils_uids_free:
 * @uids: array of uids
 *
 * Frees the array of uids pointed to by @uids back to the system.
 **/
void
em_utils_uids_free (GPtrArray *uids)
{
	gint i;

	for (i = 0; i < uids->len; i++)
		g_free (uids->pdata[i]);

	g_ptr_array_free (uids, TRUE);
}

/* Returns TRUE if CamelURL points to a local mbox file. */
gboolean
em_utils_is_local_delivery_mbox_file (CamelURL *url)
{
	g_return_val_if_fail (url != NULL, FALSE);

	return g_str_equal (url->protocol, "mbox") &&
		(url->path != NULL) &&
		g_file_test (url->path, G_FILE_TEST_EXISTS) &&
		!g_file_test (url->path, G_FILE_TEST_IS_DIR);
}

