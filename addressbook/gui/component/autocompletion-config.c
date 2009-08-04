/*
 * e-shell-config-autocompletion.h - Configuration page for addressbook autocompletion.
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
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "autocompletion-config.h"

#include "Evolution.h"

#include <bonobo/bonobo-exception.h>

#include <libedataserver/e-source-list.h>
#include <libedataserverui/e-source-selector.h>
#include <libedataserverui/e-name-selector-entry.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-util/e-datetime-format.h"


typedef struct {
	EvolutionConfigControl *config_control;

	GtkWidget *control_widget;

	ESourceList *source_list;
	GConfClient *gconf;
} AutocompletionConfig;

static void
source_selection_changed (ESourceSelector *selector,
			  AutocompletionConfig *ac)
{
	GSList *selection;
	GSList *l;
	GSList *groups;

	/* first we clear all the completion flags from all sources */
	for (groups = e_source_list_peek_groups (ac->source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;
		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);

			e_source_set_property (source, "completion", NULL);
		}
	}

	/* then we loop over the selector's selection, setting the
	   property on those sources */
	selection = e_source_selector_get_selection (selector);
	for (l = selection; l; l = l->next) {
		e_source_set_property (E_SOURCE (l->data), "completion", "true");
	}
	e_source_selector_free_selection (selection);

	e_source_list_sync (ac->source_list, NULL); /* XXX we should pop up a dialog if this fails */
}

static void
config_control_destroy_notify (gpointer data,
			       GObject *where_the_config_control_was)
{
	AutocompletionConfig *ac = (AutocompletionConfig *) data;

	g_object_unref (ac->source_list);
	g_object_unref (ac->gconf);

	g_free (ac);
}

static void
initialize_selection (AutocompletionConfig *ac)
{
	GSList *groups;

	for (groups = e_source_list_peek_groups (ac->source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;
		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);
			const gchar *completion = e_source_get_property (source, "completion");
			if (completion && !g_ascii_strcasecmp (completion, "true"))
				e_source_selector_select_source (E_SOURCE_SELECTOR (ac->control_widget),
								 source);
		}
	}
}

static GtkWidget *
add_section (GtkWidget *vbox, const gchar *caption, gboolean expand)
{
	GtkWidget *label, *hbox, *itembox;
	gchar *txt;

	g_return_val_if_fail (vbox != NULL, NULL);
	g_return_val_if_fail (caption != NULL, NULL);

	txt = g_strconcat ("<b>", caption, "</b>", NULL);

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_markup (GTK_LABEL (label), txt);

	g_free (txt);

	/* bold caption of the section */
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 12);

	/* space on the left for the items in the section */
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (""), FALSE, FALSE, 0);

	/* itembox, here will all section items go */
	itembox = gtk_vbox_new (FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), itembox, TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, expand, expand, 0);

	return itembox;
}

static void
show_address_check_toggled_cb (GtkToggleButton *check, AutocompletionConfig *ac)
{
	g_return_if_fail (check != NULL);
	g_return_if_fail (ac != NULL);
	g_return_if_fail (ac->gconf != NULL);

	gconf_client_set_bool (ac->gconf, FORCE_SHOW_ADDRESS, gtk_toggle_button_get_active (check), NULL);
}

EvolutionConfigControl*
autocompletion_config_control_new (void)
{
	AutocompletionConfig *ac;
	CORBA_Environment ev;
	GtkWidget *scrolledwin, *vbox, *itembox, *w, *table;

	ac = g_new0 (AutocompletionConfig, 1);

	CORBA_exception_init (&ev);

	ac->gconf = gconf_client_get_default ();

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

	itembox = add_section (vbox, _("Autocompletion"), FALSE);

	w = gtk_check_button_new_with_mnemonic (_("Always _show address of the autocompleted contact"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), gconf_client_get_bool (ac->gconf, FORCE_SHOW_ADDRESS, NULL));
	g_signal_connect (w, "toggled", (GCallback)show_address_check_toggled_cb, ac);
	gtk_box_pack_start (GTK_BOX (itembox), w, FALSE, FALSE, 0);

	itembox = add_section (vbox, _("Date/Time Format"), FALSE);
	table = gtk_table_new (1, 3, FALSE);
	gtk_box_pack_start (GTK_BOX (itembox), table, TRUE, TRUE, 0);
	e_datetime_format_add_setup_widget (table, 0, "addressbook", "table",  DTFormatKindDateTime, _("Table column:"));

	itembox = add_section (vbox, _("Look up in address books"), TRUE);

	ac->source_list =  e_source_list_new_for_gconf_default ("/apps/evolution/addressbook/sources");
	/* XXX should we watch for the source list to change and
	   update it in the control?  what about our local changes? */
	/*	g_signal_connect (ac->source_list, "changed", G_CALLBACK (source_list_changed), ac); */

	scrolledwin = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwin),
					     GTK_SHADOW_IN);

	ac->control_widget = e_source_selector_new (ac->source_list);

	gtk_container_add (GTK_CONTAINER (scrolledwin), ac->control_widget);

	initialize_selection (ac);

	gtk_widget_show (ac->control_widget);
	gtk_widget_show (scrolledwin);

	gtk_widget_show_all (vbox);
	gtk_box_pack_start (GTK_BOX (itembox), scrolledwin, TRUE, TRUE, 0);

	ac->config_control = evolution_config_control_new (vbox);

	g_signal_connect (ac->control_widget, "selection_changed",
			  G_CALLBACK (source_selection_changed), ac);

	g_object_weak_ref (G_OBJECT (ac->config_control), config_control_destroy_notify, ac);

	CORBA_exception_free (&ev);

	return ac->config_control;
}

