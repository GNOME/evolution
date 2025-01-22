/*
 * evolution-cal-config-local.c
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
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>
#include <libedataserver/libedataserver.h>

#include "e-util/e-util.h"
#include "calendar/gui/itip-utils.h"

typedef ESourceConfigBackend ECalConfigLocal;
typedef ESourceConfigBackendClass ECalConfigLocalClass;

typedef struct _Context Context;

struct _Context {
	GtkWidget *custom_file_checkbox;
	GtkWidget *custom_file_chooser;
	GtkWidget *writable_checkbox;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_cal_config_local_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	ECalConfigLocal,
	e_cal_config_local,
	E_TYPE_SOURCE_CONFIG_BACKEND)

static void
cal_config_local_fill_addresses (ESourceRegistry *registry,
				 GtkComboBoxText *combo_box)
{
	gchar **address_strings;
	gint ii;

	address_strings = itip_get_user_identities (registry);

	for (ii = 0; address_strings && address_strings[ii]; ii++) {
		gtk_combo_box_text_append_text (combo_box, address_strings[ii]);
	}

	g_strfreev (address_strings);
}

static void
cal_config_local_context_free (Context *context)
{
	g_object_unref (context->custom_file_checkbox);
	g_object_unref (context->custom_file_chooser);
	g_object_unref (context->writable_checkbox);

	g_slice_free (Context, context);
}

static gboolean
cal_config_local_active_to_custom_file (GBinding *binding,
                                        const GValue *source_value,
                                        GValue *target_value,
                                        gpointer user_data)
{
	Context *context = user_data;
	GtkFileChooser *file_chooser;
	GFile *file = NULL;

	file_chooser = GTK_FILE_CHOOSER (context->custom_file_chooser);

	if (g_value_get_boolean (source_value))
		file = gtk_file_chooser_get_file (file_chooser);

	g_value_take_object (target_value, file);

	return TRUE;
}

static gboolean
cal_config_local_custom_file_to_active (GBinding *binding,
                                        const GValue *source_value,
                                        GValue *target_value,
                                        gpointer user_data)
{
	Context *context = user_data;
	GtkFileChooser *file_chooser;
	GFile *file;
	gboolean success;

	file_chooser = GTK_FILE_CHOOSER (context->custom_file_chooser);

	file = g_value_get_object (source_value);

	if (file == NULL) {
		g_value_set_boolean (target_value, FALSE);
		return TRUE;
	}

	success = gtk_file_chooser_set_file (file_chooser, file, NULL);
	g_value_set_boolean (target_value, success);

	return success;
}

static void
cal_config_local_file_set_cb (GtkFileChooserButton *button,
                              GtkWidget *custom_file_checkbox)
{
	/* This will update ESourceLocal:custom-file. */
	g_object_notify (G_OBJECT (custom_file_checkbox), "active");
}

