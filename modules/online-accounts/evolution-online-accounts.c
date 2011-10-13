/*
 * evolution-online-accounts.c
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
 */

#include <config.h>
#include <glib/gi18n-lib.h>

/* XXX Just use the deprecated APIs for now.
 *     We'll be switching away soon enough. */
#undef E_CAL_DISABLE_DEPRECATED
#undef E_BOOK_DISABLE_DEPRECATED

#include <libecal/e-cal.h>
#include <libebook/e-book.h>
#include <libedataserver/e-uid.h>
#include <libedataserver/e-account-list.h>

#include <shell/e-shell.h>
#include <libemail-utils/e-account-utils.h>

#include "camel-sasl-xoauth.h"
#include "e-online-accounts-google.h"

/* Standard GObject macros */
#define E_TYPE_ONLINE_ACCOUNTS \
	(e_online_accounts_get_type ())
#define E_ONLINE_ACCOUNTS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ONLINE_ACCOUNTS, EOnlineAccounts))

/* This is the property name or URL parameter under which we
 * embed the GoaAccount ID into an EAccount or ESource object. */
#define GOA_KEY "goa-account-id"

typedef struct _EOnlineAccounts EOnlineAccounts;
typedef struct _EOnlineAccountsClass EOnlineAccountsClass;

typedef struct _AccountNode AccountNode;

struct _EOnlineAccounts {
	EExtension parent;

	/* GoaAccount ID -> EAccount/ESource ID */
	GHashTable *accounts;

	GoaClient *goa_client;
	EActivity *connecting;
};

struct _EOnlineAccountsClass {
	EExtensionClass parent_class;
};

