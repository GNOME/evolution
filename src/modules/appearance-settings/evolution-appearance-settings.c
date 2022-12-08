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

#include <glib/gi18n-lib.h>

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include "shell/e-shell-window.h"

/* Standard GObject macros */
#define E_TYPE_APPEARANCE_SETTINGS \
	(e_appearance_settings_get_type ())
#define E_APPEARANCE_SETTINGS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_APPEARANCE_SETTINGS, EAppearanceSettings))

typedef struct _EAppearanceSettings EAppearanceSettings;
typedef struct _EAppearanceSettingsClass EAppearanceSettingsClass;

struct _EAppearanceSettings {
	EExtension parent;
};

struct _EAppearanceSettingsClass {
	EExtensionClass parent_class;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_appearance_settings_get_type (void);

G_DEFINE_DYNAMIC_TYPE (EAppearanceSettings, e_appearance_settings, E_TYPE_EXTENSION)

typedef struct _ToolbarIconSizeData {
	gint ref_count;
	EToolbarIconSize current_value;
	GtkWidget *radio_default;
	GtkWidget *radio_small;
	GtkWidget *radio_large;
} ToolbarIconSizeData;

static void
e_appearance_settings_toolbar_icon_size_changed_cb (GSettings *settings,
						    const gchar *key,
						    gpointer user_data)
{
	ToolbarIconSizeData *tisd = user_data;
	EToolbarIconSize current_value;

	g_return_if_fail (tisd != NULL);

	if (g_strcmp0 (key, "toolbar-icon-size") != 0)
		return;

	current_value = g_settings_get_enum (settings, "toolbar-icon-size");

	if (tisd->current_value == current_value)
		return;

	tisd->current_value = current_value;

	switch (tisd->current_value) {
	default:
	case E_TOOLBAR_ICON_SIZE_DEFAULT:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tisd->radio_default), TRUE);
		break;
	case E_TOOLBAR_ICON_SIZE_SMALL:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tisd->radio_small), TRUE);
		break;
	case E_TOOLBAR_ICON_SIZE_LARGE:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tisd->radio_large), TRUE);
		break;
	}
}

static ToolbarIconSizeData *
toolbar_icon_size_data_ref (ToolbarIconSizeData *tisd)
{
	g_atomic_int_inc (&tisd->ref_count);

	return tisd;
}

static void
toolbar_icon_size_data_unref (ToolbarIconSizeData *tisd)
{
	if (g_atomic_int_dec_and_test (&tisd->ref_count)) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.shell");
		g_signal_handlers_disconnect_by_func (settings, G_CALLBACK (e_appearance_settings_toolbar_icon_size_changed_cb), tisd);
		g_clear_object (&settings);

		g_free (tisd);
	}
}

static void
e_appearance_settings_toolbar_icon_size_toggled_cb (GtkWidget *radio_button,
						    gpointer user_data)
{
	ToolbarIconSizeData *tisd = user_data;
	EToolbarIconSize new_value;
	GSettings *settings;

	g_return_if_fail (tisd != NULL);
	g_return_if_fail (tisd->radio_default == radio_button ||
			  tisd->radio_small == radio_button ||
			  tisd->radio_large == radio_button);

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_button)))
		return;

	new_value = radio_button == tisd->radio_small ? E_TOOLBAR_ICON_SIZE_SMALL :
		radio_button == tisd->radio_large ? E_TOOLBAR_ICON_SIZE_LARGE :
		E_TOOLBAR_ICON_SIZE_DEFAULT;

	if (new_value == tisd->current_value)
		return;

	tisd->current_value = new_value;

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	g_settings_set_enum (settings, "toolbar-icon-size", tisd->current_value);
	g_clear_object (&settings);
}

