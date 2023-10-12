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

typedef struct _AppearanceData {
	gint ref_count;

	gulong toolbar_icon_size_handler_id;
	EToolbarIconSize icon_size_value;
	GtkWidget *icon_size_radio_default;
	GtkWidget *icon_size_radio_small;
	GtkWidget *icon_size_radio_large;

	gulong prefer_symbolic_icons_handler_id;
	EPreferSymbolicIcons symbolic_icons_value;
	GtkWidget *symbolic_icons_radio_no;
	GtkWidget *symbolic_icons_radio_yes;
	GtkWidget *symbolic_icons_radio_auto;
} AppearanceData;

static AppearanceData *
appearance_data_ref (AppearanceData *ad)
{
	g_atomic_int_inc (&ad->ref_count);

	return ad;
}

static void
appearance_data_unref (AppearanceData *ad)
{
	if (g_atomic_int_dec_and_test (&ad->ref_count)) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.shell");

		if (ad->toolbar_icon_size_handler_id) {
			g_signal_handler_disconnect (settings, ad->toolbar_icon_size_handler_id);
			ad->toolbar_icon_size_handler_id = 0;
		}

		if (ad->prefer_symbolic_icons_handler_id) {
			g_signal_handler_disconnect (settings, ad->prefer_symbolic_icons_handler_id);
			ad->prefer_symbolic_icons_handler_id = 0;
		}

		g_clear_object (&settings);

		g_free (ad);
	}
}

static void
e_appearance_settings_toolbar_icon_size_changed_cb (GSettings *settings,
						    const gchar *key,
						    gpointer user_data)
{
	AppearanceData *ad = user_data;
	EToolbarIconSize current_value;

	g_return_if_fail (ad != NULL);

	if (g_strcmp0 (key, "toolbar-icon-size") != 0)
		return;

	current_value = g_settings_get_enum (settings, "toolbar-icon-size");

	if (ad->icon_size_value == current_value)
		return;

	ad->icon_size_value = current_value;

	switch (ad->icon_size_value) {
	default:
	case E_TOOLBAR_ICON_SIZE_DEFAULT:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ad->icon_size_radio_default), TRUE);
		break;
	case E_TOOLBAR_ICON_SIZE_SMALL:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ad->icon_size_radio_small), TRUE);
		break;
	case E_TOOLBAR_ICON_SIZE_LARGE:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ad->icon_size_radio_large), TRUE);
		break;
	}
}

static void
e_appearance_settings_toolbar_icon_size_toggled_cb (GtkWidget *radio_button,
						    gpointer user_data)
{
	AppearanceData *ad = user_data;
	EToolbarIconSize new_value;
	GSettings *settings;

	g_return_if_fail (ad != NULL);
	g_return_if_fail (ad->icon_size_radio_default == radio_button ||
			  ad->icon_size_radio_small == radio_button ||
			  ad->icon_size_radio_large == radio_button);

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_button)))
		return;

	new_value = radio_button == ad->icon_size_radio_small ? E_TOOLBAR_ICON_SIZE_SMALL :
		radio_button == ad->icon_size_radio_large ? E_TOOLBAR_ICON_SIZE_LARGE :
		E_TOOLBAR_ICON_SIZE_DEFAULT;

	if (new_value == ad->icon_size_value)
		return;

	ad->icon_size_value = new_value;

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	g_settings_set_enum (settings, "toolbar-icon-size", ad->icon_size_value);
	g_clear_object (&settings);
}

static void
e_appearance_settings_prefer_symbolic_icons_changed_cb (GSettings *settings,
							const gchar *key,
							gpointer user_data)
{
	AppearanceData *ad = user_data;
	EPreferSymbolicIcons current_value;

	g_return_if_fail (ad != NULL);

	if (g_strcmp0 (key, "prefer-symbolic-icons") != 0)
		return;

	current_value = g_settings_get_enum (settings, "prefer-symbolic-icons");

	if (ad->symbolic_icons_value == current_value)
		return;

	ad->symbolic_icons_value = current_value;

	switch (ad->symbolic_icons_value) {
	default:
	case E_PREFER_SYMBOLIC_ICONS_NO:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ad->symbolic_icons_radio_no), TRUE);
		break;
	case E_PREFER_SYMBOLIC_ICONS_YES:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ad->symbolic_icons_radio_yes), TRUE);
		break;
	case E_PREFER_SYMBOLIC_ICONS_AUTO:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ad->symbolic_icons_radio_auto), TRUE);
		break;
	}
}

