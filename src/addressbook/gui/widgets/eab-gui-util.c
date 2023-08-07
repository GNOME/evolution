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
 *		Chris Toshok <toshok@ximian.com>
 *		Dan Vratil <dvratil@redhat.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <locale.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "shell/e-shell.h"

#include "eab-gui-util.h"
#include "util/eab-book-util.h"
#include "eab-contact-merging.h"

/* we link to camel for decoding quoted printable email addresses */
#include <camel/camel.h>

/* Template tags for address format localization */
#define ADDRESS_REALNAME   			"%n" /* this is not used intentionally */
#define ADDRESS_REALNAME_UPPER			"%N" /* this is not used intentionally */
#define ADDRESS_COMPANY				"%m"
#define ADDRESS_COMPANY_UPPER			"%M"
#define ADDRESS_POBOX				"%p"
#define ADDRESS_STREET				"%s"
#define ADDRESS_STREET_UPPER			"%S"
#define ADDRESS_ZIPCODE				"%z"
#define ADDRESS_LOCATION			"%l"
#define ADDRESS_LOCATION_UPPER			"%L"
#define ADDRESS_REGION				"%r"
#define ADDRESS_REGION_UPPER			"%R"
#define ADDRESS_CONDCOMMA			"%,"	/* Conditional comma is removed when a surrounding tag is evaluated to zero */
#define ADDRESS_CONDWHITE			"%w"	/* Conditional whitespace is removed when a surrounding tag is evaluated to zero */
#define ADDRESS_COND_PURGEEMPTY			"%0"	/* Purge empty has following syntax: %0(...) and is removed when no tag within () is evaluated non-zero */

/* Fallback formats */
#define ADDRESS_DEFAULT_FORMAT 			"%0(%n\n)%0(%m\n)%0(%s\n)%0(PO BOX %p\n)%0(%l%w%r)%,%z"
#define ADDRESS_DEFAULT_COUNTRY_POSITION	"below"

enum {
	LOCALES_LANGUAGE = 0,
	LOCALES_COUNTRY = 1
};

typedef enum {
	ADDRESS_FORMAT_HOME = 0,
	ADDRESS_FORMAT_BUSINESS = 1
} AddressFormat;

void
eab_error_dialog (EAlertSink *alert_sink,
                  GtkWindow *parent,
                  const gchar *msg,
                  const GError *error)
{
	if (error && error->message) {
		if (alert_sink)
			e_alert_submit (
				alert_sink,
				"addressbook:generic-error",
				msg, error->message, NULL);
		else {
			if (!parent)
				parent = e_shell_get_active_window (NULL);

			e_alert_run_dialog_for_args (
				parent,
				"addressbook:generic-error",
				msg, error->message, NULL);
		}
	}
}

void
eab_load_error_dialog (GtkWidget *parent,
                       EAlertSink *alert_sink,
                       ESource *source,
                       const GError *error)
{
	ESourceBackend *extension;
	gchar *label_string, *label = NULL;
	gboolean can_detail_error = TRUE;
	const gchar *backend_name;
	const gchar *extension_name;

	g_return_if_fail (source != NULL);

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	extension = e_source_get_extension (source, extension_name);
	backend_name = e_source_backend_get_backend_name (extension);

	if (g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_OFFLINE_UNAVAILABLE)) {
		can_detail_error = FALSE;
		label_string =
			_("This address book cannot be opened. This either "
			  "means this book is not marked for offline usage "
			  "or not yet downloaded for offline usage. Please "
			  "load the address book once in online mode to "
			  "download its contents.");
	}

	else if (g_strcmp0 (backend_name, "local") == 0) {
		const gchar *user_data_dir;
		const gchar *uid;
		gchar *path;

		uid = e_source_get_uid (source);
		user_data_dir = e_get_user_data_dir ();

		path = g_build_filename (
			user_data_dir, "addressbook", uid, NULL);

		label = g_strdup_printf (
			_("This address book cannot be opened.  Please check that the "
			"path %s exists and that permissions are set to access it."), path);

		g_free (path);
		label_string = label;
	}

#ifndef HAVE_LDAP
	else if (g_strcmp0 (backend_name, "ldap") == 0) {
		/* special case for ldap: contact folders so we can tell the user about openldap */

		can_detail_error = FALSE;
		label_string =
			_("This version of Evolution does not have LDAP support "
			  "compiled in to it.  To use LDAP in Evolution "
			  "an LDAP-enabled Evolution package must be installed.");

	}
