/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-session.c: handles the session information and resource manipulation */
/*
 *  Authors: Miguel de Icaza <miguel@gnu.org>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-config.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-stock.h>
#include "mail.h"
#include "mail-session.h"
#include "mail-mt.h"

CamelSession *session;

static GHashTable *passwords;
static gboolean interaction_enabled;

static void
request_callback (gchar *string, gpointer data)
{
	char **ans = data;

	if (string)
		*ans = g_strdup(string);
	else
		*ans = NULL;
}

char *
mail_session_request_dialog (const char *prompt, gboolean secret, const char *key,
			     gboolean async)
{
	GtkWidget *dialog;
	char *ans;

	if (!passwords)
		passwords = g_hash_table_new (g_str_hash, g_str_equal);

	ans = g_hash_table_lookup (passwords, key);
	if (ans)
		return g_strdup (ans);

	if (!interaction_enabled)
		return NULL;

	if (!async) {
		dialog = gnome_request_dialog (secret, prompt, NULL, 0,
					       request_callback, &ans, NULL);
		if (!dialog)
			return NULL;
		if (gnome_dialog_run_and_close (GNOME_DIALOG (dialog)) == -1 ||
		    ans == NULL)
			return NULL;
	} else {
		if ((ans = mail_get_password (prompt, secret)) == NULL)
			return NULL;
	}
	
	g_hash_table_insert (passwords, g_strdup (key), g_strdup (ans));
	return ans;
}

gboolean
mail_session_accept_dialog (const char *prompt, const char *key, gboolean async)
{
	GtkWidget *dialog;
	GtkWidget *label;
	
	if (!interaction_enabled)
		return FALSE;
	
	if (!async) {
		dialog = gnome_dialog_new (_("Do you accept?"),
					   GNOME_STOCK_BUTTON_YES,
					   GNOME_STOCK_BUTTON_NO,
					   NULL);
		gnome_dialog_set_default (GNOME_DIALOG (dialog), 1);
		gtk_window_set_policy (GTK_WINDOW (dialog), TRUE, TRUE, TRUE);
		
		label = gtk_label_new (prompt);
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
		
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label,
				    TRUE, TRUE, 0);
		gtk_widget_show (label);
		
		if (gnome_dialog_run_and_close (GNOME_DIALOG (dialog)) == 0)
			return TRUE;
		else
			return FALSE;
	} else {
		return mail_get_accept (prompt);
	}
}

static gpointer
auth_callback (CamelAuthCallbackMode mode, char *data, gboolean secret,
	       CamelService *service, char *item, CamelException *ex)
{
	char *key, *ans, *url;
	gboolean accept;
	
	url = camel_url_to_string (service->url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	key = g_strdup_printf ("%s:%s", url, item);
	g_free (url);
	
	switch (mode) {
	case CAMEL_AUTHENTICATOR_TELL:
		if (!data) {
			g_hash_table_remove (passwords, key);
			g_free (key);
		} else {
			gpointer old_key, old_data;
			
			if (g_hash_table_lookup_extended (passwords, key,
							  &old_key,
							  &old_data)) {
				g_hash_table_insert (passwords, old_key, data);
				g_free (old_data);
				g_free (key);
			} else
				g_hash_table_insert (passwords, key, data);
		}
		
		return NULL;
		break;
	case CAMEL_AUTHENTICATOR_ASK:
		ans = mail_session_request_dialog (data, secret, key, TRUE);
		g_free (key);
	
		if (!ans) {
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     "User canceled operation.");
		}
		
		return ans;
		break;
	case CAMEL_AUTHENTICATOR_ACCEPT:
		accept = mail_session_accept_dialog (data, key, TRUE);
		g_free (key);
		
		return GINT_TO_POINTER (accept);
		break;
	}
	
	return NULL;
}

static char *
decode_base64 (char *base64)
{
	char *plain, *pad = "==";
	int len, out, state, save;

	len = strlen (base64);
	plain = g_malloc0 (len);
	state = save = 0;
	out = base64_decode_step (base64, len, plain, &state, &save);
	if (len % 4) {
		base64_decode_step (pad, 4 - len % 4, plain + out,
				    &state, &save);
	}

	return plain;
}

static void
maybe_remember_password (gpointer key, gpointer password, gpointer url)
{
	char *path, *key64, *pass64;
	int len, state, save;

	len = strlen (url);
	if (strncmp (key, url, len) != 0)
		return;

	len = strlen (key);
	key64 = g_malloc0 ((len + 2) * 4 / 3 + 1);
	state = save = 0;
	base64_encode_close (key, len, FALSE, key64, &state, &save);
	path = g_strdup_printf ("/Evolution/Passwords/%s", key64);
	g_free (key64);

	len = strlen (password);
	pass64 = g_malloc0 ((len + 2) * 4 / 3 + 1);
	state = save = 0;
	base64_encode_close (password, len, FALSE, pass64, &state, &save);

	gnome_config_private_set_string (path, pass64);
	g_free (path);
	g_free (pass64);
}

void
mail_session_remember_password (const char *url)
{
	g_hash_table_foreach (passwords, maybe_remember_password, (void *) url);
}

void
mail_session_forget_password (const char *key)
{
	gpointer okey, value;
	
	if (g_hash_table_lookup_extended (passwords, key, &okey, &value)) {
		g_hash_table_remove (passwords, key);
		g_free (okey);
		g_free (value);
	}
}

/* ******************** */

