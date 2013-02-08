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

#include <glib/gi18n.h>
#include <libebook/libebook.h>
#include <libedataserver/libedataserver.h>

#include <libemail-engine/mail-mt.h>

#include "e-mail-folder-utils.h"
#include "e-mail-session.h"
#include "e-mail-utils.h"
#include "mail-tools.h"

#define d(x)

/**
 * em_utils_folder_is_drafts:
 * @registry: an #ESourceRegistry
 * @folder: a #CamelFolder
 *
 * Decides if @folder is a Drafts folder.
 *
 * Returns %TRUE if this is a Drafts folder or %FALSE otherwise.
 **/
gboolean
em_utils_folder_is_drafts (ESourceRegistry *registry,
                           CamelFolder *folder)
{
	CamelFolder *local_drafts_folder;
	CamelSession *session;
	CamelStore *store;
	GList *list, *iter;
	gchar *folder_uri;
	gboolean is_drafts = FALSE;
	const gchar *extension_name;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	store = camel_folder_get_parent_store (folder);
	session = camel_service_get_session (CAMEL_SERVICE (store));

	local_drafts_folder =
		e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_DRAFTS);

	if (folder == local_drafts_folder)
		return TRUE;

	folder_uri = e_mail_folder_uri_from_folder (folder);

	store = camel_folder_get_parent_store (folder);
	session = camel_service_get_session (CAMEL_SERVICE (store));

	extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
	list = e_source_registry_list_sources (registry, extension_name);

	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		ESource *source = E_SOURCE (iter->data);
		ESourceExtension *extension;
		const gchar *drafts_folder_uri;

		extension = e_source_get_extension (source, extension_name);

		drafts_folder_uri =
			e_source_mail_composition_get_drafts_folder (
			E_SOURCE_MAIL_COMPOSITION (extension));

		if (drafts_folder_uri != NULL)
			is_drafts = e_mail_folder_uri_equal (
				session, folder_uri, drafts_folder_uri);

		if (is_drafts)
			break;
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);
	g_free (folder_uri);

	return is_drafts;
}

/**
 * em_utils_folder_is_templates:
 * @registry: an #ESourceRegistry
 * @folder: a #CamelFolder
 *
 * Decides if @folder is a Templates folder.
 *
 * Returns %TRUE if this is a Templates folder or %FALSE otherwise.
 **/

gboolean
em_utils_folder_is_templates (ESourceRegistry *registry,
                              CamelFolder *folder)
{
	CamelFolder *local_templates_folder;
	CamelSession *session;
	CamelStore *store;
	GList *list, *iter;
	gchar *folder_uri;
	gboolean is_templates = FALSE;
	const gchar *extension_name;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	store = camel_folder_get_parent_store (folder);
	session = camel_service_get_session (CAMEL_SERVICE (store));

	local_templates_folder =
		e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_TEMPLATES);

	if (folder == local_templates_folder)
		return TRUE;

	folder_uri = e_mail_folder_uri_from_folder (folder);

	store = camel_folder_get_parent_store (folder);
	session = camel_service_get_session (CAMEL_SERVICE (store));

	extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
	list = e_source_registry_list_sources (registry, extension_name);

	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		ESource *source = E_SOURCE (iter->data);
		ESourceExtension *extension;
		const gchar *templates_folder_uri;

		extension = e_source_get_extension (source, extension_name);

		templates_folder_uri =
			e_source_mail_composition_get_templates_folder (
			E_SOURCE_MAIL_COMPOSITION (extension));

		if (templates_folder_uri != NULL)
			is_templates = e_mail_folder_uri_equal (
				session, folder_uri, templates_folder_uri);

		if (is_templates)
			break;
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);
	g_free (folder_uri);

	return is_templates;
}

/**
 * em_utils_folder_is_sent:
 * @registry: an #ESourceRegistry
 * @folder: a #CamelFolder
 *
 * Decides if @folder is a Sent folder.
 *
 * Returns %TRUE if this is a Sent folder or %FALSE otherwise.
 **/
