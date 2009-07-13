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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libebook/e-book.h>
#include <libedataserver/e-url.h>
#include <libedataserverui/e-passwords.h>

#include "e-util/e-error.h"
#include "addressbook.h"

#define d(x)

static void addressbook_authenticate (EBook *book, gboolean previous_failure,
				      ESource *source, EBookCallback cb, gpointer closure);
static void auth_required_cb (EBook *book, gpointer data);

typedef struct {
	EBookCallback cb;
	ESource *source;
	gpointer closure;
	guint cancelled : 1;
} LoadSourceData;

static void
free_load_source_data (LoadSourceData *data)
{
	if (data->source)
		g_object_unref (data->source);
	g_free (data);
}

/*this function removes of anything present after semicolon
in uri*/

static gchar *
remove_parameters_from_uri (const gchar *uri)
{
	gchar *euri_str;
	EUri *euri;

	euri = e_uri_new (uri);
	euri_str = e_uri_to_string (euri, FALSE);
	e_uri_free (euri);
	return euri_str;
}

static void
load_source_auth_cb (EBook *book, EBookStatus status, gpointer closure)
{
	LoadSourceData *data = closure;
	gboolean was_in = g_object_get_data (G_OBJECT (book), "authenticated") != NULL;

	g_object_set_data (G_OBJECT (book), "authenticated", NULL);

	if (data->cancelled) {
		free_load_source_data (data);
		return;
	}

	if (status != E_BOOK_ERROR_OK) {

		/* the user clicked cancel in the password dialog */
		if (status == E_BOOK_ERROR_CANCELLED) {

			if (e_book_check_static_capability (book, "anon-access")) {

				GtkWidget *dialog;

				/* XXX "LDAP" has to be removed from the folowing message
				   so that it wil valid for other servers which provide
				   anonymous access*/

				dialog = gtk_message_dialog_new (NULL,
						0,
						GTK_MESSAGE_WARNING,
						GTK_BUTTONS_OK,
						"%s", _("Accessing LDAP Server anonymously"));
				g_signal_connect (dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
				gtk_widget_show (dialog);
				status = E_BOOK_ERROR_OK;

				goto done;
			}
		} else if (status == E_BOOK_ERROR_INVALID_SERVER_VERSION) {
			e_error_run (NULL, "addressbook:server-version", NULL);
			status = E_BOOK_ERROR_OK;
			goto done;
		} else if (status == E_BOOK_ERROR_UNSUPPORTED_AUTHENTICATION_METHOD) {
			goto done;
		} else {
			if (status == E_BOOK_ERROR_AUTHENTICATION_FAILED) {
				const gchar *uri = e_book_get_uri (book);
				gchar *stripped_uri = remove_parameters_from_uri (uri);
				const gchar *auth_domain = e_source_get_property (data->source, "auth-domain");
				const gchar *component_name;

				component_name = auth_domain ? auth_domain : "Addressbook";

				e_passwords_forget_password (component_name, stripped_uri);

				g_free (stripped_uri);
			} else if (was_in) {
				/* We already tried to authenticate to the server, and it failed with
				   other reason than with E_BOOK_ERROR_AUTHENTICATION_FAILED, thus stop
				   poking with the server and report error to the user. */
				goto done;
			}

			g_object_set_data (G_OBJECT (book), "authenticated", GINT_TO_POINTER (1));
			addressbook_authenticate (book, TRUE, data->source, load_source_auth_cb, closure);
			return;
		}
	}

done:
	if (data->cb)
		data->cb (book, status, data->closure);

	free_load_source_data (data);
}

static gboolean
get_remember_password (ESource *source)
{
	const gchar *value;

	value = e_source_get_property (source, "remember_password");
	if (value && !g_ascii_strcasecmp (value, "true"))
		return TRUE;

	return FALSE;
}

static void
set_remember_password (ESource *source, gboolean value)
{
	e_source_set_property (source, "remember_password",
			       value ? "true" : "false");
}

static void
addressbook_authenticate (EBook *book, gboolean previous_failure, ESource *source,
			  EBookCallback cb, gpointer closure)
{
	const gchar *password = NULL;
	gchar *pass_dup = NULL;
	const gchar *auth;
	const gchar *user;
	gchar *uri = remove_parameters_from_uri(e_book_get_uri (book));
	const gchar *auth_domain = e_source_get_property (source, "auth-domain");
	const gchar *component_name;

	component_name = auth_domain ? auth_domain : "Addressbook";

	password = e_passwords_get_password (component_name, uri);

	auth = e_source_get_property (source, "auth");

	if (auth && !strcmp ("ldap/simple-binddn", auth)) {
		user = e_source_get_property (source, "binddn");
	}
	else if (auth && !strcmp ("plain/password", auth)) {
		user = e_source_get_property (source, "user");
		if (!user) {
			user = e_source_get_property (source, "username");
		}
	}
	else {
		user = e_source_get_property (source, "email_addr");
	}
	if (!user)
		user = "";

	if (!password) {
		gchar *prompt;
		gchar *password_prompt;
		gboolean remember;
		const gchar *failed_auth;
		guint32 flags = E_PASSWORDS_REMEMBER_FOREVER|E_PASSWORDS_SECRET|E_PASSWORDS_ONLINE;

		if (previous_failure) {
			failed_auth = _("Failed to authenticate.\n");
			flags |= E_PASSWORDS_REPROMPT;
		}
		else {
			failed_auth = "";
		}

		password_prompt = g_strdup_printf (_("Enter password for %s (user %s)"),
					   e_source_peek_name (source), user);

		prompt = g_strconcat (failed_auth, password_prompt, NULL);
		g_free (password_prompt);

		remember = get_remember_password (source);
		pass_dup = e_passwords_ask_password (
			_("Enter password"), component_name,
			uri, prompt, flags, &remember, NULL);
		if (remember != get_remember_password (source))
			set_remember_password (source, remember);

		g_free (prompt);
	}

	if (password || pass_dup) {
		e_book_async_authenticate_user (book, user, password ? password : pass_dup,
						e_source_get_property (source, "auth"),
						cb, closure);
		g_free (pass_dup);
	}
	else {
		/* they hit cancel */
		cb (book, E_BOOK_ERROR_CANCELLED, closure);
	}

	g_free (uri);
}

static void
auth_required_cb (EBook *book, gpointer data)
{
	LoadSourceData *load_source_data = g_new0(LoadSourceData, 1);

	load_source_data->source = g_object_ref (g_object_ref (e_book_get_source (book)));
	load_source_data->cancelled = FALSE;
	addressbook_authenticate (book, FALSE, load_source_data->source,
				  load_source_auth_cb, load_source_data);

}
static void
load_source_cb (EBook *book, EBookStatus status, gpointer closure)
{
	LoadSourceData *load_source_data = closure;

	if (load_source_data->cancelled) {
		free_load_source_data (load_source_data);
		return;
	}

	if (status == E_BOOK_ERROR_OK && book != NULL) {
		const gchar *auth;

		auth = e_source_get_property (load_source_data->source, "auth");
		if (auth && strcmp (auth, "none")) {
			g_signal_connect (book, "auth_required", G_CALLBACK(auth_required_cb), NULL);

			if (e_book_is_online (book)) {
				addressbook_authenticate (book, FALSE, load_source_data->source,
							  load_source_auth_cb, closure);
				return;
		}
		}
	}
	load_source_data->cb (book, status, load_source_data->closure);
	free_load_source_data (load_source_data);
}

guint
addressbook_load (EBook *book,
		  EBookCallback cb, gpointer closure)
{
	LoadSourceData *load_source_data = g_new0 (LoadSourceData, 1);

	load_source_data->cb = cb;
	load_source_data->closure = closure;
	load_source_data->source = g_object_ref (g_object_ref (e_book_get_source (book)));
	load_source_data->cancelled = FALSE;

	e_book_async_open (book, FALSE, load_source_cb, load_source_data);

	return GPOINTER_TO_UINT (load_source_data);
}

void
addressbook_load_cancel (guint id)
{
	LoadSourceData *load_source_data = GUINT_TO_POINTER (id);

	load_source_data->cancelled = TRUE;
}

static void
default_book_cb (EBook *book, EBookStatus status, gpointer closure)
{
	LoadSourceData *load_source_data = closure;

	if (status == E_BOOK_ERROR_OK)
		load_source_data->source = g_object_ref (e_book_get_source (book));

	load_source_cb (book, status, closure);
}

void
addressbook_load_default_book (EBookCallback cb, gpointer closure)
{
	LoadSourceData *load_source_data = g_new (LoadSourceData, 1);
	EBook *book;

	load_source_data->cb = cb;
	load_source_data->source = NULL;
	load_source_data->closure = closure;
	load_source_data->cancelled = FALSE;

	book = e_book_new_default_addressbook (NULL);
	if (!book)
		load_source_cb (NULL, E_BOOK_ERROR_OTHER_ERROR, load_source_data); /* XXX we should just use a GError and it's error code here */
	else
		e_book_async_open (book, FALSE, default_book_cb, load_source_data);
}