static void
cal_config_local_insert_widgets (ESourceConfigBackend *backend,
                                 ESource *scratch_source)
{
	ESourceConfig *config;
	ESource *builtin_source;
	ESourceRegistry *registry;
	ESourceExtension *extension;
	GtkFileFilter *filter;
	GtkWidget *container;
	GtkWidget *widget;
	Context *context;
	gboolean source_is_builtin = FALSE;
	const gchar *extension_name;
	const gchar *uid;
	gchar *markup;

	uid = e_source_get_uid (scratch_source);
	config = e_source_config_backend_get_config (backend);
	registry = e_source_config_get_registry (config);

	/* The built-in sources can't use a custom file. */

	builtin_source = e_source_registry_ref_builtin_calendar (registry);
	source_is_builtin |= e_source_equal (scratch_source, builtin_source);
	g_object_unref (builtin_source);

	builtin_source = e_source_registry_ref_builtin_memo_list (registry);
	source_is_builtin |= e_source_equal (scratch_source, builtin_source);
	g_object_unref (builtin_source);

	builtin_source = e_source_registry_ref_builtin_task_list (registry);
	source_is_builtin |= e_source_equal (scratch_source, builtin_source);
	g_object_unref (builtin_source);

	extension_name = E_SOURCE_EXTENSION_LOCAL_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	if (!source_is_builtin) {
		context = g_slice_new (Context);

		g_object_set_data_full (
			G_OBJECT (backend), uid, context,
			(GDestroyNotify) cal_config_local_context_free);

		widget = gtk_check_button_new_with_label (
			_("Use an existing iCalendar (ics) file"));
		e_source_config_insert_widget (
			config, scratch_source, NULL, widget);
		context->custom_file_checkbox = g_object_ref (widget);
		gtk_widget_show (widget);

		g_signal_connect_swapped (
			widget, "toggled",
			G_CALLBACK (e_source_config_resize_window), config);

		container = e_source_config_get_page (config, scratch_source);

		markup = g_markup_printf_escaped ("<b>%s</b>", _("iCalendar File"));
		widget = gtk_label_new (markup);
		gtk_widget_set_margin_top (widget, 12);
		gtk_widget_set_margin_bottom (widget, 6);
		gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
		gtk_label_set_xalign (GTK_LABEL (widget), 0);
		gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
		gtk_widget_show (widget);
		g_free (markup);

		e_binding_bind_property (
			context->custom_file_checkbox, "active",
			widget, "visible",
			G_BINDING_SYNC_CREATE);

		filter = gtk_file_filter_new ();
		gtk_file_filter_add_mime_type (filter, "text/calendar");

		widget = gtk_file_chooser_button_new (
			_("Choose an iCalendar file"), GTK_FILE_CHOOSER_ACTION_OPEN);
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (widget), filter);
		e_source_config_insert_widget (
			config, scratch_source, _("File:"), widget);
		context->custom_file_chooser = g_object_ref (widget);
		gtk_widget_show (widget);

		g_signal_connect (
			widget, "file-set",
			G_CALLBACK (cal_config_local_file_set_cb),
			context->custom_file_checkbox);

		e_binding_bind_property (
			context->custom_file_checkbox, "active",
			widget, "visible",
			G_BINDING_SYNC_CREATE);

		widget = gtk_check_button_new_with_label (
			_("Allow Evolution to update the file"));
		e_source_config_insert_widget (
			config, scratch_source, NULL, widget);
		context->writable_checkbox = g_object_ref (widget);
		gtk_widget_show (widget);

		e_binding_bind_property (
			context->custom_file_checkbox, "active",
			widget, "visible",
			G_BINDING_SYNC_CREATE);

		e_binding_bind_property_full (
			extension, "custom-file",
			context->custom_file_checkbox, "active",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE,
			cal_config_local_custom_file_to_active,
			cal_config_local_active_to_custom_file,
			context, (GDestroyNotify) NULL);

		e_binding_bind_property (
			extension, "writable",
			context->writable_checkbox, "active",
			G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
	}

	widget = e_ellipsized_combo_box_text_new (TRUE);
	cal_config_local_fill_addresses (registry, GTK_COMBO_BOX_TEXT (widget));
	e_source_config_insert_widget (config, scratch_source, _("Email:"), widget);
	gtk_widget_show (widget);

	e_binding_bind_object_text_property (
		extension, "email-address",
		gtk_bin_get_child (GTK_BIN (widget)), "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
}

static gboolean
cal_config_local_check_complete (ESourceConfigBackend *backend,
                                 ESource *scratch_source)
{
	ESourceLocal *extension;
	GtkToggleButton *toggle_button;
	Context *context;
	GFile *file;
	const gchar *extension_name;
	const gchar *uid;
	gboolean active;

	uid = e_source_get_uid (scratch_source);
	context = g_object_get_data (G_OBJECT (backend), uid);

	/* This function might get called before we install a
	 * context for this ESource, in which case just return. */
	if (context == NULL)
		return FALSE;

	extension_name = E_SOURCE_EXTENSION_LOCAL_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	file = e_source_local_get_custom_file (extension);

	toggle_button = GTK_TOGGLE_BUTTON (context->custom_file_checkbox);
	active = gtk_toggle_button_get_active (toggle_button);

	/* If toggle button is active we need a valid file. */
	return !active || (file != NULL);
}

static void
e_cal_config_local_class_init (ESourceConfigBackendClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CAL_SOURCE_CONFIG;

	class->parent_uid = "local-stub";
	class->backend_name = "local";
	class->insert_widgets = cal_config_local_insert_widgets;
	class->check_complete = cal_config_local_check_complete;
}

static void
e_cal_config_local_class_finalize (ESourceConfigBackendClass *class)
{
}

static void
e_cal_config_local_init (ESourceConfigBackend *backend)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_cal_config_local_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