struct _AccountNode {
	gchar *goa_id;		/* GoaAccount ID */
	gchar *evo_id;		/* EAccount/ESource ID */
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_online_accounts_get_type (void);

G_DEFINE_DYNAMIC_TYPE (EOnlineAccounts, e_online_accounts, E_TYPE_EXTENSION)

static EShell *
online_accounts_get_shell (EOnlineAccounts *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return E_SHELL (extensible);
}

static void
online_accounts_account_added_cb (GoaClient *goa_client,
                                  GoaObject *goa_object,
                                  EOnlineAccounts *extension)
{
	GoaAccount *goa_account;
	const gchar *provider_type;
	const gchar *goa_id;
	const gchar *evo_id;

	goa_account = goa_object_get_account (goa_object);
	provider_type = goa_account_get_provider_type (goa_account);

	goa_id = goa_account_get_id (goa_account);
	evo_id = g_hash_table_lookup (extension->accounts, goa_id);

	if (g_strcmp0 (provider_type, "google") == 0) {
		if (evo_id == NULL) {
			gchar *uid = e_uid_new ();
			g_hash_table_insert (
				extension->accounts,
				g_strdup (goa_id), uid);
			evo_id = uid;
		}

		e_online_accounts_google_sync (goa_object, evo_id);
	}

	g_object_unref (goa_account);
}

static void
online_accounts_account_changed_cb (GoaClient *goa_client,
                                    GoaObject *goa_object,
                                    EOnlineAccounts *extension)
{
	/* XXX We'll be able to handle changes more sanely once we have
	 *     key-file based ESources with proper change notifications. */
	online_accounts_account_added_cb (goa_client, goa_object, extension);
}

static void
online_accounts_account_removed_cb (GoaClient *goa_client,
                                    GoaObject *goa_object,
                                    EOnlineAccounts *extension)
{
	GoaAccount *goa_account;
	EAccountList *account_list;
	ESourceList *source_list;
	ECalSourceType type;
	EAccount *account;
	const gchar *goa_id;
	const gchar *evo_id;

	goa_account = goa_object_get_account (goa_object);
	goa_id = goa_account_get_id (goa_account);
	evo_id = g_hash_table_lookup (extension->accounts, goa_id);

	if (evo_id == NULL)
		goto exit;

	/* Remove the mail account. */

	account_list = e_get_account_list ();
	account = e_get_account_by_uid (evo_id);

	if (account != NULL)
		e_account_list_remove (account_list, account);

	/* Remove the address book. */

	if (e_book_get_addressbooks (&source_list, NULL)) {
		e_source_list_remove_source_by_uid (source_list, evo_id);
		g_object_unref (source_list);
	}

	/* Remove the calendar. */

	for (type = 0; type < E_CAL_SOURCE_TYPE_LAST; type++) {
		if (e_cal_get_sources (&source_list, type, NULL)) {
			e_source_list_remove_source_by_uid (
				source_list, evo_id);
			g_object_unref (source_list);
		}
	}

exit:
	g_object_unref (goa_account);
}

static gint
online_accounts_compare_id (GoaObject *goa_object,
                            const gchar *goa_id)
{
	GoaAccount *goa_account;
	gint result;

	goa_account = goa_object_get_account (goa_object);
	result = g_strcmp0 (goa_account_get_id (goa_account), goa_id);
	g_object_unref (goa_account);

	return result;
}

static void
online_accounts_handle_uid (EOnlineAccounts *extension,
                            const gchar *goa_id,
                            const gchar *evo_id)
{
	const gchar *match;

	/* If the GNOME Online Account ID is already registered, the
	 * corresponding Evolution ID better match what was passed in. */
	match = g_hash_table_lookup (extension->accounts, goa_id);
	g_return_if_fail (match == NULL || g_strcmp0 (match, evo_id) == 0);

	if (match == NULL)
		g_hash_table_insert (
			extension->accounts,
			g_strdup (goa_id),
			g_strdup (evo_id));
}

static void
online_accounts_search_source_list (EOnlineAccounts *extension,
                                    GList *goa_objects,
                                    ESourceList *source_list)
{
	GSList *list_a;

	list_a = e_source_list_peek_groups (source_list);

	while (list_a != NULL) {
		ESourceGroup *source_group;
		GQueue trash = G_QUEUE_INIT;
		GSList *list_b;

		source_group = E_SOURCE_GROUP (list_a->data);
		list_a = g_slist_next (list_a);

		list_b = e_source_group_peek_sources (source_group);

		while (list_b != NULL) {
			ESource *source;
			const gchar *property;
			const gchar *uid;
			GList *match;

			source = E_SOURCE (list_b->data);
			list_b = g_slist_next (list_b);

			uid = e_source_peek_uid (source);
			property = e_source_get_property (source, GOA_KEY);

			if (property == NULL)
				continue;

			/* Verify the GOA account still exists. */
			match = g_list_find_custom (
				goa_objects, property, (GCompareFunc)
				online_accounts_compare_id);

			/* If a matching GoaObject was found, add its ID
			 * to our accounts hash table.  Otherwise remove
			 * the ESource after we finish looping. */
			if (match != NULL)
				online_accounts_handle_uid (
					extension, property, uid);
			else
				g_queue_push_tail (&trash, source);
		}

		/* Empty the trash. */
		while (!g_queue_is_empty (&trash)) {
			ESource *source = g_queue_pop_head (&trash);
			e_source_group_remove_source (source_group, source);
		}
	}
}

static void
online_accounts_populate_accounts_table (EOnlineAccounts *extension,
                                         GList *goa_objects)
{
	EAccountList *account_list;
	ESourceList *source_list;
	EIterator *iterator;
	ECalSourceType type;
	GQueue trash = G_QUEUE_INIT;

	/* XXX All this messy logic will be much prettier once the new
	 *     key-file based ESource API is merged.  For now though,
	 *     we trudge through it the old and cumbersome way. */

	/* Search mail accounts. */

	account_list = e_get_account_list ();
	iterator = e_list_get_iterator (E_LIST (account_list));

	while (e_iterator_is_valid (iterator)) {
		EAccount *account;
		CamelURL *url;
		const gchar *param;

		/* XXX EIterator misuses const. */
		account = (EAccount *) e_iterator_get (iterator);
		e_iterator_next (iterator);

		if (account->source == NULL)
			continue;

		if (account->source->url == NULL)
			continue;

		url = camel_url_new (account->source->url, NULL);
		if (url == NULL)
			continue;

		param = camel_url_get_param (url, GOA_KEY);
		if (param != NULL) {
			GList *match;

			/* Verify the GOA account still exists. */
			match = g_list_find_custom (
				goa_objects, param, (GCompareFunc)
				online_accounts_compare_id);

			/* If a matching GoaObject was found, add its ID
			 * to our accounts hash table.  Otherwise remove
			 * the EAccount after we finish looping. */
			if (match != NULL)
				online_accounts_handle_uid (
					extension, param, account->uid);
			else
				g_queue_push_tail (&trash, account);
		}

		camel_url_free (url);
	}

	g_object_unref (iterator);

	/* Empty the trash. */
	while (!g_queue_is_empty (&trash)) {
		EAccount *account = g_queue_pop_head (&trash);
		e_account_list_remove (account_list, account);
	}

	/* Search address book sources. */

	if (e_book_get_addressbooks (&source_list, NULL)) {
		online_accounts_search_source_list (
			extension, goa_objects, source_list);
		g_object_unref (source_list);
	}

	/* Search calendar-related sources. */

	for (type = 0; type < E_CAL_SOURCE_TYPE_LAST; type++) {
		if (e_cal_get_sources (&source_list, type, NULL)) {
			online_accounts_search_source_list (
				extension, goa_objects, source_list);
			g_object_unref (source_list);
		}
	}
}

static void
online_accounts_connect_done (GObject *source_object,
                              GAsyncResult *result,
                              EOnlineAccounts *extension)
{
	GList *list, *link;
	GError *error = NULL;

	extension->goa_client = goa_client_new_finish (result, &error);

	/* FIXME Add an EAlert for this? */
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return;
	}

