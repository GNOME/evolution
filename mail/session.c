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
	char *ans;

	/* XXX look up stored passwords */

	/* XXX parent window? */
	dialog = gnome_request_dialog (secret, prompt, NULL, 0,
				       request_callback, &ans, NULL);
	if (!dialog) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     "Could not create dialog box.");
		return NULL;
	}
	if (gnome_dialog_run_and_close (GNOME_DIALOG (dialog)) == -1 ||
	    ans == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
				     "User cancelled query.");
		return NULL;
	}

	return ans;
}

void
session_init (void)
{
	camel_init ();

	session = camel_session_new (evolution_auth_callback);
}
