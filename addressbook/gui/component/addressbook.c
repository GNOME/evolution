/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* addressbook.c
 *
 * Copyright (C) 2000, 2001, 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Chris Lahey (clahey@ximian.com)
 */

#include <config.h>

#include <string.h>

#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <libebook/e-book.h>

#include "e-util/e-passwords.h"

#include "addressbook.h"

#define d(x)

static void addressbook_authenticate (EBook *book, gboolean previous_failure,
				      ESource *source, EBookCallback cb, gpointer closure);

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

static gchar*
remove_parameters_from_uri (gchar *uri)
{
  gchar **components;
  gchar *new_uri = NULL;
                                                                                                                             
  components = g_strsplit (uri, ";", 2);
  if (components[0])
        new_uri = g_strdup (components[0]);
   g_strfreev (components);
   return new_uri;
}

static void
load_source_auth_cb (EBook *book, EBookStatus status, gpointer closure)
{
	LoadSourceData *data = closure;

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
							 _("Accessing LDAP Server anonymously"));
			g_signal_connect (dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
			gtk_widget_show (dialog);
			data->cb (book, E_BOOK_ERROR_OK, data->closure);
			free_load_source_data (data);
			return;
			}
		}
		else {
			gchar *uri = e_source_get_uri (data->source);
			gchar *stripped_uri = remove_parameters_from_uri (uri);
			const gchar *auth_domain = e_source_get_property (data->source, "auth-domain");
		        const gchar *component_name;
			
			component_name = auth_domain ? auth_domain : "Addressbook";
			
			e_passwords_forget_password (component_name, stripped_uri);
			addressbook_authenticate (book, TRUE, data->source, load_source_auth_cb, closure);
			
			g_free (stripped_uri);
			g_free (uri);
			return;
		}
	}

	data->cb (book, status, data->closure);

	free_load_source_data (data);
}

static gboolean
get_remember_password (ESource *source)
{
	const gchar *value;

	value = e_source_get_property (source, "remember_password");
	if (value && !strcasecmp (value, "true"))
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
	const char *password = NULL;
	char *pass_dup = NULL;
	const gchar *auth;
	const gchar *user;
	gchar *uri = e_source_get_uri (source);
        gchar *stripped_uri = remove_parameters_from_uri (uri);
	const gchar *auth_domain = e_source_get_property (source, "auth-domain");
	const gchar *component_name;
			
	component_name = auth_domain ? auth_domain : "Addressbook";
	g_free (uri);
	uri = stripped_uri;

	password = e_passwords_get_password (component_name, uri);

	auth = e_source_get_property (source, "auth");

	if (auth && !strcmp ("ldap/simple-binddn", auth))
		user = e_source_get_property (source, "binddn");
	else if (auth && !strcmp ("plain/password", auth))
		user = e_source_get_property (source, "user");
	else
		user = e_source_get_property (source, "email_addr");
	if (!user)
		user = "";

	if (!password) {
		char *prompt;
		gboolean remember;
		char *failed_auth;
		guint32 flags = E_PASSWORDS_REMEMBER_FOREVER|E_PASSWORDS_SECRET|E_PASSWORDS_ONLINE;

		if (previous_failure) {
			failed_auth = _("Failed to authenticate.\n");
			flags |= E_PASSWORDS_REPROMPT;
		}
		else {
			failed_auth = "";
		}

		prompt = g_strdup_printf (_("%sEnter password for %s (user %s)"),
					  failed_auth, e_source_peek_name (source), user);

		remember = get_remember_password (source);
		pass_dup = e_passwords_ask_password (prompt, component_name, uri, prompt,
						     flags, &remember,
						     NULL);
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

		/* check if the addressbook needs authentication */

		if (auth && strcmp (auth, "none")) {
			addressbook_authenticate (book, FALSE, load_source_data->source,
						  load_source_auth_cb, closure);

			return;
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