gboolean
em_utils_folder_is_sent (ESourceRegistry *registry,
                         CamelFolder *folder)
{
	CamelFolder *local_sent_folder;
	CamelSession *session;
	CamelStore *store;
	GList *list, *iter;
	gchar *folder_uri;
	gboolean is_sent = FALSE;
	const gchar *extension_name;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	store = camel_folder_get_parent_store (folder);
	session = camel_service_get_session (CAMEL_SERVICE (store));

	local_sent_folder =
		e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_SENT);

	if (folder == local_sent_folder)
		return TRUE;

	folder_uri = e_mail_folder_uri_from_folder (folder);

	store = camel_folder_get_parent_store (folder);
	session = camel_service_get_session (CAMEL_SERVICE (store));

	extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;
	list = e_source_registry_list_sources (registry, extension_name);

	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		ESource *source = E_SOURCE (iter->data);
		ESourceExtension *extension;
		const gchar *sent_folder_uri;

		extension = e_source_get_extension (source, extension_name);

		sent_folder_uri =
			e_source_mail_submission_get_sent_folder (
			E_SOURCE_MAIL_SUBMISSION (extension));

		if (sent_folder_uri != NULL)
			is_sent = e_mail_folder_uri_equal (
				session, folder_uri, sent_folder_uri);

		if (is_sent)
			break;
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);
	g_free (folder_uri);

	return is_sent;
}

/**
 * em_utils_folder_is_outbox:
 * @registry: an #ESourceRegistry
 * @folder: a #CamelFolder
 *
 * Decides if @folder is an Outbox folder.
 *
 * Returns %TRUE if this is an Outbox folder or %FALSE otherwise.
 **/
gboolean
em_utils_folder_is_outbox (ESourceRegistry *registry,
                           CamelFolder *folder)
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

struct TryOpenEBookStruct {
	GError **error;
	EFlag *flag;
	gboolean result;
};

extern gint camel_application_is_exiting;

#define NOT_FOUND_BOOK (GINT_TO_POINTER (1))

/* to be able to cancel pending requests on exit; this lock
 * should not be held while contact_cache lock is held */
G_LOCK_DEFINE_STATIC (search_addressbook_cancellables);
static GSList *search_addressbook_cancellables = NULL;

ECancellableMutex contact_cache_lock;

/* key is lowercased contact email; value is EBook pointer
 * (just for comparison) where it comes from */
static GHashTable *contact_cache = NULL;

/* key is source ID; value is pointer to EBook */
static GHashTable *emu_books_hash = NULL;

/* key is source ID; value is same pointer as key; this is hash of
 * broken books, which failed to open for some reason */
static GHashTable *emu_broken_books_hash = NULL;

