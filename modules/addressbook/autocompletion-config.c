/*
 * e-shell-config-autocompletion.h - Configuration page for addressbook autocompletion.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

#include "addressbook/gui/widgets/eab-config.h"

static GtkWidget *
add_section (GtkWidget *container,
             const gchar *caption,
             gboolean expand)
{
	GtkWidget *widget;
	gchar *markup;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
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

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	return widget;
}

static GtkWidget *
get_main_notebook (EConfig *config,
                   EConfigItem *item,
                   GtkWidget *parent,
                   GtkWidget *old,
                   gint position,
                   gpointer user_data)
{
	GtkWidget *notebook;

	if (old != NULL)
		return old;

	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);

	return notebook;
}

static GtkWidget *
get_general_page (EConfig *config,
                  EConfigItem *item,
                  GtkWidget *parent,
                  GtkWidget *old,
                  gint position,
                  gpointer user_data)
{
	GSettings *settings;
	ESourceRegistry *registry;
	GtkWidget *container;
	GtkWidget *itembox;
	GtkWidget *widget;
	GtkWidget *vbox;
	EShell *shell;

	if (old != NULL)
		return old;

	shell = E_SHELL (user_data);
	registry = e_shell_get_registry (shell);

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	gtk_notebook_append_page (
		GTK_NOTEBOOK (parent), vbox,
		gtk_label_new (_("General")));
	gtk_widget_show (vbox);

	itembox = add_section (vbox, _("Date/Time Format"), FALSE);

	widget = gtk_table_new (1, 3, FALSE);
	gtk_box_pack_start (GTK_BOX (itembox), widget, FALSE, FALSE, 0);
	e_datetime_format_add_setup_widget (
		widget, 0, "addressbook", "table",
		DTFormatKindDateTime, _("_Table column:"));
	gtk_widget_show (widget);

	itembox = add_section (vbox, _("Address formatting"), FALSE);

	widget = gtk_check_button_new_with_mnemonic (
		_("_Format address according to standard of its destination country"));
	g_settings_bind (
		settings, "address-formatting",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);
	gtk_box_pack_start (GTK_BOX (itembox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	itembox = add_section (vbox, _("Autocompletion"), TRUE);

	widget = gtk_check_button_new_with_mnemonic (
		_("Always _show address of the autocompleted contact"));
	g_settings_bind (
		settings, "completion-show-address",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);
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

	g_object_unref (settings);

	return vbox;
}

static EConfigItem config_items[] = {
	{ E_CONFIG_BOOK, (gchar *) "", (gchar *) "main-notebook", get_main_notebook },
	{ E_CONFIG_PAGE, (gchar *) "00.general", (gchar *) "general", get_general_page }
};

static void
config_items_free (EConfig *config,
                   GSList *items,
                   gpointer user_data)
{
	g_slist_free (items);
}

GtkWidget *
autocompletion_config_new (EPreferencesWindow *window)
{
	EShell *shell;
	EABConfig *config;
	EABConfigTargetPrefs *target;
	GSettings *settings;
	GtkWidget *vbox;
	GtkWidget *widget;
	GSList *items = NULL;
	gint ii;

	shell = e_preferences_window_get_shell (window);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);
	gtk_widget_show (vbox);

	/** @HookPoint-EABConfig: Contacts Preferences Page
	 * @Id: org.gnome.evolution.addressbook.prefs
	 * @Type: E_CONFIG_BOOK
	 * @Class: org.gnome.evolution.addressbook.config:1.0
	 * @Target: EABConfigTargetPrefs
	 *
	 * The main contacts preferences page.
	 */
	config = eab_config_new ("org.gnome.evolution.addressbook.prefs");

	for (ii = 0; ii < G_N_ELEMENTS (config_items); ii++)
		items = g_slist_prepend (items, &config_items[ii]);
	e_config_add_items (
		E_CONFIG (config), items, config_items_free, shell);

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");

	target = eab_config_target_new_prefs (config, settings);
	e_config_set_target (E_CONFIG (config), (EConfigTarget *) target);
	widget = e_config_create_widget (E_CONFIG (config));
	gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);

	g_object_unref (settings);

	return vbox;
}

