/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <stdio.h>

#include "camel.h"
#include "camel-nntp-store.h"
#include "camel-session.h"
#include "camel-exception.h"

static char*
authenticator (char *prompt, gboolean secret, CamelService *service, char *item,
	       CamelException *ex)
{
}

static void
print_name(gpointer data, gpointer foo)
{
	printf ("%s\n", (char*)data);
}

int
main (int argc, char **argv)
{
	CamelSession *session;
	CamelException *ex;
	CamelStore *store;
	CamelFolder *n_p_m_a;
	GList *groups;
	const gchar *news_url = "news://news.mozilla.org";

	gtk_init (&argc, &argv);
	camel_init ();		
	ex = camel_exception_new ();

	g_assert (camel_provider_register_as_module ("/usr/local/lib/evolution/camel-providers/0.0.1/libcamelnntp.so"));

	session = camel_session_new (authenticator);
	store = camel_session_get_store (session, news_url, ex);

	g_assert (store);

	camel_nntp_store_subscribe_group (store, "netscape.public.mozilla.announce");

	printf ("subscribed groups on %s\n", news_url);

	groups = camel_nntp_store_list_subscribed_groups (store);

	g_list_foreach(groups, print_name, NULL);

	n_p_m_a = camel_store_get_folder (store, "netscape.public.mozilla.announce", ex);

	camel_folder_open(n_p_m_a, FOLDER_OPEN_READ, ex);

	camel_folder_close(n_p_m_a, FALSE, ex);
}
