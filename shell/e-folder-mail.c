/*
 * e-folder-mail.c: Mail folder
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include "e-util/e-util.h"
#include "e-folder-mail.h"

#define PARENT_TYPE e_folder_get_type ()

static void
e_folder_mail_init (GtkObject *object)
{
}

E_MAKE_TYPE (e_folder_mail, "EFolderMail", EFolderMail, e_folder_mail_class_init, e_folder_mail_init, PARENT_TYPE)

EFolder *
e_folder_mail_new (const char *uri, const char *name, const char *desc,
		   const char *home_page, const char *view_name)
{
	EFolderMail *efm = gtk_type_new (e_folder_mail_get_type ());

	e_folder_construct (E_FOLDER (efm), E_FOLDER_MAIL, uri, name, desc, home_page, view_name);

	return E_FOLDER (efm);
}


