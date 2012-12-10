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

#include <glib/gi18n.h>

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
	ESourceRegistry *registry;
	GtkWidget *container;
	GtkWidget *itembox;
	GtkWidget *widget;
	GtkWidget *vbox;
	EShell *shell;

	shell = e_preferences_window_get_shell (window);

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	registry = e_shell_get_registry (shell);
	shell_settings = e_shell_get_shell_settings (shell);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (vbox);

	itembox = add_section (vbox, _("Date/Time Format"), FALSE);

	widget = gtk_table_new (1, 3, FALSE);
	gtk_box_pack_start (GTK_BOX (itembox), widget, TRUE, TRUE, 0);
	e_datetime_format_add_setup_widget (
		widget, 0, "addressbook", "table",
		DTFormatKindDateTime, _("_Table column:"));
	gtk_widget_show (widget);

	itembox = add_section (vbox, _("Address formatting"), FALSE);

	widget = gtk_check_button_new_with_mnemonic (
		_("_Format address according to standard of its destination country"));
	g_object_bind_property (
		shell_settings, "enable-address-formatting",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
	gtk_box_pack_start (GTK_BOX (itembox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	itembox = add_section (vbox, _("Autocompletion"), TRUE);

	widget = gtk_check_button_new_with_mnemonic (
		_("Always _show address of the autocompleted contact"));
	g_object_bind_property (
		shell_settings, "book-completion-show-address",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
	gtk_box_pack_start (GTK_BOX (itembox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (itembox), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_autocomplete_selector_new (registry);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	return vbox;
}
