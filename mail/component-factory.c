/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* component-factory.c
 *
 * Authors: Ettore Perazzoli <ettore@helixcode.com>
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo.h>

#include "camel.h"

#include "Evolution.h"
#include "evolution-storage.h"

#include "evolution-shell-component.h"
#include "folder-browser.h"
#include "mail.h"		/* YUCK FIXME */
#include "mail-threads.h"
#include "e-util/e-gui-utils.h"
#include "e-util/e-setup.h"

#include "filter/filter-driver.h"
#include "component-factory.h"

static void create_vfolder_storage (EvolutionShellComponent *shell_component);
static void create_imap_storage (EvolutionShellComponent *shell_component);
static void real_create_imap_storage( gpointer user_data );
static void create_news_storage (EvolutionShellComponent *shell_component);
static void real_create_news_storage( gpointer user_data );

#define COMPONENT_FACTORY_ID "OAFIID:evolution-shell-component-factory:evolution-mail:0ea887d5-622b-4b8c-b525-18aa1cbe18a6"

static BonoboGenericFactory *factory = NULL;

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "mail", "evolution-inbox.png" },
	{ NULL, NULL }
};

/* GROSS HACK: for passing to other parts of the program */
EvolutionShellClient *global_shell_client = NULL;

/* EvolutionShellComponent methods and signals.  */

static EvolutionShellComponentResult
create_view (EvolutionShellComponent *shell_component,
	     const char *physical_uri,
	     const char *folder_type,
	     BonoboControl **control_return,
	     void *closure)
{
	BonoboControl *control;
	GtkWidget *folder_browser_widget;

	if (g_strcasecmp (folder_type, "mail") != 0)
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;

	control = folder_browser_factory_new_control (physical_uri);
	if (!control)
		return EVOLUTION_SHELL_COMPONENT_NOTFOUND;

	folder_browser_widget = bonobo_control_get_widget (control);

	g_assert (folder_browser_widget != NULL);
	g_assert (IS_FOLDER_BROWSER (folder_browser_widget));

	/* dum de dum, hack to let the folder browser know the storage its in */
	gtk_object_set_data (GTK_OBJECT (folder_browser_widget), "e-storage",
			     gtk_object_get_data(GTK_OBJECT (shell_component), "e-storage"));

	*control_return = control;

	return EVOLUTION_SHELL_COMPONENT_OK;
}

static void
create_folder (EvolutionShellComponent *shell_component,
	       const char *physical_uri,
	       const char *type,
	       const Evolution_ShellComponentListener listener,
	       void *closure)
{
	CORBA_Environment ev;
	CamelStore *store;
	CamelFolder *folder;
	CamelException ex;
	Evolution_ShellComponentListener_Result result;

	camel_exception_init (&ex);
	if (strcmp (type, "mail") != 0)
		result = Evolution_ShellComponentListener_UNSUPPORTED_TYPE;
	else {
		char *camel_url = g_strdup_printf ("mbox://%s", physical_uri);

		store = camel_session_get_store (session, camel_url, &ex);
		g_free (camel_url);
		if (!camel_exception_is_set (&ex)) {
			folder = camel_store_get_folder (store, "mbox",
							 TRUE, &ex);
			gtk_object_unref (GTK_OBJECT (store));
		} else {
			folder = NULL;
		}

		if (!camel_exception_is_set (&ex)) {
			gtk_object_unref (GTK_OBJECT (folder));
			result = Evolution_ShellComponentListener_OK;
		} else {
			result = Evolution_ShellComponentListener_INVALID_URI;
		}
	}

	camel_exception_clear (&ex);

	CORBA_exception_init (&ev);
	Evolution_ShellComponentListener_report_result (listener, result, &ev);
	CORBA_exception_free (&ev);
}

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      EvolutionShellClient *shell_client,
	      gpointer user_data)
{
	g_print ("evolution-mail: Yeeeh! We have an owner!\n");	/* FIXME */

	/* GROSS HACK */
	global_shell_client = shell_client;

	create_vfolder_storage (shell_component);
	create_imap_storage (shell_component);
	create_news_storage (shell_component);
}

static void
owner_unset_cb (EvolutionShellComponent *shell_component, gpointer user_data)
{
	gtk_main_quit ();
}

/* The factory function.  */

static BonoboObject *
factory_fn (BonoboGenericFactory *factory, void *closure)
{
	EvolutionShellComponent *shell_component;

	shell_component = evolution_shell_component_new (folder_types,
							 create_view,
							 create_folder,
							 NULL,
							 NULL,
							 NULL);

	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
			    GTK_SIGNAL_FUNC (owner_unset_cb), NULL);

	return BONOBO_OBJECT (shell_component);
}