#endif
	 else {
		/* other network folders (or if ldap is enabled and server is unreachable) */
		label_string =
			_("This address book cannot be opened.  This either "
			  "means that an incorrect URI was entered, or the server "
			  "is unreachable.");
	}

	if (g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_REPOSITORY_OFFLINE)) {
		/* Do not show a detailed message; too generic. */
	} else if (can_detail_error && error != NULL) {
		label = g_strconcat (
			label_string, "\n\n",
			_("Detailed error message:"),
			" ", error->message, NULL);
		label_string = label;
	}

	if (alert_sink) {
		e_alert_submit (
			alert_sink, "addressbook:load-error",
			e_source_get_display_name (source),
			label_string, NULL);
	} else {
		GtkWidget *dialog;

		dialog = e_alert_dialog_new_for_args (
			(GtkWindow *) parent,
			"addressbook:load-error",
			e_source_get_display_name (source),
			label_string, NULL);
		g_signal_connect (
			dialog, "response",
			G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_widget_show (dialog);
	}

	g_free (label);
}

void
eab_search_result_dialog (EAlertSink *alert_sink,
                          const GError *error)
{
	gchar *str = NULL;

	if (!error)
		return;

	if (error->domain == E_CLIENT_ERROR) {
		switch (error->code) {
		case E_CLIENT_ERROR_SEARCH_SIZE_LIMIT_EXCEEDED:
			str = _("More cards matched this query than either the server is \n"
				"configured to return or Evolution is configured to display.\n"
				"Please make your search more specific or raise the result limit in\n"
				"the directory server preferences for this address book.");
			str = g_strdup (str);
			break;
		case E_CLIENT_ERROR_SEARCH_TIME_LIMIT_EXCEEDED:
			str = _("The time to execute this query exceeded the server limit or the limit\n"
				"configured for this address book.  Please make your search\n"
				"more specific or raise the time limit in the directory server\n"
				"preferences for this address book.");
			str = g_strdup (str);
			break;
		case E_CLIENT_ERROR_INVALID_QUERY:
			/* Translators: %s is replaced with a detailed error message, or an empty string, if not provided */
			str = _("The backend for this address book was unable to parse this query. %s");
			str = g_strdup_printf (str, error->message);
			break;
		case E_CLIENT_ERROR_QUERY_REFUSED:
			/* Translators: %s is replaced with a detailed error message, or an empty string, if not provided */
			str = _("The backend for this address book refused to perform this query. %s");
			str = g_strdup_printf (str, error->message);
			break;
		case E_CLIENT_ERROR_OTHER_ERROR:
		default:
			/* Translators: %s is replaced with a detailed error message, or an empty string, if not provided */
			str = _("This query did not complete successfully. %s");
			str = g_strdup_printf (str, error->message);
			break;
		}
	} else {
		/* Translators: %s is replaced with a detailed error message, or an empty string, if not provided */
		str = _("This query did not complete successfully. %s");
		str = g_strdup_printf (str, error->message);
	}

	e_alert_submit (alert_sink, "addressbook:search-error", str, NULL);

	g_free (str);
}

gint
eab_prompt_save_dialog (GtkWindow *parent)
{
	return e_alert_run_dialog_for_args (parent, "addressbook:prompt-save", NULL);
}

static gchar *
make_safe_filename (gchar *name)
{
	gchar *safe;

	if (!name) {
		/* This is a filename. Translators take note. */
		name = _("card.vcf");
	}

	if (!g_strrstr (name, ".vcf"))
		safe = g_strdup_printf ("%s%s", name, ".vcf");
	else
		safe = g_strdup (name);

	e_util_make_safe_filename (safe);

	return safe;
}

static void
source_selection_changed_cb (ESourceSelector *selector,
                             GtkWidget *ok_button)
{
	ESource *except_source = NULL, *selected;
	gboolean sensitive;

	except_source = g_object_get_data (G_OBJECT (ok_button), "except-source");
	selected = e_source_selector_ref_primary_selection (selector);

	sensitive = (selected != NULL && selected != except_source);
	gtk_widget_set_sensitive (ok_button, sensitive);

	if (selected != NULL)
		g_object_unref (selected);
}

