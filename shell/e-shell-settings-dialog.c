/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-settings-dialog.c
 *
 * Copyright (C) 2002  Ximian, Inc.
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
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-settings-dialog.h"

#include "e-corba-config-page.h"

#include <gal/util/e-util.h>

#include <bonobo/bonobo-widget.h>

#include <liboaf/liboaf.h>

#include <string.h>


#define PARENT_TYPE e_multi_config_dialog_get_type ()
static EMultiConfigDialogClass *parent_class = NULL;


static GSList *
get_language_list (void)
{
	const char *env;
	const char *p;

	env = g_getenv ("LANGUAGE");
	if (env == NULL) {
		env = g_getenv ("LANG");
		if (env == NULL)
			return NULL;
	}

	p = strchr (env, '=');
	if (p != NULL)
		return g_slist_prepend (NULL, (void *) (p + 1));
	else
		return g_slist_prepend (NULL, (void *) env);
}

static void
load_pages (EShellSettingsDialog *dialog)
{
	OAF_ServerInfoList *control_list;
	CORBA_Environment ev;
	GSList *language_list;
	int i;

	CORBA_exception_init (&ev);

	control_list = oaf_query ("defined(evolution:config_item:title)", NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION || control_list == NULL) {
		g_warning ("Cannot load configuration pages -- %s", ev._repo_id);
		CORBA_exception_free (&ev);
		return;
	}

	language_list = get_language_list ();

	for (i = 0; i < control_list->_length; i ++) {
		CORBA_Object corba_object;
		OAF_ServerInfo *info;
		const char *title;
		const char *description;
		const char *icon_path;
		GdkPixbuf *icon;

		info = & control_list->_buffer[i];

		title       = oaf_server_info_prop_lookup (info, "evolution:config_item:title", language_list);
		description = oaf_server_info_prop_lookup (info, "evolution:config_item:description", language_list);
		icon_path   = oaf_server_info_prop_lookup (info, "evolution:config_item:icon_name", NULL);

		if (icon_path == NULL) {
			icon = NULL;
		} else {
			if (g_path_is_absolute (icon_path)) {
				icon = gdk_pixbuf_new_from_file (icon_path);
			} else {
				char *real_icon_path;

				real_icon_path = g_concat_dir_and_file (EVOLUTION_IMAGES, icon_path);
				icon = gdk_pixbuf_new_from_file (real_icon_path);
				g_free (real_icon_path);
			}
		}

		corba_object = oaf_activate_from_id ((char *) info->iid, 0, NULL, &ev);
		if (ev._major == CORBA_NO_EXCEPTION)
			e_multi_config_dialog_add_page (E_MULTI_CONFIG_DIALOG (dialog),
							title, description, icon,
							E_CONFIG_PAGE (e_corba_config_page_new_from_objref (corba_object)));

		if (icon != NULL)
			gdk_pixbuf_unref (icon);
	}

	CORBA_free (control_list);

	g_slist_free (language_list);

	CORBA_exception_free (&ev);
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EShellSettingsDialog *dialog;

	dialog = E_SHELL_SETTINGS_DIALOG (object);

	/* (Really nothing to do here for now.)  */

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EShellSettingsDialog *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = impl_destroy;

	parent_class = gtk_type_class (PARENT_TYPE);
}

static void
init (EShellSettingsDialog *dialog)
{
	load_pages (dialog);

	gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 450);
}


GtkWidget *
e_shell_settings_dialog_new (void)
{
	EShellSettingsDialog *new;

	new = gtk_type_new (e_shell_settings_dialog_get_type ());

	return GTK_WIDGET (new);
}


E_MAKE_TYPE (e_shell_settings_dialog, "EShellSettingsDialog", EShellSettingsDialog,
	     class_init, init, PARENT_TYPE)

