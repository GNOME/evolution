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
#include "session.h"
#include "e-util/e-setup.h"
#include "camel/camel.h"

SessionStore *default_session;

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

static void
session_providers_init (void)
{
	camel_provider_register_as_module (CAMEL_PROVIDERDIR "/libcamelmbox.so");
}

SessionStore *
session_store_new (const char *uri)
{
	SessionStore *ss = g_new (SessionStore, 1);
	CamelException ex;
	
	ss->session = camel_session_new (evolution_auth_callback);
	camel_exception_init (&ex);
	ss->store = camel_session_get_store (ss->session, uri, &ex);
	

	g_assert (ss->session);
	g_assert (ss->store);
	
	return ss;
}

void
session_store_destroy (SessionStore *ss)
{
	g_assert (ss != NULL);

	gtk_object_unref (GTK_OBJECT (ss->store));
	gtk_object_unref (GTK_OBJECT (ss->session));

	g_free (ss);
}

static void
init_default_session (void)
{
	char *url;

	url = g_strconcat ("mbox://", evolution_folders_dir, NULL);
	default_session = session_store_new (url);
	g_free (url);
}

void
session_init (void)
{
	e_setup_base_dir ();
	camel_init ();
	session_providers_init ();

	init_default_session ();
}

 
