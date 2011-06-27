/*
 * e-online-accounts-google.c
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

#include "e-online-accounts-google.h"

#include <config.h>
#include <glib/gi18n-lib.h>

/* XXX Just use the deprecated APIs for now.
 *     We'll be switching away soon enough. */
#undef E_CAL_DISABLE_DEPRECATED
#undef E_BOOK_DISABLE_DEPRECATED

#include <libecal/e-cal.h>
#include <libebook/e-book.h>

#include <e-util/e-account-utils.h>

/* This is the property name or URL parameter under which we
 * embed the GoaAccount ID into an EAccount or ESource object. */
#define GOA_KEY "goa-account-id"

#define GOOGLE_BASE_URI "google://"

/**
 * XXX Once the key-file based ESource API is merged, I'd
 *     like to structure the ESources something like this:
 *
 *     * Maybe add an "Enabled" key to the [Data Source] group,
 *       so we have an easy way to hide/show individual sources
 *       without destroying custom settings.  Would replace the
 *       "Enabled" key in [Mail Account], and rename the same
 *       key in ESourceSelectable to "Active".
 *
 *     +---------------------------------------------------+
 *     | [Data Source]                                     |
 *     | DisplayName: <<GoaAccount:presentation-identity>> |
 *     | Backend: google                                   |
 *     | Enabled: true  # What would 'false' mean?         |
 *     |                                                   |
 *     | [GNOME Online Accounts]                           |
 *     | Id: <<GoaAccount:id>>                             |
 *     +---------------------------------------------------+
 *         |
 *         | (child ESources)
 *         |
 *         |    +------------------------------------------+
 *         |    | [Data Source]                            |
 *         |    | DisplayName: (same as parent)            |
 *         |    | Enabled: true                            |
 *         |    |                                          |
 *         |    | [Authentication]                         |
 *         |    | Host: imap.gmail.com                     |
 *         |    | blah, blah, blah...                      |
 *         |    |                                          |
 *         +----| [Mail Account]                           |
 *         |    | blah, blah, blah...                      |
 *         |    |                                          |
 *         |    | [Mail Identity]                          |
 *         |    | Name: Matthew Barnes                     |
 *         |    | Address: <<my-gmail-address>>            |
 *         |    | blah, blah, blah...                      |
 *         |    +------------------------------------------+
 *         |
 *         |    +------------------------------------------+
 *         |    | [Data Source]                            |
 *         |    | DisplayName: GMail SMTP Server           |
 *         |    | Enabled: true                            |
 *         |    |                                          |
 *         |    | [Authentication]                         |
 *         |    | Host: smtp.gmail.com                     |
 *         +----| blah, blah, blah...                      |
 *         |    |                                          |
 *         |    | [Mail Transport]                         |
 *         |    | blah, blah, blah...                      |
 *         |    +------------------------------------------+
 *         |
 *         |    +------------------------------------------+
 *         |    | [Data Source]                            |
 *         |    | DisplayName: Contacts                    |
 *         |    | Enabled: true                            |
 *         |    |                                          |
 *         |    | [Authentication]                         |
 *         |    | blah, blah, blah...                      |
 *         +----|                                          |
 *         |    | [Address Book]                           |
 *         |    | blah, blah, blah...                      |
 *         |    +------------------------------------------+
 *         |
 *         |    +------------------------------------------+
 *         |    | [Data Source]                            |
 *         |    | DisplayName: Calendar                    |
 *         |    | Backend: caldav                          |
 *         |    | Enabled: true                            |
 *         |    |                                          |
 *         |    | [Authentication]                         |
 *         |    | blah, blah, blah...                      |
 *         +----|                                          |
 *              | [Calendar]                               |
 *              | blah, blah, blah...                      |
 *              +------------------------------------------+
 */

/* XXX Copy part of the private struct here so we can set our own UID.
 *     Since EAccountList and ESourceList forces the different aspects
 *     of the Google account to be disjoint, we can reuse the UID to
 *     link them back together. */
struct _ESourcePrivate {
	ESourceGroup *group;

	gchar *uid;
	/* ... yadda, yadda, yadda ... */
};

