/*
 * mail-session.c: handles the session information and resource manipulation
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 2000 Helix Code, Inc. http://www.helixcode.com
 */
#include <config.h>
#include <gnome.h>
#include "mail.h"
#include "mail-session.h"
#include "mail-threads.h"

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
		if (!mail_op_get_password ((char *) prompt, secret, &ans))
			return NULL;
	}

	g_hash_table_insert (passwords, g_strdup (key), g_strdup (ans));
	return ans;
}

static char *
auth_callback (CamelAuthCallbackMode mode, char *data, gboolean secret,
	       CamelService *service, char *item, CamelException *ex)
{
	char *key, *ans, *url;

	url = camel_url_to_string (service->url, FALSE);
	key = g_strdup_printf ("%s:%s", url, item);
	g_free (url);

	if (mode == CAMEL_AUTHENTICATOR_TELL) {
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
	}

	ans = mail_session_request_dialog (data, secret, key, TRUE);
	g_free (key);

	if (!ans) {
		camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
				     "User canceled operation.");
	}

	return ans;
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
	g_hash_table_foreach (passwords, maybe_remember_password, url);
}


/* ******************** */

typedef struct _timeout_data_s {
	CamelTimeoutCallback cb;
	gpointer camel_data;
	gboolean result;
} timeout_data_t;

static gchar *
describe_camel_timeout (gpointer in_data, gboolean gerund)
{
	/* FIXME this is so wrong */

	if (gerund)
		return g_strdup ("Keeping connection alive");
	else
		return g_strdup ("Keep connection alive");
}

static void
noop_camel_timeout (gpointer in_data, gpointer op_data, CamelException *ex)
{
}

static void
do_camel_timeout (gpointer in_data, gpointer op_data, CamelException *ex)
{
	timeout_data_t *td = (timeout_data_t *) in_data;

	td->result = (td->cb) (td->camel_data);
}

static const mail_operation_spec spec_camel_timeout =
{
	describe_camel_timeout,
	0,
	noop_camel_timeout,
	do_camel_timeout,
	noop_camel_timeout
};

static gboolean 
camel_timeout (gpointer data)
{
	timeout_data_t *td = (timeout_data_t *) data;

	if (td->result == FALSE) {
		g_free (td);
		return FALSE;
	}

	mail_operation_queue (&spec_camel_timeout, td, FALSE);
	return TRUE;
}

static guint
register_callback (guint32 interval, CamelTimeoutCallback cb, gpointer camel_data)
{
	timeout_data_t *td;

	/* We do this because otherwise the timeout can get called
	 * more often than the dispatch thread can get rid of it,
	 * leading to timeout calls piling up, and we don't have a
	 * good way to watch the return values. It's not cool.
	 */
	g_return_val_if_fail (interval > 1000, 0);

	td = g_new (timeout_data_t, 1);
	td->result = TRUE;
	td->cb = cb;
	td->camel_data = camel_data;

	return gtk_timeout_add_full (interval, camel_timeout, NULL,
				     td, g_free);
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

	camel_init ();
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
