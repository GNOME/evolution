/*
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <mail/em-config.h>

#include <shell/e-shell.h>
#include <libedataserver/libedataserver.h>
#include <libecal/libecal.h>
#include <modules/itip-formatter/e-conflict-search-selector.h>
#include <modules/itip-formatter/e-source-conflict-search.h>

#define CONF_KEY_DELETE "delete-processed"

GtkWidget *itip_formatter_page_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	return 0;
}

static void
delete_toggled_cb (GtkWidget *widget)
{
	GSettings *settings;
	gboolean active;

	settings = g_settings_new ("org.gnome.evolution.plugin.itip");
	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	g_settings_set_boolean (settings, CONF_KEY_DELETE, active);
	g_object_unref (settings);
}

GtkWidget *
itip_formatter_page_factory (EPlugin *ep,
                             EConfigHookItemFactoryData *hook_data)
{
	EShell *shell;
	ESourceRegistry *registry;
	GtkWidget *page;
	GtkWidget *tab_label;
	GtkWidget *frame;
	GtkWidget *frame_label;
	GtkWidget *padding_label;
	GtkWidget *hbox;
	GtkWidget *inner_vbox;
	GtkWidget *check;
	GtkWidget *label;
	GtkWidget *ess;
	GtkWidget *scrolledwin;
	gchar *str;
	GSettings *settings;

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);

	/* Create a new notebook page */
	page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);
	tab_label = gtk_label_new (_("Meeting Invitations"));
	gtk_notebook_append_page (GTK_NOTEBOOK (hook_data->parent), page, tab_label);

	/* Frame */
	frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (page), frame, FALSE, FALSE, 0);

	/* "General" */
	frame_label = gtk_label_new ("");
	str = g_strdup_printf ("<span weight=\"bold\">%s</span>", _("General"));
	gtk_label_set_markup (GTK_LABEL (frame_label), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (frame_label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (frame), frame_label, FALSE, FALSE, 0);

	/* Indent/padding */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start (GTK_BOX (frame), hbox, FALSE, TRUE, 0);
	padding_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), padding_label, FALSE, FALSE, 0);
	inner_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (hbox), inner_vbox, FALSE, FALSE, 0);

	/* Delete message after acting */
	settings = g_settings_new ("org.gnome.evolution.plugin.itip");

	check = gtk_check_button_new_with_mnemonic (_("_Delete message after acting"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), g_settings_get_boolean (settings, CONF_KEY_DELETE));
	g_signal_connect (
		check, "toggled",
		G_CALLBACK (delete_toggled_cb), NULL);
	gtk_box_pack_start (GTK_BOX (inner_vbox), check, FALSE, FALSE, 0);

	g_object_unref (settings);

	/* "Conflict searching" */
	frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (page), frame, TRUE, TRUE, 24);

	frame_label = gtk_label_new ("");
	str = g_strdup_printf ("<span weight=\"bold\">%s</span>", _("Conflict Search"));
	gtk_label_set_markup (GTK_LABEL (frame_label), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (frame_label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (frame), frame_label, FALSE, FALSE, 0);

	/* Indent/padding */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start (GTK_BOX (frame), hbox, TRUE, TRUE, 0);
	padding_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), padding_label, FALSE, FALSE, 0);
	inner_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (hbox), inner_vbox, TRUE, TRUE, 0);

	/* Source selector */
	label = gtk_label_new (_("Select the calendars to search for meeting conflicts"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (inner_vbox), label, FALSE, FALSE, 0);

	scrolledwin = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolledwin),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (scrolledwin),
		GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (inner_vbox), scrolledwin, TRUE, TRUE, 0);

	ess = e_conflict_search_selector_new (registry);
	atk_object_set_name (gtk_widget_get_accessible (ess), _("Conflict Search"));
	gtk_container_add (GTK_CONTAINER (scrolledwin), ess);

	gtk_widget_show_all (page);

	return page;
}
