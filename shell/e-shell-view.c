/*
 * E-shell-view.c: Implements a Shell View of Evolution
 *
 * Authors:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include "e-util/e-util.h"
#include "e-shell-view.h"
#include "e-shell-view-menu.h"

#define PARENT_TYPE gnome_app_get_type ()

static GtkObjectClass *parent_class;

static void
esv_destroy (GtkObject *object)
{
	EShellView *eshell_view = E_SHELL_VIEW (object);

	e_shell_unregister_view (eshell_view->eshell, eshell_view);

	parent_class->destroy (object);
}

static void
e_shell_view_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = esv_destroy;

	parent_class = gtk_type_class (PARENT_TYPE);
}

GtkWidget *
e_shell_view_new (EShell *eshell)
{
	EShellView *eshell_view;

	eshell_view = gtk_type_new (e_shell_view_get_type ());

	gnome_app_construct (GNOME_APP (eshell_view), "Evolution", "Evolution");
	e_shell_view_setup_menus (eshell_view);

	e_shell_register_view (eshell, eshell_view);
	
	return (GtkWidget *) eshell_view;
}

E_MAKE_TYPE (e_shell_view, "EShellView", EShellView, e_shell_view_class_init, NULL, PARENT_TYPE);

void
e_shell_view_new_folder (EShellView *esv)
{
	g_return_if_fail (esv != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (esv));
}

void
e_shell_view_new_shortcut (EShellView *esv)
{
	g_return_if_fail (esv != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (esv));
}