ESource *
eab_select_source (ESourceRegistry *registry,
                   ESource *except_source,
                   const gchar *title,
                   const gchar *message,
                   const gchar *select_uid,
                   GtkWindow *parent)
{
	ESource *source;
	GtkWidget *content_area;
	GtkWidget *dialog;
	GtkWidget *ok_button;
	/* GtkWidget *label; */
	GtkWidget *selector;
	GtkWidget *scrolled_window;
	const gchar *extension_name;
	gint response;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	dialog = gtk_dialog_new_with_buttons (
		_("Select Address Book"), parent,
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_ACCEPT,
		NULL);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 350, 300);

	/* label = gtk_label_new (message); */

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	selector = e_source_selector_new (registry, extension_name);
	e_source_selector_set_show_toggles (
		E_SOURCE_SELECTOR (selector), FALSE);

	ok_button = gtk_dialog_get_widget_for_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	if (except_source)
		g_object_set_data (
			G_OBJECT (ok_button), "except-source", except_source);

	g_signal_connect (
		selector, "primary_selection_changed",
		G_CALLBACK (source_selection_changed_cb), ok_button);

	if (select_uid) {
		source = e_source_registry_ref_source (registry, select_uid);
		if (source != NULL) {
			e_source_selector_set_primary_selection (
				E_SOURCE_SELECTOR (selector), source);
			g_object_unref (source);
		}
	}

	source_selection_changed_cb (E_SOURCE_SELECTOR (selector), ok_button);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (scrolled_window), selector);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (GTK_BOX (content_area), scrolled_window, TRUE, TRUE, 4);

	gtk_widget_show_all (dialog);
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_ACCEPT)
		source = e_source_selector_ref_primary_selection (
			E_SOURCE_SELECTOR (selector));
	else
		source = NULL;

	gtk_widget_destroy (dialog);

	/* XXX Return a borrowed reference for backward-compatibility. */
	if (source != NULL)
		g_object_unref (source);

	return source;
}

gchar *
eab_suggest_filename (EContact *contact)
{
	gchar *res = NULL;

	if (contact) {
		gchar *string;

		string = e_contact_get (contact, E_CONTACT_FILE_AS);
		if (string == NULL)
			string = e_contact_get (contact, E_CONTACT_FULL_NAME);
		if (string != NULL)
			res = make_safe_filename (string);
		g_free (string);
	}

	if (res == NULL)
		res = make_safe_filename (_("list"));

	return res;
}

typedef struct ContactCopyProcess_ ContactCopyProcess;

struct ContactCopyProcess_ {
	gint count;
	gboolean book_status;
	GSList *contacts;
	EBookClient *source;
	EBookClient *destination;
	ESourceRegistry *registry;
	gboolean delete_from_source;
	EAlertSink *alert_sink;
};

static void process_unref (ContactCopyProcess *process);

