/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Nat Friedman <nat@novell.com>
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include <addressbook/util/eab-book-util.h>
#include <composer/e-msg-composer.h>

#include "bbdb.h"

#define d(x)

/* EBbdb extension */

typedef struct _EBbdb EBbdb;
typedef struct _EBbdbClass EBbdbClass;

struct _EBbdb {
	EExtension parent;
};

struct _EBbdbClass {
	EExtensionClass parent_class;
};

GType e_bbdb_get_type (void);

G_DEFINE_DYNAMIC_TYPE (EBbdb, e_bbdb, E_TYPE_EXTENSION)

/* How often check, in minutes. Read only on plugin enable. Use <= 0 to disable polling. */
static gint
get_check_interval (void)
{
	GSettings *settings;
	gint res = BBDB_BLIST_DEFAULT_CHECK_INTERVAL;

	settings = e_util_ref_settings (CONF_SCHEMA);
	res = g_settings_get_int (settings, CONF_KEY_GAIM_CHECK_INTERVAL);

	g_object_unref (settings);

	return res * 60;
}

static gboolean bbdb_timeout (gpointer data);

static gboolean
bbdb_timeout (gpointer data)
{
	if (bbdb_check_gaim_enabled ())
		bbdb_sync_buddy_list_check ();

	/* not a NULL for a one-time idle check, thus stop it there */
	return data == NULL;
}

typedef struct {
	gchar *name;
	gchar *email;
} todo_struct;

static void
free_todo_struct (todo_struct *td)
{
	if (td) {
		g_free (td->name);
		g_free (td->email);
		g_slice_free (todo_struct, td);
	}
}

static GQueue todo = G_QUEUE_INIT;
G_LOCK_DEFINE_STATIC (todo);

static void
todo_queue_clear (void)
{
	G_LOCK (todo);
	while (!g_queue_is_empty (&todo))
		free_todo_struct (g_queue_pop_head (&todo));
	G_UNLOCK (todo);
}

static todo_struct *
todo_queue_pop (void)
{
	todo_struct *td;

	G_LOCK (todo);
	td = g_queue_pop_head (&todo);
	G_UNLOCK (todo);

	return td;
}

static void bbdb_do_it (EBookClient *client, const gchar *name, const gchar *email);
static void add_email_to_contact (EContact *contact, const gchar *email);

static gpointer
todo_queue_process_thread (gpointer data)
{
	EBookClient *client;
	GError *error = NULL;

	client = bbdb_create_book_client (
		AUTOMATIC_CONTACTS_ADDRESSBOOK, NULL, &error);

	if (client != NULL) {
		todo_struct *td;

		while ((td = todo_queue_pop ()) != NULL) {
			bbdb_do_it (client, td->name, td->email);
			free_todo_struct (td);
		}

		g_object_unref (client);
	}

	if (error != NULL) {
		g_warning (
			"bbdb: Failed to get addressbook: %s",
			error->message);
		g_error_free (error);
		todo_queue_clear ();
	}

	return NULL;
}

static void
todo_queue_process (const gchar *name,
                    const gchar *email)
{
	todo_struct *td;

	td = g_slice_new (todo_struct);
	td->name = g_strdup (name);
	td->email = g_strdup (email);

	G_LOCK (todo);

	g_queue_push_tail (&todo, td);

	if (g_queue_get_length (&todo) == 1) {
		GThread *thread;

		thread = g_thread_new (NULL, todo_queue_process_thread, NULL);
		g_thread_unref (thread);
	}

	G_UNLOCK (todo);
}

static void
handle_destination (EDestination *destination)
{
	g_return_if_fail (destination != NULL);

	if (e_destination_is_evolution_list (destination)) {
		GList *list, *link;

		/* XXX e_destination_list_get_dests() misuses const. */
		list = (GList *) e_destination_list_get_dests (destination);

		for (link = list; link != NULL; link = g_list_next (link))
			handle_destination (E_DESTINATION (link->data));

	} else {
		gchar *tname = NULL, *temail = NULL;
		const gchar *textrep;
		EContact *contact;

		contact = e_destination_get_contact (destination);

		/* Skipping autocompleted contacts */
		if (contact != NULL)
			return;

		textrep = e_destination_get_textrep (destination, TRUE);
		if (eab_parse_qp_email (textrep, &tname, &temail)) {
			if (tname != NULL || temail != NULL)
				todo_queue_process (tname, temail);
			g_free (tname);
			g_free (temail);
		} else {
			const gchar *cname, *cemail;

			cname = e_destination_get_name (destination);
			cemail = e_destination_get_email (destination);

			if (cname != NULL || cemail != NULL)
				todo_queue_process (cname, cemail);
		}
	}
}

