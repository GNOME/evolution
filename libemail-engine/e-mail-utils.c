/*
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

static gboolean
e_mail_utils_folder_uri_is_drafts (ESourceRegistry *registry,
				   CamelSession *session,
				   const gchar *folder_uri)
{
	GList *sources, *link;
	gboolean is_drafts = FALSE;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);
	g_return_val_if_fail (CAMEL_IS_SESSION (session), FALSE);
	g_return_val_if_fail (folder_uri != NULL, FALSE);

	sources = e_source_registry_list_sources (registry, E_SOURCE_EXTENSION_MAIL_COMPOSITION);

	for (link = sources; link; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceMailComposition *extension;
		const gchar *drafts_folder_uri;

		extension = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION);

		drafts_folder_uri = e_source_mail_composition_get_drafts_folder (extension);

		if (drafts_folder_uri != NULL)
			is_drafts = e_mail_folder_uri_equal (session, folder_uri, drafts_folder_uri);

		if (is_drafts)
			break;
	}

	g_list_free_full (sources, g_object_unref);

	return is_drafts;
}

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
	gchar *folder_uri;
	gboolean is_drafts = FALSE;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	store = camel_folder_get_parent_store (folder);
	session = camel_service_ref_session (CAMEL_SERVICE (store));

	local_drafts_folder =
		e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_DRAFTS);

	if (folder == local_drafts_folder) {
		is_drafts = TRUE;
		goto exit;
	}

	folder_uri = e_mail_folder_uri_from_folder (folder);

	is_drafts = e_mail_utils_folder_uri_is_drafts (registry, session, folder_uri);

	g_free (folder_uri);

exit:
	g_object_unref (session);

	return is_drafts;
}

/**
 * em_utils_folder_name_is_drafts:
 * @registry: an #ESourceRegistry
 * @store: a #CamelStore
 * @folder_name: a folder name
 *
 * Decides if @folder_name of the @store is a Drafts folder.
 *
 * Returns %TRUE if this is a Drafts folder or %FALSE otherwise.
 *
 * Since: 3.22.5
 **/
gboolean
em_utils_folder_name_is_drafts (ESourceRegistry *registry,
				CamelStore *store,
				const gchar *folder_name)
{
	CamelSession *session;
	CamelFolder *local_drafts_folder;
	gchar *folder_uri, *local_drafts_uri;
	gboolean is_drafts;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);
	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	g_return_val_if_fail (folder_name != NULL, FALSE);

	folder_uri = e_mail_folder_uri_build (store, folder_name);
	g_return_val_if_fail (folder_uri != NULL, FALSE);

	session = camel_service_ref_session (CAMEL_SERVICE (store));

	local_drafts_folder =
		e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_DRAFTS);

	local_drafts_uri = e_mail_folder_uri_from_folder (local_drafts_folder);

	is_drafts = g_strcmp0 (local_drafts_uri, folder_uri) == 0 ||
		e_mail_utils_folder_uri_is_drafts (registry, session, folder_uri);

	g_clear_object (&session);
	g_free (local_drafts_uri);
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
	session = camel_service_ref_session (CAMEL_SERVICE (store));

	local_templates_folder =
		e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_TEMPLATES);

	if (folder == local_templates_folder) {
		is_templates = TRUE;
		goto exit;
	}

	folder_uri = e_mail_folder_uri_from_folder (folder);

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

exit:
	g_object_unref (session);

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
	session = camel_service_ref_session (CAMEL_SERVICE (store));

	local_sent_folder =
		e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_SENT);

	if (folder == local_sent_folder) {
		is_sent = TRUE;
		goto exit;
	}

	folder_uri = e_mail_folder_uri_from_folder (folder);

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