static void
remove_contact_ready_cb (GObject *source_object,
                         GAsyncResult *result,
                         gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	ContactCopyProcess *process = user_data;
	GError *error = NULL;

	e_book_client_remove_contact_by_uid_finish (book_client, result, &error);

	if (error != NULL) {
		g_warning (
			"%s: Remove contact by uid failed: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	}

	process_unref (process);
}

static void
do_delete_from_source (gpointer data,
                       gpointer user_data)
{
	ContactCopyProcess *process = user_data;
	EContact *contact = data;
	const gchar *id;
	EBookClient *book_client = process->source;

	id = e_contact_get_const (contact, E_CONTACT_UID);
	g_return_if_fail (id != NULL);
	g_return_if_fail (book_client != NULL);

	process->count++;
	e_book_client_remove_contact_by_uid (book_client, id, E_BOOK_OPERATION_FLAG_NONE, NULL, remove_contact_ready_cb, process);
}

static void
delete_contacts (ContactCopyProcess *process)
{
	if (process->book_status == TRUE) {
		g_slist_foreach (process->contacts,
				do_delete_from_source,
				process);
	}
}

static void
process_unref (ContactCopyProcess *process)
{
	process->count--;
	if (process->count == 0) {
		if (process->delete_from_source) {
			delete_contacts (process);
			/* to not repeate this again */
			process->delete_from_source = FALSE;

			if (process->count > 0)
				return;
		}
		g_slist_free_full (
			process->contacts,
			(GDestroyNotify) g_object_unref);
		g_object_unref (process->source);
		g_object_unref (process->destination);
		g_object_unref (process->registry);
		g_slice_free (ContactCopyProcess, process);
	}
}

static void
contact_added_cb (EBookClient *book_client,
                  const GError *error,
                  const gchar *id,
                  gpointer user_data)
{
	ContactCopyProcess *process = user_data;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		process->book_status = FALSE;
	} else if (error != NULL) {
		process->book_status = FALSE;
		eab_error_dialog (
			process->alert_sink, NULL,
			_("Error adding contact"), error);
	} else {
		/* success */
		process->book_status = TRUE;
	}

	process_unref (process);
}

static void
do_copy (gpointer data,
         gpointer user_data)
{
	EBookClient *book_client;
	EContact *contact;
	ContactCopyProcess *process;

	process = user_data;
	contact = data;

	book_client = process->destination;
	e_contact_inline_local_photos (contact, NULL);

	process->count++;
	eab_merging_book_add_contact (
		process->registry, book_client,
		contact, contact_added_cb, process, TRUE);
}

static void
book_client_connect_cb (GObject *source_object,
                        GAsyncResult *result,
                        gpointer user_data)
{
	ContactCopyProcess *process = user_data;
	EClient *client;
	GError *error = NULL;

	client = e_book_client_connect_finish (result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		goto exit;
	}

	process->destination = E_BOOK_CLIENT (client);
	process->book_status = TRUE;
	g_slist_foreach (process->contacts, do_copy, process);

exit:
	process_unref (process);
}

void
eab_transfer_contacts (ESourceRegistry *registry,
                       EBookClient *source_client,
                       GSList *contacts /* adopted */,
                       gboolean delete_from_source,
                       EAlertSink *alert_sink)
{
	ESource *source;
	ESource *destination;
	static gchar *last_uid = NULL;
	ContactCopyProcess *process;
	gchar *desc;
	GtkWindow *window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (alert_sink)));

	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (E_IS_BOOK_CLIENT (source_client));

	if (contacts == NULL)
		return;

	if (last_uid == NULL)
		last_uid = g_strdup ("");

	if (contacts->next == NULL) {
		if (delete_from_source)
			desc = _("Move contact to");
		else
			desc = _("Copy contact to");
	} else {
		if (delete_from_source)
			desc = _("Move contacts to");
		else
			desc = _("Copy contacts to");
	}

	source = e_client_get_source (E_CLIENT (source_client));

	destination = eab_select_source (
		registry, source, desc, NULL, last_uid, window);

	if (!destination) {
		g_slist_free_full (contacts, g_object_unref);
		return;
	}

	if (strcmp (last_uid, e_source_get_uid (destination)) != 0) {
		g_free (last_uid);
		last_uid = g_strdup (e_source_get_uid (destination));
	}

	process = g_slice_new0 (ContactCopyProcess);
	process->count = 1;
	process->book_status = FALSE;
	process->source = g_object_ref (source_client);
	process->contacts = contacts;
	process->destination = NULL;
	process->registry = g_object_ref (registry);
	process->alert_sink = alert_sink;
	process->delete_from_source = delete_from_source;

	e_book_client_connect (destination, (guint32) -1, NULL, book_client_connect_cb, process);
}

/*
 * eab_format_address helper function
 *
 * Splits locales from en_US to array "en","us",NULL. When
 * locales don't have the second part (for example "C"),
 * the output array is "c",NULL
 */
static gchar **
get_locales (void)
{
	gchar *locale, *l_locale;
	gchar *dot;
	gchar **split;

#ifdef LC_ADDRESS
	locale = g_strdup (setlocale (LC_ADDRESS, NULL));
#else
	locale = NULL;
#endif
	if (!locale)
		return NULL;

	l_locale = g_utf8_strdown (locale, -1);
	g_free (locale);

	dot = strchr (l_locale, '.');
	if (dot != NULL) {
		gchar *p = l_locale;
		l_locale = g_strndup (l_locale, dot - l_locale);
		g_free (p);
	}

	split = g_strsplit (l_locale, "_", 2);

	g_free (l_locale);
	return split;

}

