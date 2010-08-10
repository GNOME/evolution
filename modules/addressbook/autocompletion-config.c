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

#include "autocompletion-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libedataserver/e-source-list.h>
#include <libedataserverui/e-source-selector.h>
#include <libedataserverui/e-name-selector-entry.h>

#include "e-util/e-binding.h"
#include "e-util/e-datetime-format.h"

static void
source_selection_changed_cb (ESourceSelector *source_selector)
{
	ESourceList *source_list;
	GSList *selection;
	GSList *l;
	GSList *groups;

	source_list = e_source_selector_get_source_list (source_selector);

	/* first we clear all the completion flags from all sources */
	for (groups = e_source_list_peek_groups (source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;

		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);

			e_source_set_property (source, "completion", NULL);
		}
	}

	/* then we loop over the selector's selection, setting the
	   property on those sources */
	selection = e_source_selector_get_selection (source_selector);
	for (l = selection; l; l = l->next) {
		ESource *source = E_SOURCE (l->data);

		e_source_set_property (source, "completion", "true");
	}
	e_source_selector_free_selection (selection);

	/* XXX we should pop up a dialog if this fails */
	e_source_list_sync (source_list, NULL);
}

static void
initialize_selection (ESourceSelector *source_selector)
{
	ESourceList *source_list;
	GSList *groups;

	source_list = e_source_selector_get_source_list (source_selector);

	for (groups = e_source_list_peek_groups (source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;

		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);
			const gchar *completion;

			completion = e_source_get_property (source, "completion");
			if (completion && !g_ascii_strcasecmp (completion, "true"))
				e_source_selector_select_source (source_selector, source);
		}
	}
}

static GtkWidget *
add_section (GtkWidget *container,
             const gchar *caption,
             gboolean expand)
{
	GtkWidget *widget;
	gchar *markup;

	widget = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, expand, expand, 0);
	gtk_widget_show (widget);

	container = widget;

	markup = g_markup_printf_escaped ("<b>%s</b>", caption);
	widget = gtk_label_new (markup);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	widget = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (widget), 0, 0, 12, 0);
	gtk_box_pack_start (GTK_BOX (container), widget, expand, expand, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	return widget;
}

GtkWidget *
autocompletion_config_new (EPreferencesWindow *window)
{
	EShellSettings *shell_settings;
	ESourceList *source_list;
	GtkWidget *scrolled_window;
	GtkWidget *source_selector;
	GtkWidget *itembox;
	GtkWidget *widget;
	GtkWidget *vbox;
	EShell *shell;

	shell = e_preferences_window_get_shell (window);

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	shell_settings = e_shell_get_shell_settings (shell);

	source_list = e_source_list_new_for_gconf_default (
		"/apps/evolution/addressbook/sources");

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	gtk_widget_show (vbox);

	itembox = add_section (vbox, _("Date/Time Format"), FALSE);

	widget = gtk_table_new (1, 3, FALSE);
	gtk_box_pack_start (GTK_BOX (itembox), widget, TRUE, TRUE, 0);
	e_datetime_format_add_setup_widget (
		widget, 0, "addressbook", "table",
		DTFormatKindDateTime, _("_Table column:"));
	gtk_widget_show (widget);

	itembox = add_section (vbox, _("Autocompletion"), TRUE);

	widget = gtk_check_button_new_with_mnemonic (
		_("Always _show address of the autocompleted contact"));
	e_mutual_binding_new (
		shell_settings, "book-completion-show-address",
		widget, "active");
	gtk_box_pack_start (GTK_BOX (itembox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolled_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_widget_show (scrolled_window);

	source_selector = e_source_selector_new (source_list);
	initialize_selection (E_SOURCE_SELECTOR (source_selector));
	g_signal_connect (
		source_selector, "selection_changed",
		G_CALLBACK (source_selection_changed_cb), NULL);
	gtk_container_add (GTK_CONTAINER (scrolled_window), source_selector);
	gtk_widget_show (source_selector);

	gtk_box_pack_start (GTK_BOX (itembox), scrolled_window, TRUE, TRUE, 0);

	return vbox;
}