	list = goa_client_get_accounts (extension->goa_client);

	/* This populates a hash table of GOA ID -> Evo ID strings by
	 * searching through all Evolution sources for ones tagged with
	 * a GOA ID.  If a GOA ID tag is found, but no corresponding GOA
	 * account (presumably meaning the GOA account was deleted between
	 * Evo sessions), then the EAccount or ESource on which the tag was
	 * found gets deleted. */
	online_accounts_populate_accounts_table (extension, list);

	for (link = list; link != NULL; link = g_list_next (link))
		online_accounts_account_added_cb (
			extension->goa_client,
			GOA_OBJECT (link->data),
			extension);

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	/* Listen for Online Account changes. */
	g_signal_connect (
		extension->goa_client, "account-added",
		G_CALLBACK (online_accounts_account_added_cb), extension);
	g_signal_connect (
		extension->goa_client, "account-changed",
		G_CALLBACK (online_accounts_account_changed_cb), extension);
	g_signal_connect (
		extension->goa_client, "account-removed",
		G_CALLBACK (online_accounts_account_removed_cb), extension);

	/* This will allow the Evolution Setup Assistant to proceed. */
	g_object_unref (extension->connecting);
	extension->connecting = NULL;
}

static void
online_accounts_connect (EShell *shell,
                         EActivity *activity,
                         EOnlineAccounts *extension)
{
	/* This will inhibit the Evolution Setup Assistant until
	 * we've synchronized with the OnlineAccounts service. */
	extension->connecting = g_object_ref (activity);

	/* We don't really need to reference the extension in the
	 * async closure since its lifetime is bound to the EShell. */
	goa_client_new (
		NULL, (GAsyncReadyCallback)
		online_accounts_connect_done, extension);
}

static void
online_accounts_dispose (GObject *object)
{
	EOnlineAccounts *extension;

	extension = E_ONLINE_ACCOUNTS (object);

	/* This should never fail... in theory. */
	g_warn_if_fail (extension->connecting == NULL);

	if (extension->goa_client != NULL) {
		g_signal_handlers_disconnect_matched (
			extension->goa_client, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (extension->goa_client);
		extension->goa_client = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_online_accounts_parent_class)->dispose (object);
}

static void
online_accounts_finalize (GObject *object)
{
	EOnlineAccounts *extension;

	extension = E_ONLINE_ACCOUNTS (object);

	g_hash_table_destroy (extension->accounts);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_online_accounts_parent_class)->finalize (object);
}

static void
online_accounts_constructed (GObject *object)
{
	EOnlineAccounts *extension;
	EShell *shell;

	extension = E_ONLINE_ACCOUNTS (object);
	shell = online_accounts_get_shell (extension);

	/* This event is emitted from the "startup-wizard" module. */
	g_signal_connect (
		shell, "event::load-accounts",
		G_CALLBACK (online_accounts_connect), extension);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_online_accounts_parent_class)->constructed (object);
}

static void
e_online_accounts_class_init (EOnlineAccountsClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = online_accounts_dispose;
	object_class->finalize = online_accounts_finalize;
	object_class->constructed = online_accounts_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SHELL;
}

static void
e_online_accounts_class_finalize (EOnlineAccountsClass *class)
{
}

static void
e_online_accounts_init (EOnlineAccounts *extension)
{
	extension->accounts = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_online_accounts_register_type (type_module);
	camel_sasl_xoauth_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

