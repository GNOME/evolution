/*
 * e-folder.c: Abstract class for Evolution folders
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include <libgnome/libgnome.h>
#include "e-util/e-util.h"
#include "e-folder.h"

#define PARENT_TYPE gtk_object_get_type ()

static GtkObjectClass *parent_class;

#define EFC(o) E_FOLDER_CLASS (GTK_OBJECT (o)->klass)

enum {
	CHANGED,
	LAST_SIGNAL
};
static guint efolder_signals [LAST_SIGNAL] = { 0, };

static void
e_folder_destroy (GtkObject *object)
{
	EFolder *efolder = E_FOLDER (object);
	
	if (efolder->uri)
		g_free (efolder->uri);

	if (efolder->desc)
		g_free (efolder->desc);

	if (efolder->home_page)
		g_free (efolder->home_page);
	
	parent_class->destroy (object);
}

static void
e_folder_class_init (GtkObjectClass *object_class)
{
	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = e_folder_destroy;

	efolder_signals [CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EFolderClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE,
				0);
	/* Register our signals */
	gtk_object_class_add_signals (
		object_class, efolder_signals, LAST_SIGNAL);
}

static void
e_folder_init (GtkObject *object)
{
}

E_MAKE_TYPE (e_folder, "EFolder", EFolder, e_folder_class_init, e_folder_init, PARENT_TYPE)

void
e_folder_set_uri (EFolder *efolder, const char *uri)
{
	g_return_if_fail (efolder != NULL);
	g_return_if_fail (E_IS_FOLDER (efolder));
	g_return_if_fail (uri != NULL);

	if (efolder->uri)
		g_free (efolder->uri);
	
	efolder->uri = g_strdup (uri);
}

const char *
e_folder_get_uri (EFolder *efolder)
{
	g_return_val_if_fail (efolder != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER (efolder), NULL);

	return efolder->uri;
}

void
e_folder_set_description (EFolder *efolder, const char *desc)
{
	g_return_if_fail (efolder != NULL);
	g_return_if_fail (E_IS_FOLDER (efolder));
	g_return_if_fail (desc != NULL);

	if (efolder->desc)
		g_free (efolder->desc);
	
	efolder->desc = g_strdup (desc);
}

const char *
e_folder_get_description (EFolder *efolder)
{
	g_return_val_if_fail (efolder != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER (efolder), NULL);

	return efolder->desc;
}

void
e_folder_set_home_page (EFolder *efolder, const char *home_page)
{
	g_return_if_fail (efolder != NULL);
	g_return_if_fail (E_IS_FOLDER (efolder));
	g_return_if_fail (home_page != NULL);

	if (efolder->home_page)
		g_free (efolder->home_page);
	
	efolder->home_page = g_strdup (home_page);
}

const char *
e_folder_get_home_page   (EFolder *efolder)
{
	g_return_val_if_fail (efolder != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER (efolder), NULL);

	return efolder->home_page;
}

const char *
e_folder_get_type_name (EFolder *efolder)
{
	g_return_val_if_fail (efolder != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER (efolder), NULL);

	switch (efolder->type){
	case E_FOLDER_MAIL:
		return _("A folder containing mail items");

	case E_FOLDER_CONTACTS:
		return _("A folder containing contacts");
		
	case E_FOLDER_CALENDAR:
		return _("A folder containing calendar entries");
		
	case E_FOLDER_TASKS:
		return _("A folder containing tasks");

	default:
		g_assert_not_reached ();
	}

	return NULL;
}

void
e_folder_construct (EFolder *efolder, EFolderType type,
		    const char *uri, const char *name,
		    const char *desc, const char *home_page,
		    const char *view_name)
{
	g_return_if_fail (efolder != NULL);
	g_return_if_fail (E_IS_FOLDER (efolder));

	/* EFolders are self-owned */
	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (efolder), GTK_FLOATING);

	if (uri)
		efolder->uri = g_strdup (uri);
	if (name)
		efolder->name = g_strdup (name);
	if (desc)
		efolder->desc = g_strdup (desc);
	if (home_page)
		efolder->home_page = g_strdup (home_page);
	if (view_name)
		efolder->view_name = g_strdup (view_name);

	efolder->type = type;
}

EFolder *
e_folder_new (EFolderType type,
	      const char *uri, const char *name,
	      const char *desc, const char *home_page,
	      const char *view_name)
{
	EFolder *efolder;
	
	efolder = gtk_type_new (e_folder_get_type ());

	e_folder_construct (efolder, type, uri, name, desc, home_page, view_name);
	return efolder;
}

const char *
e_folder_get_name (EFolder *efolder)
{
	g_return_val_if_fail (efolder != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER (efolder), NULL);

	return efolder->name;
}

void
e_folder_set_name (EFolder *efolder, const char *name)
{
	g_return_if_fail (efolder != NULL);
	g_return_if_fail (E_IS_FOLDER (efolder));

	if (efolder->name)
		g_free (efolder->name);

	efolder->name = g_strdup (name);
}

const char *
e_folder_get_view_name (EFolder *efolder)
{
	g_return_val_if_fail (efolder != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER (efolder), NULL);

	return efolder->view_name;
}

void
e_folder_set_view_name (EFolder *efolder, const char *view_name)
{
	g_return_if_fail (efolder != NULL);
	g_return_if_fail (E_IS_FOLDER (efolder));

	if (efolder->view_name)
		g_free (efolder->view_name);

	efolder->view_name = g_strdup (view_name);

	gtk_signal_emit (GTK_OBJECT (efolder),
			 efolder_signals [CHANGED]);
}