exit:
	g_object_unref (session);

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
	gboolean is_outbox;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	store = camel_folder_get_parent_store (folder);
	session = camel_service_ref_session (CAMEL_SERVICE (store));

	local_outbox_folder =
		e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_OUTBOX);

	is_outbox = (folder == local_outbox_folder);

	g_object_unref (session);

	return is_outbox;
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
			camel_message_info_unref (mi);
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
em_utils_guess_mail_account_with_recipients_and_sort (ESourceRegistry *registry,
                                                      CamelMimeMessage *message,
                                                      CamelFolder *folder,
                                                      const gchar *message_uid,
                                                      EMailUtilsSortSourcesFunc sort_func,
                                                      gpointer sort_func_data)
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
	list = e_source_registry_list_enabled (registry, extension_name);

	if (sort_func)
		sort_func (&list, sort_func_data);

	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		ESource *temp = E_SOURCE (iter->data);

		if (mail_account_in_recipients (registry, temp, recipients)) {
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
em_utils_guess_mail_identity_with_recipients_and_sort (ESourceRegistry *registry,
                                                       CamelMimeMessage *message,
                                                       CamelFolder *folder,
                                                       const gchar *message_uid,
                                                       EMailUtilsSortSourcesFunc sort_func,
                                                       gpointer sort_func_data)
{
	ESource *source;
	ESourceExtension *extension;
	const gchar *extension_name;
	const gchar *uid;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	source = em_utils_guess_mail_account_with_recipients_and_sort (
		registry, message, folder, message_uid, sort_func, sort_func_data);

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
em_utils_guess_mail_account_with_recipients (ESourceRegistry *registry,
                                             CamelMimeMessage *message,
                                             CamelFolder *folder,
                                             const gchar *message_uid)
{
	return em_utils_guess_mail_account_with_recipients_and_sort (registry, message, folder, message_uid, NULL, NULL);
}

ESource *
em_utils_guess_mail_identity_with_recipients (ESourceRegistry *registry,
                                              CamelMimeMessage *message,
                                              CamelFolder *folder,
                                              const gchar *message_uid)
{
	return em_utils_guess_mail_identity_with_recipients_and_sort (registry, message, folder, message_uid, NULL, NULL);
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
 * em_utils_is_local_delivery_mbox_file:
 * @service: a #CamelService
 *
 * Returns whether @service refers to a local mbox file where new mail
 * is delivered by some external software.
 *
 * Specifically that means @service's #CamelProvider protocol is "mbox"
 * and its #CamelLocalSettings:path setting points to an existing file,
 * not a directory.
 *
 * Returns: whether @service is for local mbox delivery
 **/
gboolean
em_utils_is_local_delivery_mbox_file (CamelService *service)
{
	CamelProvider *provider;
	CamelSettings *settings;
	gchar *mbox_path = NULL;
	gboolean is_local_delivery_mbox_file;

	g_return_val_if_fail (CAMEL_IS_SERVICE (service), FALSE);

	provider = camel_service_get_provider (service);
	g_return_val_if_fail (provider != NULL, FALSE);
	g_return_val_if_fail (provider->protocol != NULL, FALSE);

	if (!g_str_equal (provider->protocol, "mbox"))
		return FALSE;

	settings = camel_service_ref_settings (service);

	if (CAMEL_IS_LOCAL_SETTINGS (settings)) {
		CamelLocalSettings *local_settings;

		local_settings = CAMEL_LOCAL_SETTINGS (settings);
		mbox_path = camel_local_settings_dup_path (local_settings);
	}

	is_local_delivery_mbox_file =
		(mbox_path != NULL) &&
		g_file_test (mbox_path, G_FILE_TEST_EXISTS) &&
		!g_file_test (mbox_path, G_FILE_TEST_IS_DIR);

	g_free (mbox_path);
	g_clear_object (&settings);

	return is_local_delivery_mbox_file;
}

/* Expands groups to individual addresses, or removes empty groups completely.
 * Usual email addresses are left untouched.
*/
void
em_utils_expand_groups (CamelInternetAddress *addresses)
{
	gint ii, len;
	const gchar *addr;
	CamelAddress *addrs;

	g_return_if_fail (CAMEL_IS_INTERNET_ADDRESS (addresses));

	addrs = CAMEL_ADDRESS (addresses);
	len = camel_address_length (addrs);
	for (ii = len - 1; ii >= 0; ii--) {
		addr = NULL;

		if (!camel_internet_address_get (addresses, ii, NULL, &addr)) {
			camel_address_remove (addrs, ii);
		} else if (addr) {
			gchar *encoded = camel_internet_address_encode_address (NULL, NULL, addr);

			if (encoded) {
				CamelInternetAddress *iaddr = camel_internet_address_new ();
				gint decoded;

				/* decode expands respective groups */
				decoded = camel_address_decode (CAMEL_ADDRESS (iaddr), encoded);
				if (decoded <= 0 || decoded > 1) {
					camel_address_remove (addrs, ii);

					if (decoded > 1)
						camel_address_cat (addrs, CAMEL_ADDRESS (iaddr));
				}

				g_object_unref (iaddr);
				g_free (encoded);
			}
		}
	}
}

void
em_utils_get_real_folder_and_message_uid (CamelFolder *folder,
					  const gchar *uid,
					  CamelFolder **out_real_folder,
					  gchar **folder_uri,
					  gchar **message_uid)
{
	g_return_if_fail (folder != NULL);
	g_return_if_fail (uid != NULL);

	if (out_real_folder)
		*out_real_folder = NULL;

	if (CAMEL_IS_VEE_FOLDER (folder)) {
		CamelMessageInfo *mi;

		mi = camel_folder_get_message_info (folder, uid);
		if (mi) {
			CamelFolder *real_folder;
			gchar *real_uid = NULL;

			real_folder = camel_vee_folder_get_location (
				CAMEL_VEE_FOLDER (folder),
				(CamelVeeMessageInfo *) mi,
				&real_uid);

			if (real_folder) {
				if (folder_uri)
					*folder_uri = e_mail_folder_uri_from_folder (real_folder);
				if (message_uid)
					*message_uid = real_uid;
				else
					g_free (real_uid);

				camel_message_info_unref (mi);

				if (out_real_folder)
					*out_real_folder = g_object_ref (real_folder);

				return;
			}

			camel_message_info_unref (mi);
		}
	}

	if (folder_uri)
		*folder_uri = e_mail_folder_uri_from_folder (folder);
	if (message_uid)
		*message_uid = g_strdup (uid);
}
