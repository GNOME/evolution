/*
 * E-shell.c: Shell object for Evolution
 *
 * Authors:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 1999 Miguel de Icaza
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include "Shell.h"
#include "e-util/e-util.h"
#include "e-shell.h"

#define PARENT_TYPE (gnome_object_get_type ())

static GnomeObjectClass *e_shell_parent_class;
POA_GNOME_Evolution_Shell__vepv eshell_vepv;

GtkType e_shell_get_type (void);

static POA_GNOME_Evolution_Shell__epv *
e_shell_get_epv (void)
{
	POA_GNOME_Evolution_Shell__epv *epv;

	epv = g_new0 (POA_GNOME_Evolution_Shell__epv, 1);

	return epv;
}

static void
init_e_shell_corba_class (void)
{
	eshell_vepv.GNOME_Unknown_epv = gnome_object_get_epv ();
	eshell_vepv.GNOME_Evolution_Shell_epv = e_shell_get_epv ();
}

static void
e_shell_destroy (GtkObject *object)
{
	EShell *eshell = E_SHELL (object);

	if (eshell->base_uri)
		g_free (eshell->base_uri);

	GTK_OBJECT_CLASS (e_shell_parent_class)->destroy (object);
}

static void
e_shell_class_init (GtkObjectClass *object_class)
{
	e_shell_parent_class = gtk_type_class (PARENT_TYPE);
	init_e_shell_corba_class ();

	object_class->destroy = e_shell_destroy;
}

static void
e_shell_init (GtkObject *object)
{
}

void
e_shell_set_base_uri (EShell *eshell, const char *base_uri)
{
	g_return_if_fail (eshell != NULL);
	g_return_if_fail (!E_IS_SHELL (eshell));
	g_return_if_fail (base_uri != NULL);

	if (eshell->base_uri)
		g_free (eshell->base_uri);
	
	eshell->base_uri = g_strdup (base_uri);
}

const char *
e_shell_get_base_uri (EShell *eshell)
{
	g_return_val_if_fail (eshell != NULL, NULL);
	g_return_val_if_fail (!E_IS_SHELL (eshell), NULL);

	return eshell->base_uri;
}

EShell *
e_shell_new (const char *base_uri)
{
	EShell *eshell;

	g_return_val_if_fail (base_uri  != NULL, NULL);
	
	eshell = gtk_type_new (e_shell_get_type ());
	e_shell_set_base_uri (eshell, base_uri);

	return eshell;
}

E_MAKE_TYPE (e_shell, "EShell", EShell, e_shell_class_init, e_shell_init, PARENT_TYPE);



	
