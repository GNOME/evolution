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

#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include "widgets/misc/e-source-option-menu.h"
#include "select-source-dialog.h"

static void
source_selected_cb (ESourceOptionMenu *menu, ESource *selected_source, gpointer user_data)
{
	ESource **our_selection = user_data;

	if (*our_selection)
		g_object_unref (*our_selection);
	*our_selection = g_object_ref (selected_source);
}

/**
 * select_source_dialog
 *
 * Implements dialog for allowing user to select a destination source.
 */
ESource *
select_source_dialog (GtkWindow *parent, ECalSourceType obj_type)
{
	GtkWidget *dialog, *label, *source_selector;
	ESourceList *source_list;
	ESource *selected_source = NULL;
	const char *gconf_key;
	char *label_text;
	GConfClient *conf_client;

	if (obj_type == E_CAL_SOURCE_TYPE_EVENT)
		gconf_key = "/apps/evolution/calendar/sources";
	else if (obj_type == E_CAL_SOURCE_TYPE_TODO)
		gconf_key = "/apps/evolution/tasks/sources";
	else
		return NULL;

	/* create the dialog */
	dialog = gtk_dialog_new_with_buttons (_("Select source"), parent, 0,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);
	/* gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE); */

	label_text = g_strdup_printf (_("Select destination %s"),
				      obj_type == E_CAL_SOURCE_TYPE_EVENT ?
				      _("calendar") : _("task list"));
	label = gtk_label_new (label_text);
	g_free (label_text);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label, FALSE, FALSE, 6);

	conf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (conf_client, gconf_key);
	source_selector = e_source_option_menu_new (source_list);
	g_signal_connect (G_OBJECT (source_selector), "source_selected",
			  G_CALLBACK (source_selected_cb), &selected_source);
	gtk_widget_show (source_selector);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), source_selector, FALSE, FALSE, 6);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
		if (selected_source)
			g_object_unref (selected_source);
	}

	g_object_unref (conf_client);
	g_object_unref (source_list);
	gtk_widget_destroy (dialog);

	return selected_source;
}