struct _timeout_data {
	CamelTimeoutCallback cb;
	gpointer camel_data;
	gboolean result;
};

struct _timeout_msg {
	struct _mail_msg msg;

	CamelTimeoutCallback cb;
	gpointer camel_data;
};

static void timeout_timeout(struct _mail_msg *mm)
{
	struct _timeout_msg *m = (struct _timeout_msg *)mm;

	/* we ignore the callback result, do we care?? no. */
	m->cb(m->camel_data);
}

static struct _mail_msg_op timeout_op = {
	NULL,
	timeout_timeout,
	NULL,
	NULL,
};

static gboolean 
camel_timeout (gpointer data)
{
	struct _timeout_data *td = data;
	struct _timeout_msg *m;

	m = mail_msg_new(&timeout_op, NULL, sizeof(*m));

	m->cb = td->cb;
	m->camel_data = td->camel_data;

	e_thread_put(mail_thread_queued, (EMsg *)m);

	return TRUE;
}

static guint
register_callback (guint32 interval, CamelTimeoutCallback cb, gpointer camel_data)
{
	struct _timeout_data *td;

	/* We do this because otherwise the timeout can get called
	 * more often than the dispatch thread can get rid of it,
	 * leading to timeout calls piling up, and we don't have a
	 * good way to watch the return values. It's not cool.
	 */
	g_return_val_if_fail (interval > 1000, 0);

	td = g_malloc(sizeof(*td));
	td->result = TRUE;
	td->cb = cb;
	td->camel_data = camel_data;

	return gtk_timeout_add_full(interval, camel_timeout, NULL, td, g_free);
}

static gboolean
remove_callback (guint handle)
{
	gtk_timeout_remove (handle);
	return TRUE;
}

/* ******************** */

void
mail_session_init (void)
{
	char *camel_dir, *key, *value;
	void *iter;
	
	if (camel_init (evolution_dir, TRUE) != 0)
		exit (0);
	
	camel_dir = g_strdup_printf ("%s/mail", evolution_dir);
	session = camel_session_new (camel_dir, auth_callback,
				     register_callback, remove_callback);
	g_free (camel_dir);

	passwords = g_hash_table_new (g_str_hash, g_str_equal);
	iter = gnome_config_private_init_iterator ("/Evolution/Passwords");
	if (iter) {
		while (gnome_config_iterator_next (iter, &key, &value)) {
			g_hash_table_insert (passwords, decode_base64 (key),
					     decode_base64 (value));
			g_free (key);
			g_free (value);
		}
	}
}

void
mail_session_enable_interaction (gboolean enable)
{
	interaction_enabled = enable;
}

static gboolean
free_entry (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	memset (value, 0, strlen (value));
	g_free (value);
	return TRUE;
}

void
mail_session_forget_passwords (BonoboUIComponent *uih, void *user_data,
			       const char *path)
{
	g_hash_table_foreach_remove (passwords, free_entry, NULL);
	gnome_config_private_clean_section ("/Evolution/Passwords");
	gnome_config_sync ();
}

void
mail_session_set_password (const char *url, const char *password)
{
	g_hash_table_insert (passwords, g_strdup (url), g_strdup (password));
}