static void
e_appearance_settings_prefer_symbolic_icons_toggled_cb (GtkWidget *radio_button,
							gpointer user_data)
{
	AppearanceData *ad = user_data;
	EPreferSymbolicIcons new_value;
	GSettings *settings;

	g_return_if_fail (ad != NULL);
	g_return_if_fail (ad->symbolic_icons_radio_no == radio_button ||
			  ad->symbolic_icons_radio_yes == radio_button ||
			  ad->symbolic_icons_radio_auto == radio_button);

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_button)))
		return;

	new_value = radio_button == ad->symbolic_icons_radio_no ? E_PREFER_SYMBOLIC_ICONS_NO :
		radio_button == ad->symbolic_icons_radio_yes ? E_PREFER_SYMBOLIC_ICONS_YES :
		E_PREFER_SYMBOLIC_ICONS_AUTO;

	if (new_value == ad->symbolic_icons_value)
		return;

	ad->symbolic_icons_value = new_value;

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	g_settings_set_enum (settings, "prefer-symbolic-icons", ad->symbolic_icons_value);
	g_clear_object (&settings);
}

static GtkWidget *
e_appearance_settings_page_new (EPreferencesWindow *window)
{
	PangoAttrList *bold;
	PangoAttrList *italic;
	GtkGrid *grid;
	GtkWidget *widget, *main_radio, *main_symbolic_icons_radio;
	GSettings *settings;
	AppearanceData *ad;
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

	widget = gtk_check_button_new_with_mnemonic (_("Use buttons with _icons only"));
	g_object_set (widget,
		"margin-start", 24,
		NULL);

	e_binding_bind_property (
		main_radio, "active",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	g_settings_bind (
		settings, "icon-only-buttons-in-header-bar",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

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

	ad = g_new0 (AppearanceData, 1);
	ad->ref_count = 1;

	widget = gtk_label_new (_("Toolbar Icon Size"));
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"attributes", bold,
		"margin-top", 12,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 1, 1);

	/* Translators: this is in a sense of "Look & Feel of the icons" */
	widget = gtk_label_new (_("Icons Look"));
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"attributes", bold,
		"margin-top", 12,
		"margin-start", 24,
		NULL);

	gtk_grid_attach (grid, widget, 1, row, 1, 1);
	row++;

	/* Translators: This is for "Toolbar Icon Size: Default" */
	widget = gtk_radio_button_new_with_mnemonic (NULL, _("_Default"));
	g_object_set (widget,
		"margin-start", 12,
		NULL);

	main_radio = widget;

	ad->icon_size_radio_default = widget;

	gtk_grid_attach (grid, widget, 0, row, 1, 1);

	/* Translators: This is for "Icons Look: Autodetect" */
	widget = gtk_radio_button_new_with_mnemonic (NULL, _("Aut_odetect"));
	g_object_set (widget,
		"margin-start", 36,
		NULL);

	main_symbolic_icons_radio = widget;

	ad->symbolic_icons_radio_auto = widget;

	gtk_grid_attach (grid, widget, 1, row, 1, 1);
	row++;

	/* Translators: This is for "Toolbar Icon Size: Small" */
	widget = gtk_radio_button_new_with_mnemonic (NULL, _("Sm_all"));
	g_object_set (widget,
		"margin-start", 12,
		NULL);

	gtk_radio_button_join_group (GTK_RADIO_BUTTON (widget), GTK_RADIO_BUTTON (main_radio));
	ad->icon_size_radio_small = widget;

	gtk_grid_attach (grid, widget, 0, row, 1, 1);

	/* Translators: This is for "Icons Look: Prefer symbolic" */
	widget = gtk_radio_button_new_with_mnemonic (NULL, _("_Prefer symbolic"));
	g_object_set (widget,
		"margin-start", 36,
		NULL);

	gtk_radio_button_join_group (GTK_RADIO_BUTTON (widget), GTK_RADIO_BUTTON (main_symbolic_icons_radio));
	ad->symbolic_icons_radio_yes = widget;

	gtk_grid_attach (grid, widget, 1, row, 1, 1);
	row++;

	/* Translators: This is for "Toolbar Icon Size: Large" */
	widget = gtk_radio_button_new_with_mnemonic (NULL, _("_Large"));
	g_object_set (widget,
		"margin-start", 12,
		NULL);

	gtk_radio_button_join_group (GTK_RADIO_BUTTON (widget), GTK_RADIO_BUTTON (main_radio));
	ad->icon_size_radio_large = widget;

	gtk_grid_attach (grid, widget, 0, row, 1, 1);

	/* Translators: This is for "Icons Look: Prefer regular" */
	widget = gtk_radio_button_new_with_mnemonic (NULL, _("Prefer _regular"));
	g_object_set (widget,
		"margin-start", 36,
		NULL);

	gtk_radio_button_join_group (GTK_RADIO_BUTTON (widget), GTK_RADIO_BUTTON (main_symbolic_icons_radio));
	ad->symbolic_icons_radio_no = widget;

	gtk_grid_attach (grid, widget, 1, row, 1, 1);
	row++;

	ad->toolbar_icon_size_handler_id = g_signal_connect (settings, "changed::toolbar-icon-size",
		G_CALLBACK (e_appearance_settings_toolbar_icon_size_changed_cb), ad);

	/* Read after the signal handler is connected */
	ad->icon_size_value = g_settings_get_enum (settings, "toolbar-icon-size");

	switch (ad->icon_size_value) {
	default:
	case E_TOOLBAR_ICON_SIZE_DEFAULT:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ad->icon_size_radio_default), TRUE);
		break;
	case E_TOOLBAR_ICON_SIZE_SMALL:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ad->icon_size_radio_small), TRUE);
		break;
	case E_TOOLBAR_ICON_SIZE_LARGE:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ad->icon_size_radio_large), TRUE);
		break;
	}

	g_signal_connect_data (ad->icon_size_radio_default, "toggled",
		G_CALLBACK (e_appearance_settings_toolbar_icon_size_toggled_cb), appearance_data_ref (ad),
		(GClosureNotify) appearance_data_unref, G_CONNECT_DEFAULT);

	g_signal_connect_data (ad->icon_size_radio_small, "toggled",
		G_CALLBACK (e_appearance_settings_toolbar_icon_size_toggled_cb), appearance_data_ref (ad),
		(GClosureNotify) appearance_data_unref, G_CONNECT_DEFAULT);

	g_signal_connect_data (ad->icon_size_radio_large, "toggled",
		G_CALLBACK (e_appearance_settings_toolbar_icon_size_toggled_cb), appearance_data_ref (ad),
		(GClosureNotify) appearance_data_unref, G_CONNECT_DEFAULT);

	ad->prefer_symbolic_icons_handler_id = g_signal_connect (settings, "changed::prefer-symbolic-icons",
		G_CALLBACK (e_appearance_settings_prefer_symbolic_icons_changed_cb), ad);

	/* Read after the signal handler is connected */
	ad->symbolic_icons_value = g_settings_get_enum (settings, "prefer-symbolic-icons");

	switch (ad->symbolic_icons_value) {
	default:
	case E_PREFER_SYMBOLIC_ICONS_NO:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ad->symbolic_icons_radio_no), TRUE);
		break;
	case E_PREFER_SYMBOLIC_ICONS_YES:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ad->symbolic_icons_radio_yes), TRUE);
		break;
	case E_PREFER_SYMBOLIC_ICONS_AUTO:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ad->symbolic_icons_radio_auto), TRUE);
		break;
	}

	g_signal_connect_data (ad->symbolic_icons_radio_no, "toggled",
		G_CALLBACK (e_appearance_settings_prefer_symbolic_icons_toggled_cb), appearance_data_ref (ad),
		(GClosureNotify) appearance_data_unref, G_CONNECT_DEFAULT);

	g_signal_connect_data (ad->symbolic_icons_radio_yes, "toggled",
		G_CALLBACK (e_appearance_settings_prefer_symbolic_icons_toggled_cb), appearance_data_ref (ad),
		(GClosureNotify) appearance_data_unref, G_CONNECT_DEFAULT);

	g_signal_connect_data (ad->symbolic_icons_radio_auto, "toggled",
		G_CALLBACK (e_appearance_settings_prefer_symbolic_icons_toggled_cb), appearance_data_ref (ad),
		(GClosureNotify) appearance_data_unref, G_CONNECT_DEFAULT);

	appearance_data_unref (ad);

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
