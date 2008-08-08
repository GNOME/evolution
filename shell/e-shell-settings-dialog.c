/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-settings-dialog.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "e-shell-settings-dialog.h"

#include "e-corba-config-page.h"

#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-exception.h>

#include <bonobo-activation/bonobo-activation.h>

#include <stdlib.h>
#include <string.h>

#define E_SHELL_SETTINGS_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_SETTINGS_DIALOG, EShellSettingsDialogPrivate))

struct _EShellSettingsDialogPrivate {
	GHashTable *types;
};

static gpointer parent_class;

/* Page handling.  */

struct _Page {
	char *caption;
	char *icon_name;
	Bonobo_ActivationProperty *type;
	int priority;
	EConfigPage *page_widget;
};
typedef struct _Page Page;

static Page *
page_new (const char *caption,
	  const char *icon_name,
	  Bonobo_ActivationProperty *type,
	  int priority,
	  EConfigPage *page_widget)
{
	Page *page;

	page = g_new (Page, 1);
	page->caption     = g_strdup (caption);
	page->icon_name   = g_strdup (icon_name);;
	page->type        = type;
	page->priority    = priority;
	page->page_widget = page_widget;

	return page;
}

static void
page_free (Page *page)
{
	g_free (page->caption);
	g_free (page->icon_name);
	g_free (page);
}

static gint
compare_page_func (const Page *a,
		   const Page *b)
{
	if (a->priority == b->priority)
		return strcmp (a->caption, b->caption);

	return a->priority - b->priority;
}

static GList *
sort_page_list (GList *list)
{
	return g_list_sort (list, (GCompareFunc) compare_page_func);
}

static void
load_pages (EShellSettingsDialog *dialog)
{
	EShellSettingsDialogPrivate *priv;
	Bonobo_ServerInfoList *control_list;
	const gchar * const *language_names;
	CORBA_Environment ev;
	GSList *languages = NULL;
	GList *page_list;
	GList *p;
	int i, j;

	priv = dialog->priv;

	CORBA_exception_init (&ev);

	control_list = bonobo_activation_query ("repo_ids.has('IDL:GNOME/Evolution/ConfigControl:" BASE_VERSION "')", NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION || control_list == NULL) {
		g_warning ("Cannot load configuration pages -- %s", BONOBO_EX_REPOID (&ev));
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	language_names = g_get_language_names ();
	while (*language_names != NULL)
		languages = g_slist_append (languages, (gpointer)(*language_names++));

	page_list = NULL;
	for (i = 0; i < control_list->_length; i ++) {
		CORBA_Object corba_object;
		Bonobo_ServerInfo *info;
		const char *caption;
		const char *icon_name;
		const char *priority_string;
		Bonobo_ActivationProperty *type;
		int priority;

		CORBA_exception_init (&ev);

		info = & control_list->_buffer[i];

		caption       	= bonobo_server_info_prop_lookup (info, "evolution2:config_item:title", languages);
		icon_name   	= bonobo_server_info_prop_lookup (info, "evolution2:config_item:icon_name", NULL);
		type            = bonobo_server_info_prop_find   (info, "evolution2:config_item:type");
		priority_string = bonobo_server_info_prop_lookup (info, "evolution2:config_item:priority", NULL);

		if (type != NULL && type->v._d != Bonobo_ACTIVATION_P_STRINGV)
			type = NULL;
		if (priority_string == NULL)
			priority = 0xffff;
		else
			priority = atoi (priority_string);

		corba_object = bonobo_activation_activate_from_id ((char *) info->iid, 0, NULL, &ev);

		if (! BONOBO_EX (&ev)) {
			Page *page;

			page = page_new (caption, icon_name, type, priority,
					 E_CONFIG_PAGE (e_corba_config_page_new_from_objref (corba_object)));

			page_list = g_list_prepend (page_list, page);
		} else {
			char *bonobo_ex_text = bonobo_exception_get_text (&ev);
			g_warning ("Cannot activate %s -- %s", info->iid, bonobo_ex_text);
			g_free (bonobo_ex_text);
		}

		CORBA_exception_free (&ev);
	}
	g_slist_free(languages);

	page_list = sort_page_list (page_list);
	for (p = page_list, i = 0; p != NULL; p = p->next, i++) {
		Page *page;

		page = (Page *) p->data;

		e_multi_config_dialog_add_page (E_MULTI_CONFIG_DIALOG (dialog),
						page->caption,
						page->icon_name,
						page->page_widget);

		if (page->type != NULL) {
			Bonobo_StringList list = page->type->v._u.value_stringv;

			for (j = 0; j < list._length; j++) {
				if (g_hash_table_lookup (priv->types, list._buffer[j]) == NULL)
					g_hash_table_insert (priv->types, g_strdup (list._buffer[j]),
							     GINT_TO_POINTER (i));
			}
		}


		page_free (page);
	}

	g_list_free (page_list);
	CORBA_free (control_list);
}

static void
shell_settings_dialog_finalize (GObject *object)
{
	EShellSettingsDialogPrivate *priv;

	priv = E_SHELL_SETTINGS_DIALOG_GET_PRIVATE (object);

	g_hash_table_destroy (priv->types);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
shell_settings_dialog_class_init (EShellSettingsDialogClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellSettingsDialogPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = shell_settings_dialog_finalize;
}

static void
shell_settings_dialog_init (EShellSettingsDialog *dialog)
{
	dialog->priv = E_SHELL_SETTINGS_DIALOG_GET_PRIVATE (dialog);

	dialog->priv->types = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	load_pages (dialog);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Evolution Preferences"));
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
}

GType
e_shell_settings_dialog_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EShellSettingsDialogClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) shell_settings_dialog_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellSettingsDialog),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_settings_dialog_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_MULTI_CONFIG_DIALOG, "EShellSettingsDialog",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_shell_settings_dialog_new (void)
{
	return g_object_new (E_TYPE_SHELL_SETTINGS_DIALOG, NULL);
}

void
e_shell_settings_dialog_show_type (EShellSettingsDialog *dialog,
                                   const gchar *type)
{
	EShellSettingsDialogPrivate *priv;
	gpointer key, value;
	int page;

	g_return_if_fail (E_IS_SHELL_SETTINGS_DIALOG (dialog));
	g_return_if_fail (type != NULL);

	priv = dialog->priv;

	if (!g_hash_table_lookup_extended (priv->types, type, &key, &value)) {
		char *slash, *supertype;

		slash = strchr (type, '/');
		if (slash) {
			supertype = g_strndup (type, slash - type);
			value = g_hash_table_lookup (priv->types, type);
			g_free (supertype);
		} else
			value = NULL;
	}

	page = GPOINTER_TO_INT (value);
	e_multi_config_dialog_show_page (E_MULTI_CONFIG_DIALOG (dialog), page);
}