static gboolean
bbdb_presend_cb (EMsgComposer *composer,
                 gpointer user_data)
{
	EComposerHeaderTable *table;
	EDestination **destinations;
	GSettings *settings;
	gboolean enable;

	settings = e_util_ref_settings (CONF_SCHEMA);
	enable = g_settings_get_boolean (settings, CONF_KEY_ENABLE);
	g_object_unref (settings);

	if (!enable)
		return TRUE;

	table = e_msg_composer_get_header_table (composer);

	destinations = e_composer_header_table_get_destinations_to (table);
	if (destinations != NULL) {
		gint ii;

		for (ii = 0; destinations[ii] != NULL; ii++)
			handle_destination (destinations[ii]);
		e_destination_freev (destinations);
	}

	destinations = e_composer_header_table_get_destinations_cc (table);
	if (destinations != NULL) {
		gint ii;

		for (ii = 0; destinations[ii] != NULL; ii++)
			handle_destination (destinations[ii]);
		e_destination_freev (destinations);
	}

	return TRUE;
}

static void
bbdb_do_it (EBookClient *client,
            const gchar *name,
            const gchar *email)
{
	gchar *query_string, *temp_name = NULL;
	const gchar *delim;
	GSList *contacts = NULL;
	gboolean status;
	EContact *contact;
	GError *error = NULL;
	EShell *shell;
	ESourceRegistry *registry;
	ESource *dest_source;
	EClientCache *client_cache;
	GList *addressbooks;
	GList *aux_addressbooks;
	GSettings *settings;
	EBookClient *client_addressbook;
	ESourceAutocomplete *autocomplete_extension;
	gboolean on_autocomplete, has_autocomplete;

	g_return_if_fail (client != NULL);

	if (email == NULL || !strcmp (email, ""))
		return;

	if ((delim = strchr (email, '@')) == NULL)
		return;

	/* don't miss the entry if the mail has only e-mail id and no name */
	if (!name || !*name) {
		temp_name = g_strndup (email, delim - email);
		name = temp_name;
	}

	/* Search through all addressbooks */
	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);
	client_cache = e_shell_get_client_cache (shell);
	addressbooks = e_source_registry_list_enabled (registry, E_SOURCE_EXTENSION_ADDRESS_BOOK);
	dest_source = e_client_get_source (E_CLIENT (client));

	/* Test the destination client first */
	if (g_list_find (addressbooks, dest_source)) {
		addressbooks = g_list_remove (addressbooks, dest_source);
		g_object_unref (dest_source);
	}

	addressbooks = g_list_prepend (addressbooks, g_object_ref (dest_source));

	aux_addressbooks = addressbooks;
	while (aux_addressbooks != NULL) {

		if (g_strcmp0 (e_source_get_uid (dest_source), e_source_get_uid (aux_addressbooks->data)) == 0) {
			client_addressbook = g_object_ref (client);
		} else {
			/* Check only addressbooks with autocompletion enabled */
			has_autocomplete = e_source_has_extension (aux_addressbooks->data, E_SOURCE_EXTENSION_AUTOCOMPLETE);
			if (!has_autocomplete) {
				aux_addressbooks = aux_addressbooks->next;
				continue;
			}

			autocomplete_extension = e_source_get_extension (aux_addressbooks->data, E_SOURCE_EXTENSION_AUTOCOMPLETE);
			on_autocomplete = e_source_autocomplete_get_include_me (autocomplete_extension);
			if (!on_autocomplete) {
				aux_addressbooks = aux_addressbooks->next;
				continue;
			}

			client_addressbook = (EBookClient *) e_client_cache_get_client_sync (
					client_cache, (ESource *) aux_addressbooks->data,
					E_SOURCE_EXTENSION_ADDRESS_BOOK, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS,
					NULL, &error);

			if (error != NULL) {
				g_warning ("bbdb: Failed to get addressbook client: %s\n", error->message);
				g_clear_error (&error);
				aux_addressbooks = aux_addressbooks->next;
				continue;
			}
		}

		/* If any contacts exists with this email address, don't do anything */
		query_string = g_strdup_printf ("(contains \"email\" \"%s\")", email);
		status = e_book_client_get_contacts_sync (client_addressbook, query_string, &contacts, NULL, NULL);
		g_free (query_string);
		if (contacts != NULL || !status) {
			g_slist_free_full (contacts, g_object_unref);
			g_object_unref (client_addressbook);

			if (!status) {
				aux_addressbooks = aux_addressbooks->next;
				continue;
			}

			g_free (temp_name);
			g_list_free_full (addressbooks, g_object_unref);

			return;
		}

		g_object_unref (client_addressbook);
		aux_addressbooks = aux_addressbooks->next;
	}

	g_list_free_full (addressbooks, (GDestroyNotify) g_object_unref);

	if (g_utf8_strchr (name, -1, '\"')) {
		GString *tmp = g_string_new (name);
		gchar *p;

		while (p = g_utf8_strchr (tmp->str, tmp->len, '\"'), p)
			g_string_erase (tmp, p - tmp->str, 1);

		g_free (temp_name);
		temp_name = g_string_free (tmp, FALSE);
		name = temp_name;
	}

	/* Otherwise, create a new contact. */
	contact = e_contact_new ();
	e_contact_set (contact, E_CONTACT_FULL_NAME, (gpointer) name);

	settings = e_util_ref_settings (CONF_SCHEMA);
	if (g_settings_get_boolean (settings, CONF_KEY_FILE_UNDER_AS_FIRST_LAST)) {
		EContactName *cnt_name = e_contact_name_from_string (name);

		if (cnt_name) {
			if (cnt_name->family && *cnt_name->family &&
			    cnt_name->given && *cnt_name->given) {
				gchar *str;

				str = g_strconcat (cnt_name->given, " ", cnt_name->family, NULL);
				e_contact_set (contact, E_CONTACT_FILE_AS, str);
				g_free (str);
			}

			e_contact_name_free (cnt_name);
		}
	}
	g_clear_object (&settings);

	add_email_to_contact (contact, email);
	g_free (temp_name);

	e_book_client_add_contact_sync (client, contact, E_BOOK_OPERATION_FLAG_NONE, NULL, NULL, &error);

	if (error != NULL) {
		g_warning ("bbdb: Failed to add new contact: %s", error->message);
		g_error_free (error);
	}

	g_object_unref (contact);
}

