/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <libedataserverui/e-source-selector-dialog.h>
#include "select-source-dialog.h"

/**
 * select_source_dialog
 *
 * Implements dialog for allowing user to select a destination source.
 */
ESource *
select_source_dialog (GtkWindow *parent, ECalSourceType obj_type)
{
	GtkWidget *dialog;
	ESourceList *source_list;
	ESource *selected_source = NULL;
	const gchar *gconf_key;
	GConfClient *conf_client;
	const gchar *icon_name = NULL;

	if (obj_type == E_CAL_SOURCE_TYPE_EVENT)
		gconf_key = "/apps/evolution/calendar/sources";
	else if (obj_type == E_CAL_SOURCE_TYPE_TODO)
		gconf_key = "/apps/evolution/tasks/sources";
        else if (obj_type == E_CAL_SOURCE_TYPE_JOURNAL)
                gconf_key = "/apps/evolution/memos/sources";
	else
		return NULL;

	conf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (conf_client, gconf_key);

	/* create the dialog */
	dialog = e_source_selector_dialog_new (parent, source_list);

	if (obj_type == E_CAL_SOURCE_TYPE_EVENT)
		icon_name = "x-office-calendar";
	else if (obj_type == E_CAL_SOURCE_TYPE_TODO)
		icon_name = "stock_todo";
        else if (obj_type == E_CAL_SOURCE_TYPE_JOURNAL)
                icon_name = "stock_journal";

	if (icon_name)
		gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		selected_source = e_source_selector_dialog_peek_primary_selection (E_SOURCE_SELECTOR_DIALOG (dialog));
		if (selected_source) {
			gchar *absolute_uri;

			/* set the absolute URI on the source we keep around, since the group
			   will be unrefed */
			absolute_uri = e_source_build_absolute_uri (selected_source);
			e_source_set_absolute_uri (selected_source, (const gchar *) absolute_uri);

			g_object_ref (selected_source);
			g_free (absolute_uri);
		}
	} else
		selected_source = NULL;

	g_object_unref (conf_client);
	g_object_unref (source_list);
	gtk_widget_destroy (dialog);

	return selected_source;
}
