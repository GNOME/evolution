/*
 * session.c: handles the session information and resource manipulation
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 2000 Helix Code, Inc. http://www.helixcode.com
 */
#include <config.h>
#include <gnome.h>
#include "mail.h"
#include "mail-threads.h"

CamelSession *session;
GHashTable *passwords;

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
mail_request_dialog (const char *prompt, gboolean secret, const char *key,
		     gboolean async)
{
	GtkWidget *dialog;

	char *ans;

	if (!passwords)
		passwords = g_hash_table_new (g_str_hash, g_str_equal);

	ans = g_hash_table_lookup (passwords, key);
	if (ans)
		return g_strdup (ans);

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

	if (!passwords)
		passwords = g_hash_table_new (g_str_hash, g_str_equal);

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

	ans = mail_request_dialog (data, secret, key, TRUE);
	g_free (key);

	if (!ans) {
		camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
				     "User canceled operation.");
	}

	return ans;
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
session_init (void)
{
	char *camel_dir;

	camel_init ();
	camel_dir = g_strdup_printf ("%s/mail", evolution_dir);
	session = camel_session_new (camel_dir, auth_callback,
				     register_callback, remove_callback);
	g_free (camel_dir);
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
forget_passwords (BonoboUIHandler *uih, void *user_data, const char *path)
{
	g_hash_table_foreach_remove (passwords, free_entry, NULL);
}
