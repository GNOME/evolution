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
#include <gtk/gtkbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkstock.h>
#include <e-util/e-icon-factory.h>
#include <libedataserverui/e-source-selector.h>
#include "select-source-dialog.h"

static void
row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path,
		  GtkTreeViewColumn *column, GtkWidget *dialog)
{
        gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
primary_selection_changed_cb (ESourceSelector *selector, gpointer user_data)
{
	ESource **our_selection = user_data;

	if (*our_selection)
		g_object_unref (*our_selection);
	*our_selection = e_source_selector_peek_primary_selection (selector);
	if (*our_selection) {
		g_object_ref (*our_selection);
		gtk_dialog_set_response_sensitive (
			GTK_DIALOG (gtk_widget_get_toplevel (GTK_WIDGET (selector))), GTK_RESPONSE_OK, TRUE);
	} else {
		gtk_dialog_set_response_sensitive (
			GTK_DIALOG (gtk_widget_get_toplevel (GTK_WIDGET (selector))), GTK_RESPONSE_OK, FALSE);
	}
}

/**
 * select_source_dialog
 *
 * Implements dialog for allowing user to select a destination source.
 */
ESource *
select_source_dialog (GtkWindow *parent, ECalSourceType obj_type)
{
	GtkWidget *dialog, *label, *scroll, *source_selector;
	GtkWidget *vbox, *hbox, *spacer;
	ESourceList *source_list;
	ESource *selected_source = NULL;
	const char *gconf_key;
	char *label_text;
	GConfClient *conf_client;
	GList *icon_list = NULL;

	if (obj_type == E_CAL_SOURCE_TYPE_EVENT)
		gconf_key = "/apps/evolution/calendar/sources";
	else if (obj_type == E_CAL_SOURCE_TYPE_TODO)
		gconf_key = "/apps/evolution/tasks/sources";
	else
		return NULL;

	/* create the dialog */
	dialog = gtk_dialog_new ();
	gtk_window_set_title (GTK_WINDOW (dialog), _("Select destination"));
	gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      GTK_WINDOW (parent));
	gtk_window_set_default_size (GTK_WINDOW (dialog), 320, 240);

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_widget_ensure_style (dialog);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 12);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), vbox);
	gtk_widget_show (vbox);

	label_text = g_strdup_printf ("<b>%s %s</b>", _("_Destination"),
				      obj_type == E_CAL_SOURCE_TYPE_EVENT ?
				      _("Calendar") : _("Task List"));
	label = gtk_label_new_with_mnemonic (label_text);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	g_free (label_text);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show (hbox);

	spacer = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), spacer, FALSE, FALSE, 0);
	gtk_widget_show (spacer);

	conf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (conf_client, gconf_key);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_widget_show (scroll);
	source_selector = e_source_selector_new (source_list);
	e_source_selector_show_selection (E_SOURCE_SELECTOR (source_selector), FALSE);

	g_signal_connect (G_OBJECT (source_selector), "row_activated",
			  G_CALLBACK (row_activated_cb), dialog);

	g_signal_connect (G_OBJECT (source_selector), "primary_selection_changed",
			  G_CALLBACK (primary_selection_changed_cb), &selected_source);
	gtk_widget_show (source_selector);
	gtk_container_add (GTK_CONTAINER (scroll), source_selector);
	gtk_box_pack_start (GTK_BOX (hbox), scroll, TRUE, TRUE, 0);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), source_selector);

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
		if (selected_source) {
			char *absolute_uri;

			/* set the absolute URI on the source we keep around, since the group
			   will be unrefed */
			absolute_uri = e_source_build_absolute_uri (selected_source);
			e_source_set_absolute_uri (selected_source, (const char *) absolute_uri);

			g_free (absolute_uri);
		}
	} else {
		if (selected_source)
			g_object_unref (selected_source);
		selected_source = NULL;
	}

	g_object_unref (conf_client);
	g_object_unref (source_list);
	gtk_widget_destroy (dialog);

	return selected_source;
}