static void
add_email_to_contact (EContact *contact,
                      const gchar *email)
{
	GList *emails;

	emails = e_contact_get (contact, E_CONTACT_EMAIL);
	emails = g_list_append (emails, g_strdup (email));

	e_contact_set (contact, E_CONTACT_EMAIL, (gpointer) emails);

	g_list_free_full (emails, g_free);
}

EBookClient *
bbdb_create_book_client (gint type,
                         GCancellable *cancellable,
                         GError **error)
{
	EShell *shell;
	ESource *source = NULL;
	ESourceRegistry *registry;
	EClientCache *client_cache;
	EClient *client = NULL;
	GSettings *settings;
	gboolean enable = TRUE;
	gchar *uid;

	settings = e_util_ref_settings (CONF_SCHEMA);

	/* Check to see if we're supposed to be running */
	if (type == AUTOMATIC_CONTACTS_ADDRESSBOOK)
		enable = g_settings_get_boolean (settings, CONF_KEY_ENABLE);
	if (!enable) {
		g_object_unref (settings);
		return NULL;
	}

	/* Open the appropriate addresbook. */
	if (type == GAIM_ADDRESSBOOK)
		uid = g_settings_get_string (
			settings, CONF_KEY_WHICH_ADDRESSBOOK_GAIM);
	else
		uid = g_settings_get_string (
			settings, CONF_KEY_WHICH_ADDRESSBOOK);
	g_object_unref (settings);

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);
	client_cache = e_shell_get_client_cache (shell);

	if (uid != NULL) {
		source = e_source_registry_ref_source (registry, uid);
		g_free (uid);
	}

	if (source == NULL)
		source = e_source_registry_ref_builtin_address_book (registry);

	client = e_client_cache_get_client_sync (
		client_cache, source,
		E_SOURCE_EXTENSION_ADDRESS_BOOK, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS,
		cancellable, error);

	g_object_unref (source);

	return (EBookClient *) client;
}

gboolean
bbdb_check_gaim_enabled (void)
{
	GSettings *settings;
	gboolean   gaim_enabled;

	settings = e_util_ref_settings (CONF_SCHEMA);
	gaim_enabled = g_settings_get_boolean (settings, CONF_KEY_ENABLE_GAIM);

	g_object_unref (settings);

	return gaim_enabled;
}

static void
e_bbdb_constructed (GObject *object)
{
	EExtension *extension = E_EXTENSION (object);
	EMsgComposer *composer;

	G_OBJECT_CLASS (e_bbdb_parent_class)->constructed (object);

	composer = E_MSG_COMPOSER (e_extension_get_extensible (extension));
	g_signal_connect (composer, "presend", G_CALLBACK (bbdb_presend_cb), NULL);
}

static void
e_bbdb_class_init (EBbdbClass *class)
{
	G_OBJECT_CLASS (class)->constructed = e_bbdb_constructed;
	E_EXTENSION_CLASS (class)->extensible_type = E_TYPE_MSG_COMPOSER;
}

static void
e_bbdb_class_finalize (EBbdbClass *class)
{
}

static void
e_bbdb_init (EBbdb *extension)
{
}

static guint bbdb_update_source = 0;

void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	gint interval;

	e_bbdb_register_type (type_module);

	g_idle_add (bbdb_timeout, GINT_TO_POINTER (1));

	interval = get_check_interval ();
	if (interval > 0)
		bbdb_update_source = e_named_timeout_add_seconds (interval, bbdb_timeout, NULL);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
	if (bbdb_update_source) {
		g_source_remove (bbdb_update_source);
		bbdb_update_source = 0;
	}
}