static gchar *
get_locales_str (void)
{
	gchar *ret = NULL;
	gchar **loc = get_locales ();

	if (!loc)
		return g_strdup ("C");

	if (!loc[0] ||
	    (loc[0] && !loc[1])) /* We don't care about language now, we need a country at first! */
		ret = g_strdup ("C");
	else if (loc[0] && loc[1]) {
		if (*loc[0])
			ret = g_strconcat (loc[LOCALES_COUNTRY], "_", loc[LOCALES_LANGUAGE], NULL);
		else
			ret = g_strdup (loc[LOCALES_COUNTRY]);
	}

	g_strfreev (loc);
	return ret;
}

/*
 * Reads countrytransl.map file, which contains map of localized
 * country names and their ISO codes and tries to find matching record
 * for given country. The search is case insensitive.
 * When no record is found (country is probably in untranslated language), returns
 * code of local computer country (from locales)
 */
static gchar *
country_to_ISO (const gchar *country)
{
	FILE *file = fopen (EVOLUTION_PRIVDATADIR "/countrytransl.map", "r");
	gchar buffer[100];
	gint length = 100;
	gchar **pair;
	gchar *res;
	gchar *l_country = g_utf8_strdown (country, -1);

	if (!file) {
		gchar **loc;
		g_warning ("%s: Failed to open countrytransl.map. Check your installation.", G_STRFUNC);
		loc = get_locales ();
		res = g_strdup (loc ? loc[LOCALES_COUNTRY] : NULL);
		g_free (l_country);
		g_strfreev (loc);
		return res;
	}

	while (fgets (buffer, length, file) != NULL) {
		gchar *low = NULL;
		pair = g_strsplit (buffer, "\t", 2);

		if (pair[0]) {
			low = g_utf8_strdown (pair[0], -1);
			if (g_utf8_collate (low, l_country) == 0) {
				gchar *ret = g_strdup (pair[1]);
				gchar *pos;
				/* Remove trailing newline character */
				if ((pos = g_strrstr (ret, "\n")) != NULL)
					pos[0] = '\0';
				fclose (file);
				g_strfreev (pair);
				g_free (low);
				g_free (l_country);
				return ret;
			}
		}

		g_strfreev (pair);
		g_free (low);
	}

	/* If we get here, then no match was found in the map file and we
	 * fallback to local system locales */
	fclose (file);

	pair = get_locales ();
	res = g_strdup (pair ? pair[LOCALES_COUNTRY] : NULL);
	g_strfreev (pair);
	g_free (l_country);
	return res;
}

/*
 * Tries to find given key in "country_LANGUAGE" group. When fails to find
 * such group, then fallbacks to "country" group. When such group does not
 * exist either, NULL is returned
 */
static gchar *
get_key_file_locale_string (GKeyFile *key_file,
                            const gchar *key,
                            const gchar *locale)
{
	gchar *result;
	gchar *group;

	g_return_val_if_fail (locale, NULL);

	/* Default locale is in "country_lang", but such group may not exist. In such case use group "country" */
	if (g_key_file_has_group (key_file, locale))
		group = g_strdup (locale);
	else {
		gchar **locales = g_strsplit (locale, "_", 0);
		group = g_strdup (locales[LOCALES_COUNTRY]);
		g_strfreev (locales);
	}

	/* When group or key does not exist, returns NULL and fallback string will be used */
	result = g_key_file_get_string (key_file, group, key, NULL);
	g_free (group);
	return result;
}

