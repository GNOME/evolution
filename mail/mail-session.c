/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-session.c: handles the session information and resource manipulation */
/*
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
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>
#include "mail.h"
#include "mail-session.h"
#include "mail-mt.h"

CamelSession *session;


#define MAIL_SESSION_TYPE     (mail_session_get_type ())
#define MAIL_SESSION(obj)     (CAMEL_CHECK_CAST((obj), MAIL_SESSION_TYPE, MailSession))
#define MAIL_SESSION_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), MAIL_SESSION_TYPE, MailSessionClass))
#define MAIL_IS_SESSION(o)    (CAMEL_CHECK_TYPE((o), MAIL_SESSION_TYPE))


typedef struct _MailSession {
	CamelSession parent_object;

	GHashTable *passwords;
	gboolean interaction_enabled;
} MailSession;

typedef struct _MailSessionClass {
	CamelSessionClass parent_class;

} MailSessionClass;


static char *get_password (CamelSession *session, const char *prompt,
			   gboolean secret, CamelService *service,
			   const char *item, CamelException *ex);
static void forget_password (CamelSession *session, CamelService *service,
			     const char *item, CamelException *ex);
static gboolean alert_user (CamelSession *session, CamelSessionAlertType type,
			    const char *prompt, gboolean cancel);
static guint register_timeout (CamelSession *session, guint32 interval,
			       CamelTimeoutCallback cb, gpointer camel_data);
static gboolean remove_timeout (CamelSession *session, guint handle);


static char *decode_base64 (char *base64);

static void
init (MailSession *session)
{
	char *key, *value;
	void *iter;

	session->passwords = g_hash_table_new (g_str_hash, g_str_equal);

	iter = gnome_config_private_init_iterator ("/Evolution/Passwords");
	if (iter) {
		while (gnome_config_iterator_next (iter, &key, &value)) {
			g_hash_table_insert (session->passwords,
					     decode_base64 (key),
					     decode_base64 (value));
			g_free (key);
			g_free (value);
		}
	}
}

static void
class_init (MailSessionClass *mail_session_class)
{
	CamelSessionClass *camel_session_class =
		CAMEL_SESSION_CLASS (mail_session_class);

	/* virtual method override */
	camel_session_class->get_password = get_password;
	camel_session_class->forget_password = forget_password;
	camel_session_class->alert_user = alert_user;
	camel_session_class->register_timeout = register_timeout;
	camel_session_class->remove_timeout = remove_timeout;
}

CamelType
mail_session_get_type (void)
{
	static CamelType mail_session_type = CAMEL_INVALID_TYPE;

	if (mail_session_type == CAMEL_INVALID_TYPE) {
		mail_session_type = camel_type_register (
			camel_session_get_type (), "MailSession",
			sizeof (MailSession),
			sizeof (MailSessionClass),
			(CamelObjectClassInitFunc) class_init,
			NULL,
			(CamelObjectInitFunc) init,
			NULL);
	}

	return mail_session_type;
}


static char *
make_key (CamelService *service, const char *item)
{
	char *key, *url;

	if (service) {
		url = camel_url_to_string (service->url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
		key = g_strdup_printf ("%s:%s", url, item);
		g_free (url);
	} else
		key = g_strdup (item);

	return key;
}

static char *
get_password (CamelSession *session, const char *prompt, gboolean secret,
	      CamelService *service, const char *item, CamelException *ex)
{
	MailSession *mail_session = MAIL_SESSION (session);
	char *key, *ans;

	key = make_key (service, item);

	ans = g_hash_table_lookup (mail_session->passwords, key);
	if (ans) {
		g_free (key);
		return g_strdup (ans);
	}

	if (!mail_session->interaction_enabled ||
	    !(ans = mail_get_password (prompt, secret))) {
		g_free (key);
		camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
				     _("User canceled operation."));
		return NULL;
	}

	g_hash_table_insert (mail_session->passwords, key, g_strdup (ans));
	return ans;
}

static void
forget_password (CamelSession *session, CamelService *service,
		 const char *item, CamelException *ex)
{
	MailSession *mail_session = MAIL_SESSION (session);
	char *key = make_key (service, item);
	gpointer old_key, old_data;

	if (!g_hash_table_lookup_extended (mail_session->passwords, key,
					   &old_key, &old_data))
		return;

	g_hash_table_remove (mail_session->passwords, key);
	g_free (old_data);
	g_free (old_key);
}

static gboolean
alert_user (CamelSession *session, CamelSessionAlertType type,
	    const char *prompt, gboolean cancel)
{
	MailSession *mail_session = MAIL_SESSION (session);
	const char *message_type;

	if (!mail_session->interaction_enabled)
		return FALSE;

	switch (type) {
	case CAMEL_SESSION_ALERT_INFO:
		message_type = GNOME_MESSAGE_BOX_INFO;
		break;
	case CAMEL_SESSION_ALERT_WARNING:
		message_type = GNOME_MESSAGE_BOX_WARNING;
		break;
	case CAMEL_SESSION_ALERT_ERROR:
		message_type = GNOME_MESSAGE_BOX_ERROR;
		break;
	}
	return mail_user_message (message_type, prompt, cancel);
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
register_timeout (CamelSession *session, guint32 interval, CamelTimeoutCallback cb, gpointer camel_data)
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
remove_timeout (CamelSession *session, guint handle)
{
	gtk_timeout_remove (handle);
	return TRUE;
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
mail_session_remember_password (const char *url_string)
{
	GHashTable *passwords = MAIL_SESSION (session)->passwords;
	CamelURL *url;
	char *simple_url;

	url = camel_url_new (url_string, NULL);
	simple_url = camel_url_to_string (url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	camel_url_free (url);

	g_hash_table_foreach (passwords, maybe_remember_password, simple_url);
	g_free (simple_url);
}

void
mail_session_forget_password (const char *key)
{
	GHashTable *passwords = MAIL_SESSION (session)->passwords;
	gpointer okey, value;

	if (g_hash_table_lookup_extended (passwords, key, &okey, &value)) {
		g_hash_table_remove (passwords, key);
		g_free (okey);
		g_free (value);
	}
}

void
mail_session_init (void)
{
	char *camel_dir;

	if (camel_init (evolution_dir, TRUE) != 0)
		exit (0);

	session = CAMEL_SESSION (camel_object_new (MAIL_SESSION_TYPE));

	camel_dir = g_strdup_printf ("%s/mail", evolution_dir);
	camel_session_construct (session, camel_dir);
	g_free (camel_dir);
}

void
mail_session_enable_interaction (gboolean enable)
{
	MAIL_SESSION (session)->interaction_enabled = enable;
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
	GHashTable *passwords = MAIL_SESSION (session)->passwords;

	g_hash_table_foreach_remove (passwords, free_entry, NULL);
	gnome_config_private_clean_section ("/Evolution/Passwords");
	gnome_config_sync ();
}

void
mail_session_set_password (const char *url, const char *password)
{
	GHashTable *passwords = MAIL_SESSION (session)->passwords;

	g_hash_table_insert (passwords, g_strdup (url), g_strdup (password));
}

