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

#include "e-util/e-lang-utils.h"

#include <gal/util/e-util.h>

#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-exception.h>
#include <liboaf/liboaf.h>

#include <string.h>


#define PARENT_TYPE e_multi_config_dialog_get_type ()
static EMultiConfigDialogClass *parent_class = NULL;


/* Page handling.  */

struct _Page {
	char *title;
	char *description;
	GdkPixbuf *icon;
	int priority;
	EConfigPage *page_widget;
};
typedef struct _Page Page;

static Page *
page_new (const char *title,
	  const char *description,
	  GdkPixbuf *icon,
	  int priority,
	  EConfigPage *page_widget)
{
	Page *page;

	if (icon != NULL)
		gdk_pixbuf_ref (icon);

	page = g_new (Page, 1);
	page->title       = g_strdup (title);
	page->description = g_strdup (description);
	page->icon        = icon;
	page->priority    = priority;
	page->page_widget = page_widget;

	return page;
}

static void
page_free (Page *page)
{
	g_free (page->title);
	g_free (page->description);

	if (page->icon != NULL)
		gdk_pixbuf_unref (page->icon);

	g_free (page);
}

static int
compare_page_func (const void *a,
		   const void *b)
{
	const Page *page_a;
	const Page *page_b;

	page_a = (const Page *) a;
	page_b = (const Page *) b;

	if (page_a->priority == page_b->priority)
		return strcmp (page_a->title, page_b->title);

	return page_a->priority - page_b->priority;
}

static GList *
sort_page_list (GList *list)
{
	return g_list_sort (list, compare_page_func);
}

static void
load_pages (EShellSettingsDialog *dialog)
{
	OAF_ServerInfoList *control_list;
	CORBA_Environment ev;
	GSList *language_list;
	GList *page_list;
	GList *p;
	int i;

	CORBA_exception_init (&ev);

	control_list = oaf_query ("defined(evolution:config_item:title)", NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION || control_list == NULL) {
		g_warning ("Cannot load configuration pages -- %s", ev._repo_id);
		CORBA_exception_free (&ev);
		return;
	}

	language_list = e_get_language_list ();

	page_list = NULL;
	for (i = 0; i < control_list->_length; i ++) {
		CORBA_Object corba_object;
		OAF_ServerInfo *info;
		const char *title;
		const char *description;
		const char *icon_path;
		const char *priority_string;
		int priority;
		GdkPixbuf *icon;

		info = & control_list->_buffer[i];

		title       	= oaf_server_info_prop_lookup (info, "evolution:config_item:title", language_list);
		description 	= oaf_server_info_prop_lookup (info, "evolution:config_item:description", language_list);
		icon_path   	= oaf_server_info_prop_lookup (info, "evolution:config_item:icon_name", NULL);
		priority_string = oaf_server_info_prop_lookup (info, "evolution:config_item:priority", NULL);

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

		if (priority_string == NULL)
			priority = 0xffff;
		else
			priority = atoi (priority_string);

		corba_object = oaf_activate_from_id ((char *) info->iid, 0, NULL, &ev);

		if (! BONOBO_EX (&ev)) {
			Page *page;

			page = page_new (title, description, icon, priority,
					 E_CONFIG_PAGE (e_corba_config_page_new_from_objref (corba_object)));

			page_list = g_list_prepend (page_list, page);
		} else {
			g_warning ("Cannot activate %s -- %s", info->iid, BONOBO_EX_ID (&ev));
		}

		if (icon != NULL)
			gdk_pixbuf_unref (icon);
	}

	page_list = sort_page_list (page_list);
	for (p = page_list; p != NULL; p = p->next) {
		Page *page;

		page = (Page *) p->data;

		e_multi_config_dialog_add_page (E_MULTI_CONFIG_DIALOG (dialog),
						page->title,
						page->description,
						page->icon,
						page->page_widget);

		page_free (page);
	}

	g_list_free (page_list);
	e_free_language_list (language_list);
	CORBA_free (control_list);

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

	gtk_window_set_title (GTK_WINDOW (dialog), _("Evolution Settings"));
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