static void
online_accounts_google_sync_mail (GoaObject *goa_object,
                                  const gchar *evo_id)
{
	GoaMail *goa_mail;
	GoaAccount *goa_account;
	EAccountList *account_list;
	EAccount *account;
	CamelURL *url;
	const gchar *string;
	gboolean new_account = FALSE;

	/* XXX There's nothing particularly GMail-specific about this.
	 *     Maybe break this off into a more generic IMAP/SMTP sync
	 *     function and then apply any GMail-specific tweaks. */

	goa_mail = goa_object_get_mail (goa_object);
	goa_account = goa_object_get_account (goa_object);

	account_list = e_get_account_list ();
	account = e_get_account_by_uid (evo_id);

	if (account == NULL) {
		account = g_object_new (E_TYPE_ACCOUNT, NULL);
		account->uid = g_strdup (evo_id);
		account->enabled = TRUE;
		new_account = TRUE;
	}

	/*** Account Name ***/

	g_free (account->name);
	string = goa_account_get_presentation_identity (goa_account);
	account->name = g_strdup (string);

	/*** Mail Identity ***/

	if (account->id->name == NULL)
		account->id->name = g_strdup (g_get_real_name ());

	g_free (account->id->address);
	string = goa_mail_get_email_address (goa_mail);
	account->id->address = g_strdup (string);

	/*** Mail Storage ***/

	/* This quietly handles NULL strings sanely. */
	url = camel_url_new (account->source->url, NULL);

	if (url == NULL)
		url = g_new0 (CamelURL, 1);

	camel_url_set_protocol (url, "imapx");

	string = goa_account_get_identity (goa_account);
	camel_url_set_user (url, string);

	string = goa_mail_get_imap_host (goa_mail);
	camel_url_set_host (url, string);

	/* Use CamelSaslXOAuth. */
	camel_url_set_authmech (url, "XOAUTH");

	/* Always == SSL (port 993) */
	if (goa_mail_get_imap_use_tls (goa_mail))
		camel_url_set_param (url, "use_ssl", "always");
	else
		camel_url_set_param (url, "use_ssl", "never");

	string = goa_account_get_id (goa_account);
	camel_url_set_param (url, GOA_KEY, string);

	g_free (account->source->url);
	account->source->url = camel_url_to_string (url, 0);

	camel_url_free (url);

	/*** Mail Transport ***/

	/* This quietly handles NULL strings sanely. */
	url = camel_url_new (account->transport->url, NULL);

	if (url == NULL)
		url = g_new0 (CamelURL, 1);

	camel_url_set_protocol (url, "smtp");

	string = goa_account_get_identity (goa_account);
	camel_url_set_user (url, string);

	string = goa_mail_get_smtp_host (goa_mail);
	camel_url_set_host (url, string);

	/* Message Submission port */
	camel_url_set_port (url, 587);

	/* Use CamelSaslXOAuth. */
	camel_url_set_authmech (url, "XOAUTH");

	/* When-Possible == STARTTLS */
	if (goa_mail_get_smtp_use_tls (goa_mail))
		camel_url_set_param (url, "use_ssl", "when-possible");
	else
		camel_url_set_param (url, "use_ssl", "never");

	string = goa_account_get_id (goa_account);
	camel_url_set_param (url, GOA_KEY, string);

	g_free (account->transport->url);
	account->transport->url = camel_url_to_string (url, 0);

	camel_url_free (url);

	/* Clean up. */

	if (new_account) {
		e_account_list_add (account_list, account);
		g_object_unref (account);
	}

	g_object_unref (goa_account);
	g_object_unref (goa_mail);
}

static void
online_accounts_google_sync_calendar (GoaObject *goa_object,
                                      const gchar *evo_id)
{
	GoaAccount *goa_account;
	ESourceList *source_list = NULL;
	ESourceGroup *source_group;
	ECalSourceType source_type;
	ESource *source;
	const gchar *string;
	gchar *encoded;
	gchar *uri_string;
	gboolean new_source = FALSE;
	GError *error = NULL;

	source_type = E_CAL_SOURCE_TYPE_EVENT;

	if (!e_cal_get_sources (&source_list, source_type, &error)) {
		g_warn_if_fail (source_list == NULL);
		g_warn_if_fail (error != NULL);
		g_warning ("%s", error->message);
		g_error_free (error);
		return;
	}

	g_return_if_fail (E_IS_SOURCE_LIST (source_list));

	goa_account = goa_object_get_account (goa_object);

	/* This returns a new reference to the source group. */
	source_group = e_source_list_ensure_group (
		source_list, _("Google"), GOOGLE_BASE_URI, TRUE);

	source = e_source_group_peek_source_by_uid (source_group, evo_id);

	if (source == NULL) {
		source = g_object_new (E_TYPE_SOURCE, NULL);
		source->priv->uid = g_strdup (evo_id);
		e_source_set_name (source, _("Calendar"));
		new_source = TRUE;
	}

	string = goa_account_get_identity (goa_account);

	encoded = camel_url_encode (string, "@");
	uri_string = g_strdup_printf (
		"caldav://%s@www.google.com/calendar/dav/%s/events",
		encoded, string);
	e_source_set_absolute_uri (source, uri_string);
	g_free (uri_string);
	g_free (encoded);

	e_source_set_property (source, "ssl", "1");
	e_source_set_property (source, "username", string);
	e_source_set_property (source, "setup-username", string);

	/* XXX Not sure this needs to be set since the backend
	 *     will authenticate itself if it sees a GOA ID. */
	e_source_set_property (source, "auth", "1");

	string = goa_account_get_id (goa_account);
	e_source_set_property (source, GOA_KEY, string);

	if (new_source) {
		e_source_group_add_source (source_group, source, -1);
		g_object_unref (source);
	}

	g_object_unref (source_group);
	g_object_unref (goa_account);
}