void
component_factory_init (void)
{
	if (factory != NULL)
		return;

	factory = bonobo_generic_factory_new (COMPONENT_FACTORY_ID, factory_fn, NULL);

	if (factory == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize Evolution's mail component."));
		exit (1);
	}
}

static void
create_vfolder_storage (EvolutionShellComponent *shell_component)
{
	EvolutionShellClient *shell_client;
	Evolution_Shell corba_shell;
	EvolutionStorage *storage;
	
	shell_client = evolution_shell_component_get_owner (shell_component);
	if (shell_client == NULL) {
		g_warning ("We have no shell!?");
		return;
	}

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));
    
	storage = evolution_storage_new ("VFolders");
	if (evolution_storage_register_on_shell (storage, corba_shell) != EVOLUTION_STORAGE_OK) {
		g_warning ("Cannot register storage");
		return;
	}

	/* save the storage for later */
	gtk_object_set_data(GTK_OBJECT (shell_component), "e-storage", storage);

	/* this is totally not the way we want to do this - but the
	   filter stuff needs work before we can remove it */
	{
		FilterDriver *fe;
		int i, count;
		char *user, *system;

		user = g_strdup_printf ("%s/vfolders.xml", evolution_dir);
		system = g_strdup_printf("%s/evolution/vfoldertypes.xml", EVOLUTION_DATADIR);
		fe = filter_driver_new(system, user, mail_uri_to_folder);
		g_free(user);
		g_free(system);
		count = filter_driver_rule_count(fe);

		for (i = 0; i < count; i++) {
			struct filter_option *fo;
			GString *query;
			struct filter_desc *desc = NULL;
			char *desctext, descunknown[64];
			char *name;

			fo = filter_driver_rule_get(fe, i);
			if (fo == NULL)
				continue;
			query = g_string_new("");
			if (fo->description)
				desc = fo->description->data;
			if (desc)
				desctext = desc->data;
			else {
				sprintf(descunknown, "vfolder-%p", fo);
				desctext = descunknown;
			}
			g_string_sprintf(query, "vfolder:%s/vfolder/%s?", evolution_dir, desctext);
			filter_driver_expand_option(fe, query, NULL, fo);
			name = g_strdup_printf("/%s", desctext);
			evolution_storage_new_folder (storage, name,
						      "mail",
						      query->str,
						      desctext);
			g_string_free(query, TRUE);
			g_free(name);
		}
		gtk_object_unref(GTK_OBJECT (fe));
	}
}

struct create_info_s {
	EvolutionStorage *storage;
	char *source;
};

static void
create_imap_storage (EvolutionShellComponent *shell_component)
{
	EvolutionShellClient *shell_client;
	Evolution_Shell corba_shell;
	EvolutionStorage *storage;
	char *cpath, *source, *server, *p;
	struct create_info_s *ii;

	cpath = g_strdup_printf ("=%s/config=/mail/source", evolution_dir);
	source = gnome_config_get_string (cpath);
	g_free (cpath);

	if (!source || strncasecmp (source, "imap://", 7))
		return;
	
	shell_client = evolution_shell_component_get_owner (shell_component);
	if (shell_client == NULL) {
		g_warning ("We have no shell!?");
		return;
	}

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));

	if (!(server = strchr (source, '@'))) {
		g_free (source);
		return;
	}
	
	server++;
	for (p = server; *p && *p != '/'; p++);

	server = g_strndup (server, (gint)(p - server));
	
	storage = evolution_storage_new (server);
	g_free (server);

	if (evolution_storage_register_on_shell (storage, corba_shell) != EVOLUTION_STORAGE_OK) {
		g_warning ("Cannot register storage");
		g_free (source);
		return;
	}

	ii = g_new (struct create_info_s, 1);
	ii->storage = storage;
	ii->source = g_strdup (source);

#ifdef USE_BROKEN_THREADS
	mail_operation_try ("Create IMAP Storage", real_create_imap_storage, g_free, ii);
#else
	real_create_imap_storage (ii);
	g_free (ii);
#endif
	/* Note the g_free as our cleanup function deleting the ii struct when we're done */
}