static GtkWidget *
e_appearance_settings_page_new (EPreferencesWindow *window)
{
	PangoAttrList *bold;
	PangoAttrList *italic;
	GtkGrid *grid;
	GtkWidget *widget, *main_radio;
	GSettings *settings;
	ToolbarIconSizeData *tisd;
	gchar *filename;
	gint row = 0;

	settings = e_util_ref_settings ("org.gnome.evolution.shell");

	bold = pango_attr_list_new ();
	pango_attr_list_insert (bold, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

	italic = pango_attr_list_new ();
	pango_attr_list_insert (italic, pango_attr_style_new (PANGO_STYLE_ITALIC));

	grid = GTK_GRID (gtk_grid_new ());
	g_object_set (grid,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"border-width", 12,
		"row-spacing", 2,
		NULL);

	widget = gtk_label_new (_("Title Bar Mode"));
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"attributes", bold,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	/* Translators: This is the GNOME project name; you probably do not want to translate it, apart of changing the mnemonic */
	widget = gtk_radio_button_new_with_mnemonic (NULL, _("_GNOME"));
	g_object_set (widget,
		"margin-start", 12,
		NULL);

	main_radio = widget;

	g_settings_bind (
		settings, "use-header-bar",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	filename = g_build_filename (EVOLUTION_IMAGESDIR, "mode-with-headerbar.png", NULL);
	widget = gtk_image_new_from_file (filename);
	g_object_set (widget,
		"margin-start", 30,
		"margin-bottom", 6,
		NULL);
	g_free (filename);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	/* Translators: This belongs under "Title Bar Mode" setting, thus similar to "Title Bar Mode: Standard" */
	widget = gtk_radio_button_new_with_mnemonic (NULL, _("_Standard"));
	g_object_set (widget,
		"margin-start", 12,
		NULL);

	gtk_radio_button_join_group (GTK_RADIO_BUTTON (widget), GTK_RADIO_BUTTON (main_radio));

	g_settings_bind (
		settings, "use-header-bar",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	filename = g_build_filename (EVOLUTION_IMAGESDIR, "mode-without-headerbar.png", NULL);
	widget = gtk_image_new_from_file (filename);
	g_object_set (widget,
		"margin-start", 30,
		NULL);
	g_free (filename);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	tisd = g_new0 (ToolbarIconSizeData, 1);
	tisd->ref_count = 1;

	widget = gtk_label_new (_("Toolbar Icon Size"));
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"attributes", bold,
		"margin-top", 12,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	/* Translators: This is for "Toolbar Icon Size: Default" */
	widget = gtk_radio_button_new_with_mnemonic (NULL, _("_Default"));
	g_object_set (widget,
		"margin-start", 12,
		NULL);

	main_radio = widget;

	tisd->radio_default = widget;

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	/* Translators: This is for "Toolbar Icon Size: Small" */
	widget = gtk_radio_button_new_with_mnemonic (NULL, _("Sm_all"));
	g_object_set (widget,
		"margin-start", 12,
		NULL);

	gtk_radio_button_join_group (GTK_RADIO_BUTTON (widget), GTK_RADIO_BUTTON (main_radio));
	tisd->radio_small = widget;

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	/* Translators: This is for "Toolbar Icon Size: Large" */
	widget = gtk_radio_button_new_with_mnemonic (NULL, _("_Large"));
	g_object_set (widget,
		"margin-start", 12,
		NULL);

	gtk_radio_button_join_group (GTK_RADIO_BUTTON (widget), GTK_RADIO_BUTTON (main_radio));
	tisd->radio_large = widget;

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	g_signal_connect (settings, "changed::toolbar-icon-size",
		G_CALLBACK (e_appearance_settings_toolbar_icon_size_changed_cb), tisd);

	/* Read after the signal handler is connected */
	tisd->current_value = g_settings_get_enum (settings, "toolbar-icon-size");

	switch (tisd->current_value) {
	default:
	case E_TOOLBAR_ICON_SIZE_DEFAULT:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tisd->radio_default), TRUE);
		break;
	case E_TOOLBAR_ICON_SIZE_SMALL:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tisd->radio_small), TRUE);
		break;
	case E_TOOLBAR_ICON_SIZE_LARGE:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tisd->radio_large), TRUE);
		break;
	}

	g_signal_connect_data (tisd->radio_default, "toggled",
		G_CALLBACK (e_appearance_settings_toolbar_icon_size_toggled_cb), toolbar_icon_size_data_ref (tisd),
		(GClosureNotify) toolbar_icon_size_data_unref, G_CONNECT_DEFAULT);

	g_signal_connect_data (tisd->radio_small, "toggled",
		G_CALLBACK (e_appearance_settings_toolbar_icon_size_toggled_cb), toolbar_icon_size_data_ref (tisd),
		(GClosureNotify) toolbar_icon_size_data_unref, G_CONNECT_DEFAULT);

	g_signal_connect_data (tisd->radio_large, "toggled",
		G_CALLBACK (e_appearance_settings_toolbar_icon_size_toggled_cb), toolbar_icon_size_data_ref (tisd),
		(GClosureNotify) toolbar_icon_size_data_unref, G_CONNECT_DEFAULT);

	toolbar_icon_size_data_unref (tisd);

	widget = gtk_label_new (_("Layout"));
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"attributes", bold,
		"margin-top", 12,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	widget = gtk_check_button_new_with_mnemonic (_("Show M_enu Bar"));
	g_object_set (widget,
		"margin-start", 12,
		NULL);

	g_settings_bind (
		settings, "menubar-visible",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	widget = gtk_check_button_new_with_mnemonic (_("Show _Tool Bar"));
	g_object_set (widget,
		"margin-start", 12,
		NULL);

	g_settings_bind (
		settings, "toolbar-visible",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	widget = gtk_check_button_new_with_mnemonic (_("Show Side _Bar"));
	g_object_set (widget,
		"margin-start", 12,
		NULL);

	g_settings_bind (
		settings, "sidebar-visible",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	widget = gtk_check_button_new_with_mnemonic (_("Show Stat_us Bar"));
	g_object_set (widget,
		"margin-start", 12,
		NULL);

	g_settings_bind (
		settings, "statusbar-visible",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	widget = gtk_label_new (_("Note: Some changes will not take effect until restart"));
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"attributes", italic,
		"margin-top", 12,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	pango_attr_list_unref (bold);
	pango_attr_list_unref (italic);

	widget = GTK_WIDGET (grid);
	gtk_widget_show_all (widget);

	g_clear_object (&settings);

	return widget;
}

static void
appearance_settings_constructed (GObject *object)
{
	EExtensible *extensible;
	EAppearanceSettings *extension;
	EShellWindow *shell_window;
	EShell *shell;
	GtkWidget *preferences_window;

	extension = E_APPEARANCE_SETTINGS (object);
	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	shell_window = E_SHELL_WINDOW (extensible);
	shell = e_shell_window_get_shell (shell_window);
	preferences_window = e_shell_get_preferences_window (shell);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"page-appearance",
		"preferences-system",
		_("Appearance"),
		NULL,
		e_appearance_settings_page_new,
		950);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_appearance_settings_parent_class)->constructed (object);
}

static void
e_appearance_settings_class_init (EAppearanceSettingsClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = appearance_settings_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SHELL_WINDOW;
}

static void
e_appearance_settings_class_finalize (EAppearanceSettingsClass *class)
{
}

static void
e_appearance_settings_init (EAppearanceSettings *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_appearance_settings_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