static void
get_address_format (AddressFormat address_format,
                    const gchar *locale,
                    gchar **format,
                    gchar **country_position)
{
	GKeyFile *key_file;
	GError *error;
	gchar *loc;
	const gchar *addr_key, *country_key;

	if (address_format == ADDRESS_FORMAT_HOME) {
		addr_key = "AddressFormat";
		country_key = "CountryPosition";
	} else if (address_format == ADDRESS_FORMAT_BUSINESS) {
		addr_key = "BusinessAddressFormat";
		country_key = "BusinessCountryPosition";
	} else {
		return;
	}

	if (locale == NULL)
		loc = get_locales_str ();
	else
		loc = g_strdup (locale);

	error = NULL;
	key_file = g_key_file_new ();
	g_key_file_load_from_file (key_file, EVOLUTION_PRIVDATADIR "/address_formats.dat", 0, &error);
	if (error != NULL) {
		g_warning ("%s: Failed to load address_formats.dat file: %s", G_STRFUNC, error->message);
		if (format)
			*format = g_strdup (ADDRESS_DEFAULT_FORMAT);
		if (country_position)
			*country_position = g_strdup (ADDRESS_DEFAULT_COUNTRY_POSITION);
		g_key_file_free (key_file);
		g_free (loc);
		g_error_free (error);
		return;
	}

	if (format) {
		g_free (*format);
		*format = get_key_file_locale_string (key_file, addr_key, loc);
		if (!*format && address_format == ADDRESS_FORMAT_HOME) {
			*format = g_strdup (ADDRESS_DEFAULT_FORMAT);
		} else if (!*format && address_format == ADDRESS_FORMAT_BUSINESS)
			get_address_format (ADDRESS_FORMAT_HOME, loc, format, NULL);
	}

	if (country_position) {
		g_free (*country_position);
		*country_position = get_key_file_locale_string (key_file, country_key, loc);
		if (!*country_position && address_format == ADDRESS_FORMAT_HOME)
			*country_position = g_strdup (ADDRESS_DEFAULT_COUNTRY_POSITION);
		else if (!*country_position && address_format == ADDRESS_FORMAT_BUSINESS)
			get_address_format (ADDRESS_FORMAT_HOME, loc, NULL, country_position);
	}

	g_free (loc);
	g_key_file_free (key_file);
}

static const gchar *
find_balanced_bracket (const gchar *str)
{
	gint balance_counter = 0;
	gint i = 0;

	do {
		if (str[i] == '(')
			balance_counter++;

		if (str[i] == ')')
			balance_counter--;

		i++;

	} while ((balance_counter > 0) && (str[i]));

	if (balance_counter > 0)
		return str;

	return str + i;
}

static GString *
string_append_upper (GString *str,
                     const gchar *c)
{
	gchar *up_c;

	g_return_val_if_fail (str, NULL);

	if (!c || !*c)
		return str;

	up_c = g_utf8_strup (c, -1);
	g_string_append (str, up_c);
	g_free (up_c);

	return str;
}

static gboolean
parse_address_template_section (const gchar *format,
                                const gchar *realname,
                                const gchar *org_name,
                                EContactAddress *address,
                                gchar **result)