static void
real_create_imap_storage (gpointer user_data)
{	
	CamelException *ex;
	EvolutionStorage *storage;
	char *p, *source, *dir_sep;
	CamelStore *store;
	CamelFolder *folder;
	GPtrArray *lsub;
	int i, max;
	struct create_info_s *ii;

	ii = (struct create_info_s *) user_data;
	storage = ii->storage;
	source = ii->source;

#ifdef USE_BROKEN_THREADS
	mail_op_hide_progressbar ();
	mail_op_set_message ("Connecting to IMAP service...");
#endif
	ex = camel_exception_new ();
	
	store = camel_session_get_store (session, source, ex);
	if (!store) {
		goto cleanup;
	}
	
	camel_service_connect (CAMEL_SERVICE (store), ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		goto cleanup;
	}

#ifdef USE_BROKEN_THREADS
	mail_op_set_message ("Connected. Examining folders...");
#endif

	folder = camel_store_get_root_folder (store, ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		goto cleanup;
	}

	/* we need a way to set the namespace */
	lsub = camel_folder_get_subfolder_names (folder, ex);

	p = g_strdup_printf ("%s/INBOX", source);
	evolution_storage_new_folder (storage, "/INBOX", "mail", p, "description");

	/*dir_sep = CAMEL_IMAP_STORE (store)->dir_sep;*/
	
	max = lsub->len;
	for (i = 0; i < max; i++) {
		char *path, *buf, *dirname;

#if 0
		if (strcmp (dir_sep, "/")) {
			dirname = e_strreplace ((char *)lsub->pdata[i], dir_sep, "/");
		} else {
			dirname = g_strdup ((char *)lsub->pdata[i]);
		}
#endif
		dirname = g_strdup ((char *)lsub->pdata[i]);

		path = g_strdup_printf ("/%s", dirname);
		g_free (dirname);
		buf = g_strdup_printf ("%s/%s", source, path + 1);
		printf ("buf = %s\n", buf);

#ifdef USE_BROKEN_THREADS
		mail_op_set_message ("Adding %s", path);
#endif

		evolution_storage_new_folder (storage, path, "mail", buf, "description");
	}

 cleanup:
	g_free (ii->source);
#ifdef USE_BROKEN_THREADS
	if (camel_exception_is_set (ex))
		mail_op_error ("%s", camel_exception_get_description (ex));
#endif
	camel_exception_free (ex);
}

static void
create_news_storage (EvolutionShellComponent *shell_component)
{
	EvolutionShellClient *shell_client;
	Evolution_Shell corba_shell;
	EvolutionStorage *storage;
	char *cpath, *source, *server, *p;
	struct create_info_s *ni;

	cpath = g_strdup_printf ("=%s/config=/news/source", evolution_dir);
	source = gnome_config_get_string (cpath);
	g_free (cpath);

	if (!source || strncasecmp (source, "news://", 7))
		return;
	
	shell_client = evolution_shell_component_get_owner (shell_component);
	if (shell_client == NULL) {
		g_warning ("We have no shell!?");
		return;
	}

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));

	server = source + 7;
	for (p = server; *p && *p != '/'; p++);

	server = g_strndup (server, (gint)(p - server));
	
	storage = evolution_storage_new (server);
	g_free (server);

	if (evolution_storage_register_on_shell (storage, corba_shell) != EVOLUTION_STORAGE_OK) {
		g_warning ("Cannot register storage");
		g_free (source);
		return;
	}

	ni = g_new( struct create_info_s, 1 );
	ni->storage = storage;
	ni->source = g_strdup( source );

#ifdef USE_BROKEN_THREADS
	mail_operation_try( "Create News Storage", real_create_news_storage, g_free, ni );
#else
	real_create_news_storage( ni );
	g_free( ni );
#endif
	/* again note the g_free cleanup func */
}

static void
real_create_news_storage( gpointer user_data )
{	
	EvolutionStorage *storage;
	char *source;
	CamelStore *store;
	CamelFolder *folder;
	CamelException *ex;
	GPtrArray *lsub;
	int i, max;
	struct create_info_s *ni;

	ni = (struct create_info_s *) user_data;
	storage = ni->storage;
	source = ni->source;

#ifdef USE_BROKEN_THREADS
	mail_op_hide_progressbar();
	mail_op_set_message( "Connecting to news service..." );
#endif

	ex = camel_exception_new ();
	
	store = camel_session_get_store (session, source, ex);
	if (!store) {
		goto cleanup;
	}
	
	camel_service_connect (CAMEL_SERVICE (store), ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		goto cleanup;
	}

#ifdef USE_BROKEN_THREADS
	mail_op_set_message( "Connected. Examining folders..." );
#endif

	folder = camel_store_get_root_folder (store, ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		goto cleanup;
	}

	/* we need a way to set the namespace */
	lsub = camel_folder_get_subfolder_names (folder, ex);

	max = lsub->len;
	for (i = 0; i < max; i++) {
		char *path, *buf;

		path = g_strdup_printf ("/%s", (char *)lsub->pdata[i]);
		buf = g_strdup_printf ("%s%s", source, path);

#ifdef USE_BROKEN_THREADS
		mail_op_set_message( "Adding %s", path );
#endif
		/* FIXME: should be s,"mail","news",? */
		evolution_storage_new_folder (storage, path, "mail", buf, "description");
	}

 cleanup:
	g_free( ni->source );
#ifdef USE_BROKEN_THREADS
	if( camel_exception_is_set( ex ) )
		mail_op_error( "%s", camel_exception_get_description( ex ) );
#endif
	camel_exception_free (ex);
}