static gboolean
search_address_in_addressbooks (ESourceRegistry *registry,
                                const gchar *address,
                                gboolean local_only,
                                gboolean (*check_contact) (EContact *contact,
                                                           gpointer user_data),
                                gpointer user_data,
                                GCancellable *cancellable)
{
	GList *list, *link;
	GList *addr_sources = NULL;
	gboolean found = FALSE, stop = FALSE, found_any = FALSE;
	gchar *lowercase_addr;
	gpointer ptr;
	EBookQuery *book_query;
	gchar *query;
	const gchar *extension_name;

	if (!address || !*address || g_cancellable_is_cancelled (cancellable) || camel_application_is_exiting)
		return FALSE;

	G_LOCK (search_addressbook_cancellables);
	if (cancellable)
		g_object_ref (cancellable);
	else
		cancellable = g_cancellable_new ();
	search_addressbook_cancellables = g_slist_prepend (search_addressbook_cancellables, cancellable);
	G_UNLOCK (search_addressbook_cancellables);

	if (!e_cancellable_mutex_lock (&contact_cache_lock, cancellable)) {
		G_LOCK (search_addressbook_cancellables);
		search_addressbook_cancellables = g_slist_remove (search_addressbook_cancellables, cancellable);
		g_object_unref (cancellable);
		G_UNLOCK (search_addressbook_cancellables);

		return FALSE;
	}

	if (camel_application_is_exiting || g_cancellable_is_cancelled (cancellable)) {
		e_cancellable_mutex_unlock (&contact_cache_lock);

		G_LOCK (search_addressbook_cancellables);
		search_addressbook_cancellables = g_slist_remove (search_addressbook_cancellables, cancellable);
		g_object_unref (cancellable);
		G_UNLOCK (search_addressbook_cancellables);

		return FALSE;
	}

	if (emu_books_hash == NULL) {
		emu_books_hash = g_hash_table_new_full (
			g_str_hash, g_str_equal, g_free, g_object_unref);
		emu_broken_books_hash = g_hash_table_new_full (
			g_str_hash, g_str_equal, g_free, NULL);
		contact_cache = g_hash_table_new_full (
			g_str_hash, g_str_equal, g_free, NULL);
	}

	lowercase_addr = g_utf8_strdown (address, -1);
	ptr = g_hash_table_lookup (contact_cache, lowercase_addr);
	if (ptr != NULL && (check_contact == NULL || ptr == NOT_FOUND_BOOK)) {
		g_free (lowercase_addr);
		e_cancellable_mutex_unlock (&contact_cache_lock);

		G_LOCK (search_addressbook_cancellables);
		search_addressbook_cancellables = g_slist_remove (search_addressbook_cancellables, cancellable);
		g_object_unref (cancellable);
		G_UNLOCK (search_addressbook_cancellables);

		return ptr != NOT_FOUND_BOOK;
	}

	book_query = e_book_query_field_test (E_CONTACT_EMAIL, E_BOOK_QUERY_IS, address);
	query = e_book_query_to_string (book_query);
	e_book_query_unref (book_query);

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL && !g_cancellable_is_cancelled (cancellable); link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceExtension *extension;
		const gchar *backend_name;
		gboolean source_is_local;
		gboolean autocomplete;

		extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
		extension = e_source_get_extension (source, extension_name);

		backend_name = e_source_backend_get_backend_name (
			E_SOURCE_BACKEND (extension));

		source_is_local = (g_strcmp0 (backend_name, "local") == 0);

		if (local_only && !source_is_local)
			continue;

		extension_name = E_SOURCE_EXTENSION_AUTOCOMPLETE;
		extension = e_source_get_extension (source, extension_name);

		autocomplete = e_source_autocomplete_get_include_me (
			E_SOURCE_AUTOCOMPLETE (extension));

		if (!autocomplete)
			continue;

		addr_sources = g_list_prepend (
			addr_sources, g_object_ref (source));
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	stop = g_cancellable_is_cancelled (cancellable);

	for (link = addr_sources; !stop && !found && link != NULL
	     && !g_cancellable_is_cancelled (cancellable);
	     link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		GSList *contacts;
		EBookClient *book_client = NULL;
		gboolean cached_book = FALSE;
		const gchar *display_name;
		const gchar *uid;
		GError *err = NULL;

		uid = e_source_get_uid (source);
		display_name = e_source_get_display_name (source);

		/* failed to load this book last time, skip it now */
		if (g_hash_table_lookup (emu_broken_books_hash, uid) != NULL)
			continue;

		d (printf (" checking '%s'\n", e_source_get_uri (source)));

		book_client = g_hash_table_lookup (emu_books_hash, uid);
		if (!book_client) {
			/* FIXME This blocks, but this entire function is
			 *       in desperate need of a rewrite.  This is
			 *       horribly convoluted, even for Evolution! */
			book_client = (EBookClient *)
				e_book_client_connect_sync (source, NULL, &err);

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
			} else if (!stop) {
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

		if (stop && !cached_book && book_client) {
			g_object_unref (book_client);
		} else if (!stop && book_client && !cached_book) {
			g_hash_table_insert (
				emu_books_hash, g_strdup (uid), book_client);
		}
	}

	g_list_free_full (addr_sources, (GDestroyNotify) g_object_unref);

	g_free (query);

	if (!found_any) {
		g_hash_table_insert (contact_cache, lowercase_addr, NOT_FOUND_BOOK);
		lowercase_addr = NULL;
	}

	e_cancellable_mutex_unlock (&contact_cache_lock);

	g_free (lowercase_addr);

	G_LOCK (search_addressbook_cancellables);
	search_addressbook_cancellables = g_slist_remove (search_addressbook_cancellables, cancellable);
	g_object_unref (cancellable);
	G_UNLOCK (search_addressbook_cancellables);

	return found_any;
}

gboolean
em_utils_in_addressbook (ESourceRegistry *registry,
                         CamelInternetAddress *iaddr,
                         gboolean local_only,
                         GCancellable *cancellable)
{
	const gchar *addr;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);

	/* TODO: check all addresses? */
	if (iaddr == NULL || !camel_internet_address_get (iaddr, 0, NULL, &addr))
		return FALSE;

	return search_address_in_addressbooks (
		registry, addr, local_only, NULL, NULL, cancellable);
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

static ECancellableMutex photos_cache_lock;
static GSList *photos_cache = NULL; /* list of PhotoInfo-s */

CamelMimePart *
em_utils_contact_photo (ESourceRegistry *registry,
                        CamelInternetAddress *cia,
                        gboolean local_only,
                        GCancellable *cancellable)
{
	const gchar *addr = NULL;
	CamelMimePart *part = NULL;
	EContactPhoto *photo = NULL;
	GSList *p, *last = NULL;
	gint cache_len;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	if (cia == NULL || !camel_internet_address_get (cia, 0, NULL, &addr) || !addr) {
		return NULL;
	}

	if (!e_cancellable_mutex_lock (&photos_cache_lock, cancellable))
		return NULL;

	/* search a cache first */
	cache_len = 0;
	last = NULL;
	for (p = photos_cache; p; p = p->next) {
		PhotoInfo *pi = p->data;

		if (!pi)
			continue;

		if (g_ascii_strcasecmp (addr, pi->address) == 0) {
			photo = pi->photo;
			break;
		}

		cache_len++;
		last = p;
	}

	/* !p means the address had not been found in the cache */
	if (!p && search_address_in_addressbooks (
		registry, addr, local_only, extract_photo_data, &photo, cancellable)) {

		PhotoInfo *pi;

		/* keep only up to 10 photos in memory */
		if (last && (cache_len >= 10)) {
			pi = last->data;
			photos_cache = g_slist_remove (photos_cache, pi);

			if (pi)
				emu_free_photo_info (pi);
		}

		pi = g_new0 (PhotoInfo, 1);
		pi->address = g_strdup (addr);
		pi->photo = photo;

		photos_cache = g_slist_prepend (photos_cache, pi);
	}

	/* some photo found, use it */
	if (photo) {
		/* Form a mime part out of the photo */
		part = camel_mime_part_new ();

		if (photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
			camel_mime_part_set_content (
				part,
				(const gchar *) photo->data.inlined.data,
				photo->data.inlined.length, "image/jpeg");
		} else {
			gchar *s = g_filename_from_uri (photo->data.uri, NULL, NULL);
			camel_mime_part_set_filename (part, s);
			g_free (s);
		}
	}

	e_cancellable_mutex_unlock (&photos_cache_lock);

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

			if (e_cancellable_mutex_lock (&contact_cache_lock, NULL)) {
				if (g_hash_table_lookup (contact_cache, lowercase_addr) == NOT_FOUND_BOOK)
					g_hash_table_remove (contact_cache, lowercase_addr);
				e_cancellable_mutex_unlock (&contact_cache_lock);
			}

			g_free (lowercase_addr);

			if (e_cancellable_mutex_lock (&photos_cache_lock, NULL)) {
				for (p = photos_cache; p; p = p->next) {
					PhotoInfo *pi = p->data;

					if (pi && !pi->photo && g_ascii_strcasecmp (pi->address, addr) == 0) {
						photos_cache = g_slist_remove (photos_cache, pi);
						emu_free_photo_info (pi);
						break;
					}
				}
				e_cancellable_mutex_unlock (&photos_cache_lock);
			}
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

struct FreeMailCacheData
{
	GDestroyNotify done_cb;
	gpointer user_data;
};

static gboolean
free_mail_cache_idle (gpointer user_data)
{
	struct FreeMailCacheData *fmc = user_data;

	g_return_val_if_fail (fmc != NULL, FALSE);

	if (fmc->done_cb)
		fmc->done_cb (fmc->user_data);
	g_free (fmc);

	return FALSE;
}

static gpointer
free_mail_cache_thread (gpointer user_data)
{
	g_return_val_if_fail (user_data != NULL, NULL);

	if (e_cancellable_mutex_lock (&contact_cache_lock, NULL)) {
		if (emu_books_hash) {
			g_hash_table_destroy (emu_books_hash);
			emu_books_hash = NULL;
		}

		if (emu_broken_books_hash) {
			g_hash_table_destroy (emu_broken_books_hash);
			emu_broken_books_hash = NULL;
		}

		if (contact_cache) {
			g_hash_table_destroy (contact_cache);
			contact_cache = NULL;
		}

		e_cancellable_mutex_unlock (&contact_cache_lock);
	}

	if (e_cancellable_mutex_lock (&photos_cache_lock, NULL)) {
		g_slist_foreach (photos_cache, (GFunc) emu_free_photo_info, NULL);
		g_slist_free (photos_cache);
		photos_cache = NULL;

		e_cancellable_mutex_unlock (&photos_cache_lock);
	}

	/* schedule with priority higher than gtk+ uses for animations (check docs for G_PRIORITY_HIGH_IDLE) */
	g_idle_add_full (G_PRIORITY_HIGH, free_mail_cache_idle, user_data, NULL);

	return NULL;
}

/* frees all data created by call of em_utils_in_addressbook() or
 * em_utils_contact_photo(); done_cb is called when freeing is done,
 * in an idle callback
 */
void
emu_free_mail_cache (GDestroyNotify done_cb,
                     gpointer user_data)
{
	struct FreeMailCacheData *fmc;
	GThread *thread;

	G_LOCK (search_addressbook_cancellables);
	g_slist_foreach (search_addressbook_cancellables, (GFunc) g_cancellable_cancel, NULL);
	G_UNLOCK (search_addressbook_cancellables);

	fmc = g_new0 (struct FreeMailCacheData, 1);
	fmc->done_cb = done_cb;
	fmc->user_data = user_data;

	thread = g_thread_new (NULL, free_mail_cache_thread, fmc);
	g_thread_unref (thread);
}

static ESource *
guess_mail_account_from_folder (ESourceRegistry *registry,
                                CamelFolder *folder,
                                const gchar *message_uid)
{
	ESource *source;
	CamelStore *store;
	const gchar *uid;

	/* Lookup an ESource by CamelStore UID. */
	store = camel_folder_get_parent_store (folder);
	if (message_uid && folder && CAMEL_IS_VEE_STORE (store)) {
		CamelMessageInfo *mi = camel_folder_get_message_info (folder, message_uid);
		if (mi) {
			CamelFolder *location;

			location = camel_vee_folder_get_location (CAMEL_VEE_FOLDER (folder), (CamelVeeMessageInfo *) mi, NULL);
			if (location)
				store = camel_folder_get_parent_store (location);
			camel_folder_free_message_info (folder, mi);
		}
	}

	uid = camel_service_get_uid (CAMEL_SERVICE (store));
	source = e_source_registry_ref_source (registry, uid);

	/* If we found an ESource, make sure it's a mail account. */
	if (source != NULL) {
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
		if (!e_source_has_extension (source, extension_name)) {
			g_object_unref (source);
			source = NULL;
		}
	}

	return source;
}

static ESource *
guess_mail_account_from_message (ESourceRegistry *registry,
                                 CamelMimeMessage *message)
{
	ESource *source = NULL;
	const gchar *uid;

	/* Lookup an ESource by 'X-Evolution-Source' header. */
	uid = camel_mime_message_get_source (message);
	if (uid != NULL)
		source = e_source_registry_ref_source (registry, uid);

	/* If we found an ESource, make sure it's a mail account. */
	if (source != NULL) {
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
		if (!e_source_has_extension (source, extension_name)) {
			g_object_unref (source);
			source = NULL;
		}
	}

	return source;
}

ESource *
em_utils_guess_mail_account (ESourceRegistry *registry,
                             CamelMimeMessage *message,
                             CamelFolder *folder,
                             const gchar *message_uid)
{
	ESource *source = NULL;
	const gchar *newsgroups;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	if (folder != NULL)
		g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	/* check for newsgroup header */
	newsgroups = camel_medium_get_header (
		CAMEL_MEDIUM (message), "Newsgroups");
	if (folder != NULL && newsgroups != NULL)
		source = guess_mail_account_from_folder (registry, folder, message_uid);

	/* check for source folder */
	if (source == NULL && folder != NULL)
		source = guess_mail_account_from_folder (registry, folder, message_uid);

	/* then message source */
	if (source == NULL)
		source = guess_mail_account_from_message (registry, message);

	return source;
}

ESource *
em_utils_guess_mail_identity (ESourceRegistry *registry,
                              CamelMimeMessage *message,
                              CamelFolder *folder,
                              const gchar *message_uid)
{
	ESource *source;
	ESourceExtension *extension;
	const gchar *extension_name;
	const gchar *uid;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	if (folder != NULL)
		g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	source = em_utils_guess_mail_account (registry, message, folder, message_uid);

	if (source == NULL)
		return NULL;

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	extension = e_source_get_extension (source, extension_name);

	uid = e_source_mail_account_get_identity_uid (
		E_SOURCE_MAIL_ACCOUNT (extension));
	if (uid == NULL)
		return NULL;

	source = e_source_registry_ref_source (registry, uid);
	if (source == NULL)
		return NULL;

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	if (!e_source_has_extension (source, extension_name)) {
		g_object_unref (source);
		return NULL;
	}

	return source;
}

static gboolean
mail_account_in_recipients (ESourceRegistry *registry,
                            ESource *source,
                            GHashTable *recipients)
{
	ESourceExtension *extension;
	const gchar *extension_name;
	const gchar *uid;
	gboolean match = FALSE;
	gchar *address;

	/* Disregard disabled mail accounts. */
	if (!e_source_registry_check_enabled (registry, source))
		return FALSE;

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	extension = e_source_get_extension (source, extension_name);

	uid = e_source_mail_account_get_identity_uid (
		E_SOURCE_MAIL_ACCOUNT (extension));
	if (uid == NULL)
		return FALSE;

	source = e_source_registry_ref_source (registry, uid);
	if (source == NULL)
		return FALSE;

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	if (!e_source_has_extension (source, extension_name)) {
		g_object_unref (source);
		return FALSE;
	}

	extension = e_source_get_extension (source, extension_name);

	address = e_source_mail_identity_dup_address (
		E_SOURCE_MAIL_IDENTITY (extension));

	g_object_unref (source);

	if (address != NULL) {
		match = g_hash_table_contains (recipients, address);
		g_free (address);
	}

	return match;
}

ESource *
em_utils_guess_mail_account_with_recipients (ESourceRegistry *registry,
                                             CamelMimeMessage *message,
                                             CamelFolder *folder,
                                             const gchar *message_uid)
{
	ESource *source = NULL;
	GHashTable *recipients;
	CamelInternetAddress *addr;
	GList *list, *iter;
	const gchar *extension_name;
	const gchar *type;
	const gchar *key;

	/* This policy is subject to debate and tweaking,
	 * but please also document the rational here. */

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	/* Build a set of email addresses in which to test for membership.
	 * Only the keys matter here; the values just need to be non-NULL. */
	recipients = g_hash_table_new (g_str_hash, g_str_equal);

	type = CAMEL_RECIPIENT_TYPE_TO;
	addr = camel_mime_message_get_recipients (message, type);
	if (addr != NULL) {
		gint index = 0;

		while (camel_internet_address_get (addr, index++, NULL, &key))
			g_hash_table_add (recipients, (gpointer) key);
	}

	type = CAMEL_RECIPIENT_TYPE_CC;
	addr = camel_mime_message_get_recipients (message, type);
	if (addr != NULL) {
		gint index = 0;

		while (camel_internet_address_get (addr, index++, NULL, &key))
			g_hash_table_add (recipients, (gpointer) key);
	}

	/* First Preference: We were given a folder that maps to an
	 * enabled mail account, and that account's address appears
	 * in the list of To: or Cc: recipients. */

	if (folder != NULL)
		source = guess_mail_account_from_folder (
			registry, folder, message_uid);

	if (source == NULL)
		goto second_preference;

	if (mail_account_in_recipients (registry, source, recipients))
		goto exit;

second_preference:

	/* Second Preference: Choose any enabled mail account whose
	 * address appears in the list to To: or Cc: recipients. */

	if (source != NULL) {
		g_object_unref (source);
		source = NULL;
	}

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	list = e_source_registry_list_sources (registry, extension_name);

	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		ESource *temp = E_SOURCE (iter->data);

		if (e_source_registry_check_enabled (registry, temp) &&
		    mail_account_in_recipients (registry, temp, recipients)) {
			source = g_object_ref (temp);
			break;
		}
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	if (source != NULL)
		goto exit;

	/* Last Preference: Defer to em_utils_guess_mail_account(). */
	source = em_utils_guess_mail_account (
		registry, message, folder, message_uid);

exit:
	g_hash_table_destroy (recipients);

	return source;
}

ESource *
em_utils_guess_mail_identity_with_recipients (ESourceRegistry *registry,
                                              CamelMimeMessage *message,
                                              CamelFolder *folder,
                                              const gchar *message_uid)
{
	ESource *source;
	ESourceExtension *extension;
	const gchar *extension_name;
	const gchar *uid;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	source = em_utils_guess_mail_account_with_recipients (
		registry, message, folder, message_uid);

	if (source == NULL)
		return NULL;

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	extension = e_source_get_extension (source, extension_name);

	uid = e_source_mail_account_get_identity_uid (
		E_SOURCE_MAIL_ACCOUNT (extension));
	if (uid == NULL)
		return NULL;

	source = e_source_registry_ref_source (registry, uid);
	if (source == NULL)
		return NULL;

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	if (!e_source_has_extension (source, extension_name)) {
		g_object_unref (source);
		return NULL;
	}

	return source;
}

ESource *
em_utils_ref_mail_identity_for_store (ESourceRegistry *registry,
                                      CamelStore *store)
{
	ESourceMailAccount *extension;
	ESource *source;
	const gchar *extension_name;
	const gchar *store_uid;
	gchar *identity_uid;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);

	store_uid = camel_service_get_uid (CAMEL_SERVICE (store));
	g_return_val_if_fail (store_uid != NULL, NULL);

	source = e_source_registry_ref_source (registry, store_uid);
	g_return_val_if_fail (source != NULL, NULL);

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	extension = e_source_get_extension (source, extension_name);
	identity_uid = e_source_mail_account_dup_identity_uid (extension);

	g_object_unref (source);
	source = NULL;

	if (identity_uid != NULL) {
		source = e_source_registry_ref_source (registry, identity_uid);
		g_free (identity_uid);
	}

	return source;
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