static void
online_accounts_google_sync_contacts (GoaObject *goa_object,
                                      const gchar *evo_id)
{
	GoaAccount *goa_account;
	ESourceList *source_list = NULL;
	ESourceGroup *source_group;
	ESource *source;
	const gchar *string;
	gboolean new_source = FALSE;
	GError *error = NULL;

	if (!e_book_get_addressbooks (&source_list, &error)) {
		g_warn_if_fail (source_list == NULL);
		g_warn_if_fail (error != NULL);
		g_warning ("%s", error->message);
		g_error_free (error);
		return;
	}

	g_return_if_fail (E_IS_SOURCE_LIST (source_list));

	goa_account = goa_object_get_account (goa_object);

	/* This returns a new reference to the source group. */
	source_group = e_source_list_ensure_group (
		source_list, _("Google"), GOOGLE_BASE_URI, TRUE);

	source = e_source_group_peek_source_by_uid (source_group, evo_id);

	if (source == NULL) {
		source = g_object_new (E_TYPE_SOURCE, NULL);
		source->priv->uid = g_strdup (evo_id);
		e_source_set_name (source, _("Contacts"));
		new_source = TRUE;
	}

	string = goa_account_get_identity (goa_account);

	e_source_set_relative_uri (source, string);

	e_source_set_property (source, "use-ssl", "true");
	e_source_set_property (source, "username", string);

	/* XXX Not sure this needs to be set since the backend
	 *     will authenticate itself if it sees a GOA ID. */
	e_source_set_property (source, "auth", "plain/password");

	string = goa_account_get_id (goa_account);
	e_source_set_property (source, GOA_KEY, string);

	if (new_source) {
		e_source_group_add_source (source_group, source, -1);
		g_object_unref (source);
	}

	g_object_unref (source_group);
	g_object_unref (goa_account);
}

void
e_online_accounts_google_sync (GoaObject *goa_object,
                               const gchar *evo_id)
{
	GoaMail *goa_mail;
	GoaCalendar *goa_calendar;
	GoaContacts *goa_contacts;

	g_return_if_fail (GOA_IS_OBJECT (goa_object));
	g_return_if_fail (evo_id != NULL);

	/*** Google Mail ***/

	goa_mail = goa_object_get_mail (goa_object);
	if (goa_mail != NULL) {
		online_accounts_google_sync_mail (goa_object, evo_id);
		g_object_unref (goa_mail);
	} else {
		EAccountList *account_list;
		EAccount *account;

		account_list = e_get_account_list ();
		account = e_get_account_by_uid (evo_id);

		if (account != NULL)
			e_account_list_remove (account_list, account);
	}

	/*** Google Calendar ***/

	goa_calendar = goa_object_get_calendar (goa_object);
	if (goa_calendar != NULL) {
		online_accounts_google_sync_calendar (goa_object, evo_id);
		g_object_unref (goa_calendar);
	} else {
		ESourceList *source_list = NULL;
		ECalSourceType source_type;
		GError *error = NULL;

		source_type = E_CAL_SOURCE_TYPE_EVENT;
		if (e_cal_get_sources (&source_list, source_type, &error)) {
			e_source_list_remove_source_by_uid (
				source_list, evo_id);
			g_object_unref (source_list);
		} else {
			g_warn_if_fail (source_list == NULL);
			g_warn_if_fail (error != NULL);
			g_warning ("%s", error->message);
			g_error_free (error);
		}

		/* XXX Would be nice to support Google Tasks as well. */
	}

	/*** Google Contacts ***/

	goa_contacts = goa_object_get_contacts (goa_object);
	if (goa_contacts != NULL) {
		online_accounts_google_sync_contacts (goa_object, evo_id);
		g_object_unref (goa_contacts);
	} else {
		ESourceList *source_list = NULL;
		GError *error = NULL;

		if (e_book_get_addressbooks (&source_list, &error)) {
			e_source_list_remove_source_by_uid (
				source_list, evo_id);
			g_object_unref (source_list);
		} else {
			g_warn_if_fail (source_list == NULL);
			g_warn_if_fail (error != NULL);
			g_warning ("%s", error->message);
			g_error_free (error);
		}
	}
}
