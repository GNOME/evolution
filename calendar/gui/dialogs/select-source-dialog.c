/* Evolution calendar - Select source dialog
 *
 * Copyright (C) 2004 Novell, Inc.
 *
 * Author: Rodrigo Moya <rodrigo@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkstock.h>
#include <e-util/e-icon-factory.h>
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
	const char *gconf_key;
	GConfClient *conf_client;
	GList *icon_list = NULL;

	if (obj_type == E_CAL_SOURCE_TYPE_EVENT)
		gconf_key = "/apps/evolution/calendar/sources";
	else if (obj_type == E_CAL_SOURCE_TYPE_TODO)
		gconf_key = "/apps/evolution/tasks/sources";
	else
		return NULL;

	conf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (conf_client, gconf_key);

	/* create the dialog */
	dialog = e_source_selector_dialog_new (parent, source_list);

	if (obj_type == E_CAL_SOURCE_TYPE_EVENT)
		icon_list = e_icon_factory_get_icon_list ("stock_calendar");
	else if (obj_type == E_CAL_SOURCE_TYPE_TODO)
		icon_list = e_icon_factory_get_icon_list ("stock_todo");
	
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (dialog), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}
	
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		selected_source = e_source_selector_dialog_peek_primary_selection (E_SOURCE_SELECTOR_DIALOG (dialog));
		if (selected_source) {
			char *absolute_uri;

			/* set the absolute URI on the source we keep around, since the group
			   will be unrefed */
			absolute_uri = e_source_build_absolute_uri (selected_source);
			e_source_set_absolute_uri (selected_source, (const char *) absolute_uri);

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
