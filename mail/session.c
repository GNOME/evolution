/*
 * session.c: handles the session infomration and resource manipulation
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 2000 Helix Code, Inc. http://www.helixcode.com
 */
#include <config.h>
#include <gnome.h>
#include "mail.h"
#include "e-util/e-setup.h"

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

static char *
evolution_auth_callback (char *prompt, gboolean secret,
			 CamelService *service, char *item,
			 CamelException *ex)
{
	GtkWidget *dialog;
	char *key = NULL, *ans;

	if (service && item) {
		key = g_strdup_printf ("%s:%s", camel_url_to_string (service->url, FALSE), item);

		if (passwords) {
			ans = g_hash_table_lookup (passwords, key);
			if (ans) {
				g_free (key);
				return g_strdup (ans);
			}
		} else
			passwords = g_hash_table_new (g_str_hash, g_str_equal);
	}

	/* XXX parent window? */
	dialog = gnome_request_dialog (secret, prompt, NULL, 0,
				       request_callback, &ans, NULL);
	if (!dialog) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     "Could not create dialog box.");
		g_free (key);
		return NULL;
	}
	if (gnome_dialog_run_and_close (GNOME_DIALOG (dialog)) == -1 ||
	    ans == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
				     "User cancelled query.");
		g_free (key);
		return NULL;
	}

	if (service && item)
		g_hash_table_insert (passwords, key, g_strdup (ans));
	else
		g_free (key);

	return ans;
}

void
session_init (void)
{
	e_setup_base_dir ();
	camel_init ();

	session = camel_session_new (evolution_auth_callback);
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
