/* Evolution calendar - Component editor factory object
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cal-client/cal-client.h>
#include "comp-editor-factory.h"



/* A pending request */

typedef enum {
	REQUEST_EXISTING,
	REQUEST_NEW
} RequestType;

typedef struct {
	RequestType type;

	union {
		struct {
			char *uid;
		} existing;

		struct {
			CalObjType type;
		} new;
	} u;
} Request;

/* A client we have open */
typedef struct {
	/* Uri of the calendar, used as key in the clients hash table */
	GnomeVFSURI *uri;

	/* Client of the calendar */
	CalClient *client;

	/* Hash table of components that belong to this client */
	GHashTable *uid_comp_hash;

	/* Pending requests; they are pending if the client is still being opened */
	GSList *pending;
} OpenClient;

/* A component that is being edited */
typedef struct {
	/* Our parent client */
	OpenClient *parent;

	/* UID of the component we are editing, used as the key in the hash table */
	const char *uid;

	/* Component we are editing */
	CalComponent *comp;
} Component;

/* Private part of the CompEditorFactory structure */
struct CompEditorFactoryPrivate {
	/* Hash table of URI->OpenClient */
	GHashTable *uri_client_hash;
};



static void comp_editor_factory_class_init (CompEditorFactoryClass *class);
static void comp_editor_factory_init (CompEditorFactory *factory);
static void comp_editor_factory_destroy (GtkObject *object);

static void impl_editExisting (PortableServer_Servant servant,
			       const CORBA_char *uri,
			       const GNOME_Evolution_Calendar_CalObjUID uid,
			       CORBA_Environment *ev);
static void impl_editNew (PortableServer_Servant servant,
			  const CORBA_char *uri,
			  const GNOME_Evolution_Calendar_CalObjType type,
			  CORBA_Environment *ev);

static BonoboXObjectClass *parent_class = NULL;



BONOBO_X_TYPE_FUNC_FULL (CompEditorFactory,
			 GNOME_Evolution_Calendar_CompEditorFactory,
			 BONOBO_X_OBJECT_TYPE,
			 comp_editor_factory);

/* Class initialization function for the component editor factory */
static void
comp_editor_factory_class_init (CompEditorFactoryClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (BONOBO_X_OBJECT_TYPE);

	class->epv.editExisting = impl_editExisting;
	class->epv.editNew = impl_editNew;

	object_class->destroy = comp_editor_factory_destroy;
}

/* Object initialization function for the component editor factory */
static void
comp_editor_factory_init (CompEditorFactory *factory)
{
	CompEditorFactoryPrivate *priv;

	priv = g_new (CompEditorFactoryPrivate, 1);

	priv->uri_client_hash = g_hash_table_new (gnome_vfs_uri_hash, gnome_vfs_uri_hequal);
}

/* Used from g_hash_table_foreach(); frees a component structure */
static void
free_component_cb (gpointer key, gpointer value, gpointer data)
{
	Component *c;

	c = value;

	c->parent = NULL;
	c->uid = NULL;

	cal_component_unref (c->comp);
	c->comp = NULL;

	g_free (c);
}

/* Used from g_hash_table_foreach(); frees a client structure */
static void
free_client_cb (gpointer key, gpointer value, gpointer data)
{
	OpenClient *oc;
	GSList *l;

	oc = value;

	gnome_vfs_uri_unref (oc->uri);
	oc->uri = NULL;

	cal_client_unref (oc->client);
	oc->client = NULL;

	g_hash_table_foreach (oc->uid_comp_hash, free_component_cb, NULL);
	g_hash_table_destroy (oc->uid_comp_hash);
	oc->uid_comp_hash = NULL;

	for (l = oc->pending; l; l = l->next) {
		Request *r;

		r = l->data;

		if (r->type == REQUEST_EXISTING) {
			g_assert (r->u.existing.uid != NULL);
			g_free (r->u.existing.uid);
		}

		g_free (r);
	}
	g_slist_free (oc->pending);
	oc->pending = NULL;

	g_free (oc);
}

/* Destroy handler for the component editor factory */
static void
comp_editor_factory_destroy (GtkObject *object)
{
	CompEditorFactory *factory;
	CompEditorFactoryPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_COMP_EDITOR_FACTORY (object));

	factory = COMP_EDITOR_FACTORY (object);
	priv = factory->priv;

	g_hash_table_foreach (priv->uri_client_hash, free_client_cb, NULL);
	g_hash_table_destroy (priv->uri_client_hash);
	priv->uri_client_hash = NULL;

	g_free (priv);
	factory->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Creates a new OpenClient structure and queues the component editing/creation
 * process until the client is open.  Returns NULL if it could not issue the
 * open request.
 */
static OpenClient *
open_client (GnomeVFSURI *uri, gboolean only_if_exists)
{
	CalClient *client;

	client = cal_client_new ();
	if (!client)
		return NULL;

	oc = g_new (OpenClient, 1);

	gnome_vfs_uri_ref (uri);
	oc->uri = uri;

	oc->client = client;
	oc->uid_comp_hash = g_hash_table_new (g_str_hash, g_str_equal);
	oc->pending = NULL;

	gtk_signal_connect (GTK_OBJECT (oc->client), "cal_opened",
			    GTK_SIGNAL_FUNC (cal_opened_cb), oc);

	if (!cal_client_open_calendar (oc->client, uri, only_if_exists)) {
		gnome_vfs_uri_unref (oc->uri);
		gtk_object_unref (GTK_OBJECT (oc->client));
		g_hash_table_destroy (oc->uid_comp_hash);
		g_free (oc);

		return NULL;
	}

	return oc;
}

static void
impl_editExisting (PortableServer_Servant servant,
		   const CORBA_char *str_uri,
		   const GNOME_Evolution_Calendar_CalObjUID uid,
		   CORBA_Environment *ev)
{
	CompEditorFactory *factory;
	CompEditorFactoryPrivate *priv;
	GnomeVFSURI *uri;
	OpenClient *oc;
	CalClient *client;
	Component *c;

	factory = COMP_EDITOR_FACTORY (bonobo_object_from_servant (servant));
	priv = factory->priv;

	/* Look up the client */

	uri = gnome_vfs_uri_new (str_uri);
	if (!uri) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_CompEditorFactory_InvalidURI,
				     NULL);
		return;
	}

	oc = g_hash_table_lookup (priv->uri_client_hash, uri);
	if (oc)
		client = oc->client;
	else {
		oc = open_client (uri);
		if (!oc) {
			gnome_vfs_uri_unref (uri);

			CORBA_exception_set (
				ev, CORBA_USER_EXCEPTION,
				ex_GNOME_Evolution_Calendar_CompEditorFactory_BackendContactError,
				NULL);
			return;
		}

		client = oc->client;
	}

	gnome_vfs_uri_unref (uri);

	/* Look up the component */

	c = g_hash_table_lookup (oc->uid_comp_hash, 
}
