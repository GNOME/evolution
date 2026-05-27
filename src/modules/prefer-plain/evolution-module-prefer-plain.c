/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include "e-mail-parser-prefer-plain.h"
#include "e-mail-display-popup-prefer-plain.h"

#include <gmodule.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libebackend/libebackend.h>
#include <e-util/e-util.h>
#include <shell/e-shell-window.h>

void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);
const gchar * g_module_check_init (GModule *module);

static struct {
	const gchar *key;
	const gchar *label;
	const gchar *description;
} prefer_plain_modes[] = {
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

static gboolean
prefer_plain_mode_get_mapping (GValue *value,
                               GVariant *variant,
                               gpointer user_data)
{
	const gchar *key = g_variant_get_string (variant, NULL);
	gint i;

	if (key) {
		for (i = 0; i < G_N_ELEMENTS (prefer_plain_modes); i++) {
			if (!strcmp (prefer_plain_modes[i].key, key)) {
				g_value_set_int (value, i);
				return TRUE;
			}
		}
	}

	g_value_set_int (value, 0);
	return TRUE;
}

static GVariant *
prefer_plain_mode_set_mapping (const GValue *value,
                               const GVariantType *expected_type,
                               gpointer user_data)
{
	return g_variant_new_string (prefer_plain_modes[g_value_get_int (value)].key);
}

static void
prefer_plain_dropdown_changed (GtkComboBox *dropdown,
                               GtkWidget *info_label)
{
	gint mode = gtk_combo_box_get_active (dropdown);
	gchar *str;

	if (mode < 0 || mode >= (gint) G_N_ELEMENTS (prefer_plain_modes))
		mode = 0;

	str = g_strconcat ("<i>", _(prefer_plain_modes[mode].description), "</i>", NULL);
	gtk_label_set_markup (GTK_LABEL (info_label), str);
	g_free (str);
}

static GtkWidget *
prefer_plain_create_config_page (EPreferencesWindow *window)
{
	GSettings *settings;
	GtkGrid *grid;
	GtkWidget *check, *dropdown_label, *info;
	GtkComboBox *dropdown;
	GtkCellRenderer *cell;
	GtkListStore *store;
	GtkTreeIter iter;
	guint i;

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.prefer-plain");

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 6);
	gtk_grid_set_column_spacing (grid, 12);
	gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
	gtk_widget_set_margin_end (GTK_WIDGET (grid), 12);
	gtk_widget_set_margin_top (GTK_WIDGET (grid), 12);
	gtk_widget_set_margin_bottom (GTK_WIDGET (grid), 12);

	check = gtk_check_button_new_with_mnemonic (_("Show s_uppressed HTML parts as attachments"));
	gtk_widget_set_halign (check, GTK_ALIGN_START);
	gtk_widget_show (check);
	gtk_grid_attach (grid, check, 0, 0, 2, 1);

	dropdown_label = gtk_label_new_with_mnemonic (_("HTML _Mode"));
	gtk_widget_set_halign (dropdown_label, GTK_ALIGN_END);
	gtk_widget_show (dropdown_label);
	gtk_grid_attach (grid, dropdown_label, 0, 1, 1, 1);

	dropdown = GTK_COMBO_BOX (gtk_combo_box_new ());
	cell = gtk_cell_renderer_text_new ();
	store = gtk_list_store_new (1, G_TYPE_STRING);
	for (i = 0; i < G_N_ELEMENTS (prefer_plain_modes); i++) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, _(prefer_plain_modes[i].label), -1);
	}
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (dropdown), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (dropdown), cell, "text", 0, NULL);
	gtk_combo_box_set_model (dropdown, GTK_TREE_MODEL (store));
	g_object_unref (store);
	gtk_widget_set_hexpand (GTK_WIDGET (dropdown), TRUE);
	gtk_widget_show (GTK_WIDGET (dropdown));
	gtk_label_set_mnemonic_widget (GTK_LABEL (dropdown_label), GTK_WIDGET (dropdown));
	gtk_grid_attach (grid, GTK_WIDGET (dropdown), 1, 1, 1, 1);

	info = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (info), 0);
	gtk_label_set_line_wrap (GTK_LABEL (info), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (info), 40);
	gtk_label_set_max_width_chars (GTK_LABEL (info), 60);
	gtk_widget_show (info);
	gtk_grid_attach (grid, info, 1, 2, 1, 1);

	g_settings_bind (settings, "show-suppressed",
		check, "active",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind_with_mapping (settings, "mode",
		dropdown, "active",
		G_SETTINGS_BIND_DEFAULT,
		prefer_plain_mode_get_mapping,
		prefer_plain_mode_set_mapping,
		NULL, NULL);

	/* Update the description label; the binding above sets the active item. */
	prefer_plain_dropdown_changed (dropdown, info);
	g_signal_connect (dropdown, "changed",
		G_CALLBACK (prefer_plain_dropdown_changed), info);

	gtk_widget_show (GTK_WIDGET (grid));

	g_object_unref (settings);

	return GTK_WIDGET (grid);
}

typedef struct _EPreferPlainShellWindow EPreferPlainShellWindow;
typedef struct _EPreferPlainShellWindowClass EPreferPlainShellWindowClass;
struct _EPreferPlainShellWindow { EExtension parent; };
struct _EPreferPlainShellWindowClass { EExtensionClass parent_class; };

GType e_prefer_plain_shell_window_get_type (void);
G_DEFINE_DYNAMIC_TYPE (EPreferPlainShellWindow, e_prefer_plain_shell_window, E_TYPE_EXTENSION)

static void
e_prefer_plain_shell_window_constructed (GObject *object)
{
	EShellWindow *shell_window;
	EShell *shell;
	GtkWidget *preferences_window;

	G_OBJECT_CLASS (e_prefer_plain_shell_window_parent_class)->constructed (object);

	shell_window = E_SHELL_WINDOW (e_extension_get_extensible (E_EXTENSION (object)));
	shell = e_shell_window_get_shell (shell_window);
	preferences_window = e_shell_get_preferences_window (shell);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"prefer-plain",
		"text-x-generic",
		_("Plain Text Mode"),
		NULL,
		(EPreferencesWindowCreatePageFn) prefer_plain_create_config_page,
		500);
}

static void
e_prefer_plain_shell_window_class_init (EPreferPlainShellWindowClass *class)
{
	G_OBJECT_CLASS (class)->constructed = e_prefer_plain_shell_window_constructed;
	E_EXTENSION_CLASS (class)->extensible_type = E_TYPE_SHELL_WINDOW;
}

static void
e_prefer_plain_shell_window_class_finalize (EPreferPlainShellWindowClass *class)
{
}

static void
e_prefer_plain_shell_window_init (EPreferPlainShellWindow *self)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_mail_parser_prefer_plain_type_register (type_module);
	e_mail_display_popup_prefer_plain_type_register (type_module);
	e_prefer_plain_shell_window_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

G_MODULE_EXPORT const gchar *
g_module_check_init (GModule *module)
{
	/* FIXME Until mail is split into a module library and a
	 *       reusable shared library, prevent the module from
	 *       being unloaded.  Unloading the module resets all
	 *       static variables, which screws up foo_get_type()
	 *       functions among other things. */
	g_module_make_resident (module);

	return NULL;
}