{
	const gchar *pos, *old_pos;
	gboolean ret = FALSE; /* Indicates, wheter at least something was replaced */

	GString *res = g_string_new ("");

	pos = format;
	old_pos = pos;
	while ((pos = strchr (pos, '%')) != NULL) {

		if (old_pos != pos)
			g_string_append_len (res, old_pos, pos - old_pos);

		switch (pos[1]) {
			case 'n':
				if (realname && *realname) {
					g_string_append (res, realname);
					ret = TRUE;
				}
				pos += 2; /* Jump behind the modifier, see what's next */
				break;
			case 'N':
				if (realname && *realname) {
					string_append_upper (res, realname);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'm':
				if (org_name && *org_name) {
					g_string_append (res, org_name);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'M':
				if (org_name && *org_name) {
					string_append_upper (res, org_name);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'p':
				if (address->po && *(address->po)) {
					g_string_append (res, address->po);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 's':
				if (address->street && *(address->street)) {
					g_string_append (res, address->street);
					if (address->ext && *(address->ext))
						g_string_append_printf (
							res, "\n%s",
							address->ext);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'S':
				if (address->street && *(address->street)) {
					string_append_upper (res, address->street);
					if (address->ext && *(address->ext)) {
						g_string_append_c (res, '\n');
						string_append_upper (res, address->ext);
					}
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'z':
				if (address->code && *(address->code)) {
					g_string_append (res, address->code);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'l':
				if (address->locality && *(address->locality)) {
					g_string_append (res, address->locality);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'L':
				if (address->locality && *(address->locality)) {
					string_append_upper (res, address->locality);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'r':
				if (address->region && *(address->region)) {
					g_string_append (res, address->region);
					ret = TRUE;
				}
				pos += 2;
				break;
			case 'R':
				if (address->region && *(address->region)) {
					string_append_upper (res, address->region);
					ret = TRUE;
				}
				pos += 2;
				break;
			case ',':
				if (ret && (pos >= format + 2) &&		/* If there's something before %, */
				    (g_ascii_strcasecmp (pos - 2, "\n") != 0) &&  /* And if it is not a newline */
				    (g_ascii_strcasecmp (pos - 2, "%w") != 0))    /* Nor whitespace */
					g_string_append (res, ", ");
				pos += 2;
				break;
			case 'w':
				if (ret && (pos >= format + 2) &&
				    (g_ascii_strcasecmp (pos - 2, "\n") != 0) &&
				    (g_ascii_strcasecmp (pos - 1, " ") != 0))
					g_string_append_c (res, ' ');
				pos += 2;
				break;
			case '0': {
				const gchar *bpos1, *bpos2;
				gchar *inner;
				gchar *ires;
				gboolean replaced;

				bpos1 = pos + 2;
				bpos2 = find_balanced_bracket (bpos1);

				inner = g_strndup (bpos1 + 1, bpos2 - bpos1 - 2); /* Get inner content of the %0 (...) */
				replaced = parse_address_template_section (inner, realname, org_name, address, &ires);
				if (replaced)
					g_string_append (res, ires);

				g_free (ires);
				g_free (inner);

				ret = replaced;
				pos += (bpos2 - bpos1 + 2);
			} break;
		}

		old_pos = pos;
	}
	g_string_append (res, old_pos);

	*result = g_string_free (res, FALSE);

	return ret;
}

gchar *
eab_format_address (EContact *contact,
                    EContactField address_type)
{
	gchar *result;
	gchar *format = NULL;
	gchar *country_position = NULL;
	gchar *locale;
	EContactAddress *addr = e_contact_get (contact, address_type);

	if (!addr)
		return NULL;

	if (!addr->po && !addr->ext && !addr->street && !addr->locality && !addr->region &&
	    !addr->code && !addr->country) {
		e_contact_address_free (addr);
		return NULL;
	}

	if (addr->country) {
		gchar *cntry = country_to_ISO (addr->country);
		gchar **loc = get_locales ();
		locale = g_strconcat (loc ? loc[LOCALES_LANGUAGE] : "C", "_", cntry, NULL);
		g_strfreev (loc);
		g_free (cntry);
	} else
		locale = get_locales_str ();

	if (address_type == E_CONTACT_ADDRESS_HOME)
		get_address_format (ADDRESS_FORMAT_HOME, locale, &format, &country_position);
	else if (address_type == E_CONTACT_ADDRESS_WORK)
		get_address_format (ADDRESS_FORMAT_BUSINESS, locale, &format, &country_position);
	else {
		e_contact_address_free (addr);
		g_free (locale);
		return NULL;
	}

	/* Expand all the variables in format.
	 * Don't display organization in home address;
	 * and skip full names, as it's part of the EContact itself,
	 * check this bug for reason: https://bugzilla.gnome.org/show_bug.cgi?id=667912
	 */
	parse_address_template_section (
		format,
		NULL,
		(address_type == E_CONTACT_ADDRESS_WORK) ?
			e_contact_get_const (contact, E_CONTACT_ORG) : NULL,
		addr,
		&result);

	/* Add the country line. In some countries, the address can be located above the
	 * rest of the address */
	if (addr->country && country_position) {
		gchar *country_upper = g_utf8_strup (addr->country, -1);
		gchar *p = result;
		if (g_strcmp0 (country_position, "BELOW") == 0) {
			result = g_strconcat (p, "\n\n", country_upper, NULL);
			g_free (p);
		} else if (g_strcmp0 (country_position, "below") == 0) {
			result = g_strconcat (p, "\n\n", addr->country, NULL);
			g_free (p);
		} else if (g_strcmp0 (country_position, "ABOVE") == 0) {
			result = g_strconcat (country_upper, "\n\n", p, NULL);
			g_free (p);
		} else if (g_strcmp0 (country_position, "above") == 0) {
			result = g_strconcat (addr->country, "\n\n", p, NULL);
			g_free (p);
		}
		g_free (country_upper);
	}

	e_contact_address_free (addr);
	g_free (locale);
	g_free (format);
	g_free (country_position);

	return result;
}

gboolean
eab_fullname_matches_nickname (EContact *contact)
{
	gchar *nickname, *fullname;
	gboolean same;

	g_return_val_if_fail (E_IS_CONTACT (contact), FALSE);

	nickname = e_contact_get (contact, E_CONTACT_NICKNAME);
	fullname = e_contact_get (contact, E_CONTACT_FULL_NAME);
	same = g_strcmp0 (nickname && *nickname ? nickname : NULL,
			  fullname && *fullname ? fullname : NULL) == 0;

	g_free (nickname);
	g_free (fullname);

	return same;
}
