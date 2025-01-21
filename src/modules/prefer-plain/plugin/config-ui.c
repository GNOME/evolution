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

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <mail/em-config.h>

GtkWidget *prefer_plain_page_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data);

static GSettings *epp_settings = NULL;
static gint epp_mode = -1;
static gboolean epp_show_suppressed = TRUE;

static struct {
	const gchar *key;
	const gchar *label;
	const gchar *description;
} epp_options[] = {
	{ "normal",
	  N_("Show HTML if present"),
	  N_("Let Evolution choose the best part to show.") },

	{ "prefer_plain",
	  N_("Show plain text if present"),
	  N_("Show plain text part, if present, otherwise "
	     "let Evolution choose the best part to show.") },

	{ "prefer_source",
	  N_("Show plain text if present, or HTML source"),
	  N_("Show plain text part, if present, otherwise "
	     "the HTML part source.") },

	{ "only_plain",
	  N_("Only ever show plain text"),
	  N_("Always show plain text part and make attachments "
	     "from other parts, if requested.") },
};

static void
update_info_label (GtkWidget *info_label,
                   guint mode)
{
	gchar *str = g_strconcat ("<i>", _(epp_options[mode >= G_N_ELEMENTS (epp_options) ? 0 : mode].description), "</i>", NULL);

	gtk_label_set_markup (GTK_LABEL (info_label), str);

	g_free (str);
}

static void
epp_mode_changed (GtkComboBox *dropdown,
                  GtkWidget *info_label)
{
	epp_mode = gtk_combo_box_get_active (dropdown);
	if (epp_mode >= G_N_ELEMENTS (epp_options))
		epp_mode = 0;

	g_settings_set_string (epp_settings, "mode", epp_options[epp_mode].key);
	update_info_label (info_label, epp_mode);
}

static void
epp_show_suppressed_toggled (GtkToggleButton *check,
                             gpointer data)
{
	g_return_if_fail (check != NULL);

	epp_show_suppressed = gtk_toggle_button_get_active (check);
	g_settings_set_boolean (epp_settings, "show-suppressed", epp_show_suppressed);
}

GtkWidget *
prefer_plain_page_factory (EPlugin *epl,
                           struct _EConfigHookItemFactoryData *data)
{
	GtkComboBox *dropdown;
	GtkCellRenderer *cell;
	GtkListStore *store;
	GtkWidget *dropdown_label, *info, *check;
	guint i;
	GtkTreeIter iter;

	if (data->old)
		return data->old;

	check = gtk_check_button_new_with_mnemonic (_("Show s_uppressed HTML parts as attachments"));
	gtk_widget_set_halign (check, GTK_ALIGN_START);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), epp_show_suppressed);
	gtk_widget_show (check);
	g_signal_connect (
		check, "toggled",
		G_CALLBACK (epp_show_suppressed_toggled), NULL);

	dropdown = (GtkComboBox *) gtk_combo_box_new ();
	cell = gtk_cell_renderer_text_new ();
	store = gtk_list_store_new (1, G_TYPE_STRING);
	for (i = 0; i < G_N_ELEMENTS (epp_options); i++) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, _(epp_options[i].label), -1);
	}

	gtk_cell_layout_pack_start ((GtkCellLayout *) dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes ((GtkCellLayout *) dropdown, cell, "text", 0, NULL);
	gtk_combo_box_set_model (dropdown, (GtkTreeModel *) store);
	/*gtk_combo_box_set_active(dropdown, -1);*/
	gtk_combo_box_set_active (dropdown, epp_mode);
	gtk_widget_set_hexpand (GTK_WIDGET (dropdown), TRUE);
	gtk_widget_show ((GtkWidget *) dropdown);

	dropdown_label = gtk_label_new_with_mnemonic (_("HTML _Mode"));
	gtk_widget_show (dropdown_label);
	gtk_label_set_mnemonic_widget (GTK_LABEL (dropdown_label), (GtkWidget *) dropdown);

	info = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (info), 0);
	gtk_label_set_line_wrap (GTK_LABEL (info), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (info), 40);
	gtk_label_set_max_width_chars (GTK_LABEL (info), 60);

	gtk_widget_show (info);
	update_info_label (info, epp_mode);

	g_signal_connect (
		dropdown, "changed",
		G_CALLBACK (epp_mode_changed), info);

	gtk_grid_attach_next_to (GTK_GRID (data->parent), check, NULL, GTK_POS_BOTTOM, 2, 1);
	gtk_container_child_get (GTK_CONTAINER (data->parent), check, "top-attach", &i, NULL);
	gtk_grid_attach (GTK_GRID (data->parent), dropdown_label, 0, i + 1, 1, 1);
	gtk_grid_attach (GTK_GRID (data->parent), (GtkWidget *) dropdown, 1, i + 1, 1, 1);
	gtk_grid_attach (GTK_GRID (data->parent), info, 1, i + 2, 1, 1);

	/* since this isn't dynamic, we don't need to track each item */

	return (GtkWidget *) dropdown;
}

gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	gchar *key;
	gint i;

	if (epp_settings || epp_mode != -1)
		return 0;

	if (enable) {

		epp_settings = e_util_ref_settings ("org.gnome.evolution.plugin.prefer-plain");
		key = g_settings_get_string (epp_settings, "mode");
		if (key) {
			for (i = 0; i < G_N_ELEMENTS (epp_options); i++) {
				if (!strcmp (epp_options[i].key, key)) {
					epp_mode = i;
					break;
				}
			}
			g_free (key);
		} else {
			epp_mode = 0;
		}

		epp_show_suppressed = g_settings_get_boolean (epp_settings, "show-suppressed");
	} else {
		g_clear_object (&epp_settings);
	}

	return 0;
}
